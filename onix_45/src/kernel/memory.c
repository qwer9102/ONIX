#include <onix/memory.h>
#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/stdlib.h>
#include <onix/string.h>
#include <onix/bitmap.h>

//#define LOGK(fmt,args...) DEBUGK(fmt,##args)

#define ZONE_VALID    1  //ards内存可用区域
#define ZONE_RESERVED 2  //ards 不可用区域

/*
//虚拟地址是32:10bit页目录+10bit页表(一页刚好1024个页表项)+12bit页内偏移(索引号)
//页目录项指向的是页表项的物理地址(页目录项和页表项都是保存在内存中),给个虚拟地址,要获得页表地址,是通过页目录项的index找到页表地址(页表保存在物理内存中)
//页表项的index指向虚拟地址映射到的实际物理页的index
*/

#define IDX(addr)  ((u32)addr >> 12)  //获取addr的页号索引(4K刚好12bit)
#define DIDX(addr) (((u32)addr >> 22) & 0x3ff) // 获取 addr 的页目录索引,高10bit
#define TIDX(addr) (((u32)addr >> 12) & 0x3ff) // 获取 addr 的页表索引,中间10bit
#define PAGE(idx)  ((u32)idx << 12)   //有idx页索引获取页开始的地址,索引找页表
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0) //是否有对齐
 
//页目录项和页表项映射位置,映射8M
#define KERNEL_PAGE_DIR   0X1000
//内核页表索引
static u32 KERNEL_PAGE_TABLE[] = {
    0X2000,0X3000,
};

#define KERNEL_MAP_BITS 0X4000

#define KERNEL_MEMORY_SIZE (0x100000 * sizeof(KERNEL_PAGE_TABLE)) //1M*8=8M

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
    u32 count ;
    ards_t *ptr;

    if(magic == ONIX_MAGIC)
    {
        count = *(u32*)addr;
        ptr = (ards_t*)(addr + 4);
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
static u8 *memory_map;       //数组,每个元素对应一个物理页(索引还是物理地址)
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


// 得到 cr3 寄存器
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

//初始化页表项,index是索引号
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
    BMB;
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
static page_entry_t* get_pte(u32 vaddr)
{
    return (page_entry_t*)(0xffc00000 | DIDX(vaddr) << 12);
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

