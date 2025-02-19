#include <onix/memory.h>
#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/stdlib.h>
#include <onix/string.h>

#define LOGK(fmt,args...) DEBUGK(fmt,##args)

#define ZONE_VALID    1  //ards内存可用区域
#define ZONE_RESERVED 2  //ards 不可用区域

#define IDX(addr)  ((u32)addr >> 12)  //获取addr的页号索引(4K刚好12bit)
#define PAGE(idx)  ((u32)idx << 12)   //有idx页索引获取页开始的地址
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0) //是否有对齐

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

void memory_test()
{
    u32 pages[10];
    for(size_t i = 0; i < 10; i++)
    {
        pages[i] = get_page();
    }
    for(size_t i = 0; i < 10; i++)
    {
        put_page(pages[i]);
    }
}