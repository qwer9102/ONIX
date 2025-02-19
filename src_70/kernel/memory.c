#include <onix/memory.h>
#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/stdlib.h>
#include <onix/string.h>
#include <onix/bitmap.h>
#include <onix/multiboot2.h>
#include <onix/task.h>

//#define LOGK(fmt,args...) DEBUGK(fmt,##args)

#define ZONE_VALID    1  //ards内存可用区域
#define ZONE_RESERVED 2  //ards 不可用区域

/*
//虚拟地址是32:10bit页目录+10bit页表(一页刚好1024个页表项)+12bit页内偏移(索引号)
//页目录项指向的是页表项的物理地址(页目录项和页表项都是保存在内存中),给个虚拟地址,要获得页表地址,是通过页目录项的index找到页表地址(页表保存在物理内存中)
//映射操作:获取虚拟地址的页目录项,让页目录项指向一个物理页(table),这个就是虚拟地址的页表地址,
//再将这个页表项的index指向需要映射的物理内存的索引号
*/

#define IDX(addr)  ((u32)addr >> 12)  //获取addr的页号索引(4K刚好12bit)
#define DIDX(addr) (((u32)addr >> 22) & 0x3ff) // 获取 addr 的页目录索引,高10bit
#define TIDX(addr) (((u32)addr >> 12) & 0x3ff) // 获取 addr 的页表索引,中间10bit
#define PAGE(idx)  ((u32)idx << 12)   //有idx页索引获取页开始的地址,索引找页表
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0) //是否有对齐
 
#define PDE_MASK 0XFFC00000

//内核页表索引
static u32 KERNEL_PAGE_TABLE[] = {
    0X2000,
    0X3000,
};

#define KERNEL_MAP_BITS 0X4000

//#define KERNEL_MEMORY_SIZE (0x100000 * sizeof(KERNEL_PAGE_TABLE)) //1M*8=8M

bitmap_t kernel_map;

typedef struct ards_t 
{
    u64 base;
    u64 size;
    u32 type;
} _packed ards_t;

static u32 memory_base = 0;   //可用内存基地址,从1M开始(小于1M的那部分不算)
static u32 memory_size = 0;   
static u32 total_pages = 0;   //所有内存页数
static u32 free_pages  = 0;   

#define used_pages (total_pages - free_pages) //已用页数

//loader.asm中push进来的参数:ards_count(是检测的内存段的结构体指针数组)
void memory_init(u32 magic , u32 addr)
{
    u32 count  = 0;

    //如果是onix loader进入的内核
    if(magic == ONIX_MAGIC)
    {
        count = *(u32*)addr;
        ards_t *ptr = (ards_t*)(addr + 4);
        for(size_t i = 0; i < count; i++,ptr++)
        {
            LOGK("Memory base 0x%p size 0x%p type %d",
                  (u32)ptr->base,(u32)ptr->size,(u32)ptr->type);
            if(ptr->type == ZONE_VALID && ptr->size > memory_size)
            {
                memory_base = (u32)ptr->base;
                memory_size = (u32)ptr->size;
            }
        }
    }
    //从grub进入的内核
    else if(magic == MULTIBOOT2_MAGIC)
    {
        
        //addr是grub引导时ebx传进来的内容:包含 bootloader 存储 multiboot2 信息结构体的，32 位 物理地址
        //结构体第一个参数是结构体total_size,addr+8是tag内容
        u32 size = *(unsigned int*)addr;
        multi_tag_t *tag = (multi_tag_t*)(addr + 8);

        LOGK("Announced multiboot_infomation size 0x%x",size);
        while(tag->type != MULTIBOOT_TAG_TYPE_END)
        {
            if(tag->type == MULTIBOOT_TAG_TYPE_MMAP)
            {
                break;
            }
            //指向下一个tag,tag是需要对齐到8字节
            tag = (multi_tag_t *)((u32)tag + ((tag->size + 7)& ~7));
        }

        //找到MULTIBOOT_TAG_TYPE_MMAP结构体地址,在找到multi_mmap_entry_t地址
        //这里entry[]大致就是内存检测的ards的内容(loader.asm引导进来的ards_count[]的内容)
        multi_tag_mmap_t *mtag = (multi_tag_mmap_t*)tag;
        multi_mmap_entry_t *entry = mtag->entries;
        //依次打印每一个entry的信息(就是内存段的信息),再把memory起始地址指向最后可用的那一段
        while( (u32)entry < (u32)tag + tag->size ) 
        {
            LOGK("Memory base:0x%p size:0x%p type:%d",
                  (u32)entry->addr,(u32)entry->len,(u32)entry->type);
            if(entry->type == ZONE_VALID && entry->len > memory_size)
            {
                memory_base = (u32)entry->addr;
                memory_size = (u32)entry->len;
            }
            //指向下一个entry
            entry = (multi_mmap_entry_t*)((u32)entry + mtag->entry_size);
        }
    }
    else
    {
        panic("Memory init magic unknown 0x%p\n",magic);
    }

    LOGK("ARDS count  %d",count);
    LOGK("Memory base  0x%p",(u32)memory_base);
    LOGK("Memory size  0x%p",(u32)memory_size);

    assert(memory_base == MEMORY_BASE);  //内存开始的 位置是1M
    assert((memory_size & 0xfff) == 0);  //要求页对齐

    total_pages = IDX(memory_size) + IDX(MEMORY_BASE);  //最后页
    free_pages = IDX(memory_size);

    LOGK("Total pages : %d ",total_pages);
    LOGK("Free pages : %d ",free_pages);

    if (memory_size < KERNEL_MEMORY_SIZE)
    {
        panic("System memory is %dM too small, at least %dM needed",
              memory_size / MEMORY_BASE, KERNEL_MEMORY_SIZE / MEMORY_BASE);
    }
}

//得到内存大小后,开始进行分页,页大小4kB,每个物理页如果被使用,那么需要记录(用8个bit,就是最多被引用255次),
//一般在开头的物理页专门留出几页(memory_map_pages)用数组(memory_map)来专门记录页的引用数量
static u32 start_page = 0;   //除去memory_map_pages记录页后,实际可以使用的起始物理页索引号
static u8 *memory_map;       //数组,每个元素对应一个物理页的引用数(索引还是物理地址)
static u32 memory_map_pages;  //每个物理页需要一个memory_map,总共需要的页数

//初始化物理页,主要是初始化的memory_map_pages和memory_map数组
void memory_map_init()
{
    //初始化memory_base数组起始地址为mamory_base
    memory_map = (u8*)memory_base;
    //确定memory_map数组需要的页数
    memory_map_pages = div_round_up(total_pages,PAGE_SIZE);
    LOGK("Memory map page count : %d",memory_map_pages);

    free_pages -= memory_map_pages;
    //初始化memory_map数组全部为0
    memset( (void *)memory_map, 0, memory_map_pages* PAGE_SIZE);

    //实际开始使用的页号(除去1M + memory_map数组占用的)
    start_page = IDX(MEMORY_BASE) + memory_map_pages;
    for(size_t i = 0; i < start_page; i++)
    {
        //已经被memory_map占用的页设置为1
        memory_map[i] = 1;
    }

    LOGK("Total pages : %d, Free pages : %d",total_pages,free_pages);

    //初始化内核虚拟内存位图kernel_map,需要8位对齐
    u32 length = (IDX(KERNEL_MEMORY_SIZE) - IDX(MEMORY_BASE))/8;
    bitmap_init(&kernel_map, (u8*)KERNEL_MAP_BITS, length, IDX(MEMORY_BASE));
    bitmap_scan(&kernel_map,memory_map_pages);
}

//获取一页,并返回实际物理页地址,不是索引
static u32 get_page()
{
    for(size_t i = start_page; i < total_pages; i++)
    {
        //遍历找到第一个可以使用的页号
        if(!memory_map[i])
        {
            memory_map[i] = 1;
            free_pages--;
            assert(free_pages >= 0);
            //页号转换为对应的物理地址
            u32 page = ((u32)i) << 12;
            LOGK("Get page : 0x%p",page);
            return page;
        }
    }
    //遍历完没有找到空内存
    panic("Out of Memory !!!");
}

//释放一页物理内存
static void put_page(u32 addr)
{   
    //判断对齐判断地址是否有效
    ASSERT_PAGE(addr);
    //地址转换为页索引
    u32 idx = IDX(addr);
    assert(idx >= start_page && idx < total_pages);
    //保证有被引用
    assert(memory_map[idx] >= 1);
    //引用减一
    memory_map[idx]--;
    //引用如果为0了
    if(memory_map[idx] == 0)
    {
        free_pages++;
    }
    assert(free_pages > 0 && free_pages < total_pages);
    LOGK("Put page 0x%p",addr);
}


// 得到 cr2 寄存器,缺页异常时造成缺页的指令地址
u32  get_cr2()
{
    //将cr2的值移动到eax中,eax就是返回值使用的寄存器
    asm volatile("movl %cr2, %eax\n");
}


// 得到 cr3 寄存器的值,是页目录地址
u32  get_cr3()
{
    //将cr3的值移动到eax中,eax就是返回值使用的寄存器
    asm volatile("movl %cr3, %eax\n");
}

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(u32 pde)
{
    ASSERT_PAGE(pde);
    //::"a"(pde)是先将pde值移动到eax,在执行前面的语句movl
    asm volatile("movl %%eax, %%cr3\n" :: "a"(pde));
}

//将cr0的pe位置为1,表示开启分页
static _inline void enable_page()
{
    //PE位是在最高位bit31
    asm volatile(
        "movl %cr0, %eax\n"
        "orl $0x80000000,%eax\n"
        "movl %eax, %cr0\n");
}

//初始化页表项,index是索引号(在页表项新增一项,映射到第index页物理内存)
static void entry_init(page_entry_t *entry,u32 index)
{
    *(u32*)entry = 0;    //先全部位置为0
    entry->present = 1;
    entry->write = 1;
    entry->user = 1;
    entry->index = index;
}

//#define KERNEL_PAGE_DIR   0X200000
//#define KERNEL_PAGE_ENTRY 0X201000

//开启内存映射后访问超过映射位置会出现缺页异常,不开启不会
void mapping_init()
{
    //初始化页目录项内容为0
    page_entry_t *pde = (page_entry_t*)KERNEL_PAGE_DIR;
    memset(pde,0,PAGE_SIZE);
    
    idx_t index = 0;
    //didx < 2, 2个页表
    for(idx_t didx = 0; didx < (sizeof(KERNEL_PAGE_TABLE)/4); didx++)
    {
        //把一页页表内容初始化为0
        page_entry_t *pte = (page_entry_t*)KERNEL_PAGE_TABLE[didx];
        memset(pte,0,PAGE_SIZE);

        //依次更新页目录项指向页表项,一个页目录项指向一个页表起始位置
        page_entry_t  *dentry = &pde[didx];
        entry_init(dentry,IDX((u32)pte));

        //依次初始化4M页表项中的1024个页表项,设置属性,同时把memory_map[]数组置为1
        for(size_t tidx = 0; tidx < 1024; tidx++,index++)
        {
            //第0项不映射,防止空指针访问错误
            if(index == 0)
                continue;
            
            page_entry_t *tentry = &pte[tidx];
            entry_init(tentry,index);
            memory_map[index] = 1;
        }
    }
    // 将最后一个页表指向页目录自己，方便修改
    page_entry_t *entry = &pde[1023];
    entry_init(entry, IDX(KERNEL_PAGE_DIR));

    // 设置 cr3 寄存器
    set_cr3((u32)pde);
    //BMB;
    // 分页有效
    enable_page();
}

//获取页目录表地址
static page_entry_t* get_pde()
{
    return (page_entry_t*)(0xfffff000);
}

// 获取虚拟地址 vaddr 对应的页表
//addr先找到页目录地址,再将页目录项的index(指向页表项的物理地址)经过转化
static page_entry_t* get_pte(u32 vaddr,bool create)
{
    page_entry_t *pde = get_pde();
    u32 idx = DIDX(vaddr);
    page_entry_t *entry = &pde[idx];

    assert(create || (!create && entry->present));
    page_entry_t *table = (page_entry_t*)(PDE_MASK | (idx << 12));

    //需要加个判断,页目录是只有一页,页表之前只使用2页,只能记录8M的内存信息,
    //现在是在用户态,访问的内存更大,所以页表中可能不存在这个映射关系,需要判断是否需要添加一个页表
    if(!entry->present)
    {
        LOGK("Get and create page table entry for 0x%p",vaddr);
        u32 page = get_page();
        entry_init(entry ,IDX(page));
        memset(table,0,PAGE_SIZE);
    }
    return table;
    //return (page_entry_t*)(0xffc00000 | DIDX(vaddr) << 12);
}

// 刷新虚拟地址 vaddr 的 块表 TLB
void flush_tlb(u32 vaddr)
{
    asm volatile("invlpg (%0)" ::"r"(vaddr)
                 : "memory");
}

//从位图中扫描连续count个页,并返回首地址
static u32 scan_page(bitmap_t *map,u32 count)
{
    assert(count > 0);
    int32 index = bitmap_scan(map,count);
    if(index == EOF)
    {
        panic("Scan page fail !!! ");
    }

    u32 addr = PAGE(index);
    LOGK("Scan page 0x%p, count %d",addr,count);
    return addr;
}

//与scan_page相反,重置相应的页
//把从addr开始的连续count个页以及位图清0
static void reset_page(bitmap_t *map,u32 addr,u32 count)
{
    ASSERT_PAGE(addr);
    assert(count > 0);
    
    u32 index = IDX(addr);

    for(size_t i = 0; i < count; i++)
    {
        assert(bitmap_test(map,index + i));
        bitmap_set(map,index + i,0);
    }
}

//分配连续count个内核页面
u32 alloc_kpage(u32 count)
{
    assert(count > 0);
    u32 vaddr = scan_page(&kernel_map,count);
    LOGK("ALLOC kernel pages 0x%p ,count %d",vaddr,count);
    return vaddr;
}

//释放连续count个内核页面
void free_kpage(u32 vaddr,u32 count)
{
    ASSERT_PAGE(vaddr);
    assert(count > 0);
    reset_page(&kernel_map,vaddr,count);
    LOGK("FREE kernel pages 0x%p, count %d",vaddr,count);
}

//将vaddr映射物理内存
void link_page(u32 vaddr)
{
    ASSERT_PAGE(vaddr);
    page_entry_t *pte = get_pte(vaddr,true);  //找到虚拟地址对应页表开始地址
    page_entry_t *entry = &pte[TIDX(vaddr)];  //再根据pte[索引]找到页表项

    task_t *task = running_task();
    bitmap_t *map = task->vmap;
    u32 index = IDX(vaddr);   //页表项里面的index,就是映射到的物理内存索引

    //已经映射到内存中了,页面存在直接返回
    if(entry->present)
    {
        assert(bitmap_test(map,index));
        return;
    }
    //页面不存在
    assert(!bitmap_test(map,index));
    bitmap_set(map,index,true);   //标记已分配

    u32 paddr = get_page();
    entry_init(entry,IDX(paddr));
    flush_tlb(vaddr);

    LOGK("LINK from 0x%p to 0x%p",vaddr,paddr);

}

//取消vaddr对应的物理内存映射
void unlink_page(u32 vaddr)
{
    ASSERT_PAGE(vaddr);
    page_entry_t *pte = get_pte(vaddr,true);  //找到虚拟地址对应页表开始地址
    page_entry_t *entry = &pte[TIDX(vaddr)];  //再根据pte[索引]找到页表项

    task_t *task = running_task();
    bitmap_t *map = task->vmap;
    u32 index = IDX(vaddr);   //页表项里面的index,就是映射到的物理内存索引

    //本身没有映射
    if(!entry->present)
    {
        assert(!bitmap_test(map,index));  //没有映射对应的bit是0
        return;
    }
    //已经有映射
    assert(entry->present  &&  bitmap_test(map,index));

    //取消映射
    entry->present = false;
    bitmap_set(map,index,false);   //标记为未分配

    u32 paddr = PAGE(entry->index);

    LOGK("UNLINK from 0x%p to 0x%p",vaddr,paddr);
    put_page(paddr);        
    
    flush_tlb(vaddr);
}

//copy一页,返回物理地址,原视频在V70
static u32 copy_page(void *page)  
{
    u32 paddr = get_page();    //分配一页用来拷贝

    page_entry_t *entry = get_pte(0,false);
    entry_init(entry,IDX(paddr));
    memcpy((void*)0,(void*)page,PAGE_SIZE);

    entry->present = false;
    return paddr;
}

page_entry_t* copy_pde()
{
    task_t *task = running_task();
    page_entry_t *pde = (page_entry_t*)alloc_kpage(1);
    memcpy(pde,(void*)task->pde,PAGE_SIZE);    //两个页目录表,里面指向相同的页表

    //将最后一个页表指向页目录自己,方便修改
    page_entry_t *entry = &pde[1023];
    entry_init(entry,IDX(pde));

    page_entry_t *dentry;
    for(size_t didx = 2; didx < 1023; didx++)
    {
        dentry = &pde[didx];
        if(!dentry->present)   //等于0表示没有指向某个页表
            {  continue; }
        //页目录项有指向页表,找到对应的页表起始地址
        page_entry_t *pte = (page_entry_t *)(PDE_MASK | didx << 12);

        //这个循环将映射到的物理页修改为不可写,引用数++
        for(size_t tidx = 0; tidx < 1024; tidx++)
        {
            entry = &pte[tidx];   //entry指向了映射的物理页
            if(!entry->present)   //页表项没有映射到物理页         
                {continue;}
            //如果页表项有映射到物理页,将对应的物理页修改为不可写,引用数++
            assert(memory_map[entry->index] > 0);
            entry->write = false;   //引用大于1标记为不可写
            memory_map[entry->index]++;
            assert(memory_map[entry->index] < 255);  //物理页引用数最大255
        }
        u32 paddr = copy_page(pte);  //循环复制页目录项指向的页表页
        dentry->index = IDX(paddr);  //指向新复制的页表 
    }
    set_cr3(task->pde);

    return pde;
}

//这里要求传进来的地址是页对齐的,addr要求是页开始的位置
int32 sys_brk(void *addr)
{
    LOGK("task brk 0x%p",addr);
    u32 brk = (u32)addr;
    ASSERT_PAGE(brk);

    task_t *task = running_task(); 
    //只能是用户态程序调用
    assert(task->uid != KERNEL_USER);
    assert( (KERNEL_MEMORY_SIZE <= brk)  && (brk <= USER_STACK_BOTTOM) );

    //判断需要释放还是新增
    u32 old_brk = task->brk;
    if(old_brk > brk)
    {
        for(;brk < old_brk; brk += PAGE_SIZE)  //addr要求是页起始地址
        {
            unlink_page(brk);
        }
    }
    //如果要新增内存不够情形
    else if( IDX(brk - old_brk)  >  free_pages )
    {
        return -1;
    } 
    //如果新增直接赋值,没有映射是因为可以在缺页异常处理时进行映射
    task->brk = brk;
    return 0;

}


typedef struct page_error_code_t
{
    u8 present : 1;
    u8 write : 1;
    u8 user : 1;
    u8 reserved0 : 1;
    u8 fetch : 1;
    u8 protection : 1;
    u8 shadow : 1;
    u16 reserved1 : 8;
    u8 sgx : 1;
    u16 reserved2;
} _packed page_error_code_t;

void page_fault(
    u32 vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags)
{
    assert(vector == 0xe);
    u32 vaddr = get_cr2();
    LOGK("fault address 0x%p eip 0x%p", vaddr, eip);

    page_error_code_t *code = (page_error_code_t *)&error;
    task_t *task = running_task();
    
    //用户栈空间给了2M,分配超过2M就panic
    assert(KERNEL_MEMORY_SIZE <= vaddr && vaddr < USER_STACK_TOP);
    
    //缺页异常情形1:有映射到物理页,但是物理页只读,或者引用数大于1
    if(code->present)
    {
        assert(code->write);

        page_entry_t *pte = get_pte(vaddr,false);  //这个false表示确定有映射到物理页
        page_entry_t *entry = &pte[TIDX(vaddr)];   //entry指向具体页表项

        assert(entry->present);
        assert(memory_map[entry->index] > 0);  //物理页引用数
        //一般是引用数>1(先执行else),后面逐渐引用数减到1后,只读导致的缺页异常的处理
        if(memory_map[entry->index] == 1)
        {
            entry->write = true;
            LOGK("WRITE page for 0x%p",vaddr);
        }
        else
        {
            //复制物理页
            void *page = (void *)PAGE(IDX(vaddr));   //page=物理页起始地址
            u32 paddr = copy_page(page);             //复制物理页到paddr
            memory_map[entry->index]--;
            entry_init(entry,IDX(paddr));            //复制好的物理页加入到页表映射中
            flush_tlb(vaddr);
            LOGK("COPY page for 0x%p",vaddr);
        }
        return;
    }

    //缺页异常情形2:没有映射到物理页
    //vaddr < task->brk:分配堆内存,确保在可访问的堆范围内 //vaddr >= USER_STACK_BOTTOM分配栈内存
    if( (!code->present)   &&   ((vaddr < task->brk) || (vaddr >= USER_STACK_BOTTOM))  )
    {
        u32 page = PAGE(IDX(vaddr));
        link_page(page);
        //BMB;
        return;
    }
    panic("page fault!!!");
/*
    // 如果用户程序访问了不该访问的内存
    if (vaddr < USER_EXEC_ADDR || vaddr >= USER_STACK_TOP)
    {
        assert(task->uid);
        printk("Segmentation Fault!!!\n");
        task_exit(-1);
    }

    if (code->present)
    {
        assert(code->write);

        page_entry_t *entry = get_entry(vaddr, false);

        assert(entry->present);   // 目前写内存应该是存在的
        assert(!entry->shared);   // 共享内存页，不应该引发缺页
        assert(!entry->readonly); // 只读内存页，不应该被写

        // 页表写时拷贝
        copy_on_write(vaddr, 3);

        return;
    }
*/
}



//虚拟地址是32:10bit页目录+10bit页表(一页刚好1024个页表项)+12bit页内偏移(索引号).
//页目录项指向的是页表项的物理地址,给个虚拟地址,要获得页表地址,是通过页目录项找到页表地址.
//映射操作:获取虚拟地址的页目录项,让页目录项指向一个物理页(table),这个就是虚拟地址的页表地址,
//再将这个页表项的index指向需要映射的物理内存的索引号
/*
void memory_test()
{
   
    BMB;
    u32 vaddr = 0x4000000;
    u32 paddr = 0x1400000;
    u32 table = 0x900000; //实际物理内存地址

    page_entry_t *pde = get_pde();
    page_entry_t *dentry = &pde[DIDX(vaddr)];
    entry_init(dentry,IDX(table));
    
    page_entry_t *pte = get_pte(vaddr);
    page_entry_t *tentry = &pte[TIDX(vaddr)];

    entry_init(tentry,IDX(paddr));    //将虚拟4000000映射到物理1400000
    
    BMB;
    char *ptr = (char*)(0x4000000);
    ptr[0] = 'a';
    BMB;
    entry_init(tentry,IDX(0x1500000));  //将虚拟4000000映射到物理1500000
    flush_tlb(vaddr);
    BMB;
    ptr[2] = 'b';
    BMB;
   


    u32 *pages = (u32*)(0x200000);
    u32 count = 0x6fe;
    for(size_t i = 0; i < count; i++)
    {
        pages[i] = alloc_kpage(1);
        LOGK("0x%x",i);
    }
    for(size_t i = 0; i < count; i++)
    {
        free_kpage(pages[i],1);
    }

}
 */

