#include <onix/arena.h>
#include <onix/memory.h>
#include <onix/string.h>
#include <onix/stdlib.h>
#include <onix/assert.h>

#define BUF_COUNT 4    //堆内存缓存页数量

extern u32 free_pages;
static arena_descriptor_t descriptors[DESC_COUNT];


//一页内存的block_size可以是不同的,有些场合需要大点,有些场合小点,这里定义了7种大小的,一页内存需要多大的块大小选择对应的descriptor[index]
//deceriptors[0]描述的是一页内存,块大小是16字节,[1]描述的一页内存,快大小是32字节,最大block_size是1024字节
void arena_init()
{
    u32 block_size = 16;  //16个字节
    for (size_t i = 0; i < DESC_COUNT; i++)
    {
        arena_descriptor_t *desc = &descriptors[i];
        desc->block_size = block_size;
        //页开始的位置会存一个arena_t结构,记录页的情形
        desc->total_block = (PAGE_SIZE - sizeof(arena_t)) / block_size;
        desc->page_count = 0;
        list_init(&desc->free_list); 
        block_size <<= 1;  //block*=2
    }
}


//获得arena第idx块内存指针
static void* get_arena_block(arena_t *arena,u32 idx)
{
    assert(arena->desc->total_block > idx);
    void *addr = (void*)(arena + 1);           //addr是block开始的位置
    u32 gap = idx * (arena->desc->block_size);
    return addr +gap;
}

//由当前block的地址得出对应的arena_t地址(就是页开始的位置,页开始位置保存的就是arena_t结构体)
static arena_t* get_block_arena(block_t *block)
{
    return (arena_t *)( (u32)block & 0xfffff000 );
}


//分配size个字节的内存
void* kmalloc(size_t size)
{
    arena_descriptor_t *desc = NULL;
    arena_t *arena;
    block_t *block;
    char *addr;

    //申请的大于1024字节,以页为单位分配
    //这是避免内存碎片,更方便管理,内存对齐需要
    if(size > 1024)
    {
        u32 asize = size + sizeof(arena_t);   //总共需要的大小要加上arena_t这个管理员的结构体
        u32 count = div_round_up(asize,PAGE_SIZE);  //分配的页数,向上取整
        
        //分配多页也只需要一个arena_t管理者,不需要每页一个arena管理者
        arena = (arena_t*)alloc_kpage(count);   //设置arena管理员地址
        memset(arena,0,count * PAGE_SIZE);      //分配的空间初始化
        arena->large = true;                    //表示大于1
        arena->count = count;
        arena->desc = NULL;
        arena->magic = ONIX_MAGIC;

        //去除arena的大小,返回地址指向块开始的位置
        addr = (char *)( (u32)arena + sizeof(arena_t) );
        return addr;
    }

    //申请字节小于1024,以块为单位分配, /*例如分配24字节空间,优先找block大小为32字节的页*/
    //优先选择最合适的块大小来分配,尽量是只分配1个block,便于管理,内存碎片
    for(size_t i = 0; i < DESC_COUNT; i++)
    {
        desc = &descriptors[i];
        if(desc->block_size >= size)
            break;
    }

    assert(desc != NULL);
    //找到合适的块大小的页后,先看看是否有空闲的block
    //如果没有空闲的block,重新分配一页并分好块大小
    if(list_empty(&desc->free_list) )  //块大小为[i]的desc的free->list是空的
    {
        arena = (arena_t*)alloc_kpage(1);
        memset(arena,0,PAGE_SIZE);

        desc->page_count++;    //整个内存中的已经分配同样的block大小的页数
        arena->desc = desc;
        arena->large = false;
        arena->count = desc->total_block;
        arena->magic = ONIX_MAGIC;

        //页内存按块大小划分好,并把这些块的地址在free->list串联起来
        for(size_t i = 0; i < desc->total_block; i++)
        {
            block = get_arena_block(arena,i);
            assert(!list_search(&arena->desc->free_list,block));
            list_push(&arena->desc->free_list,block);
            assert(list_search(&arena->desc->free_list,block));
        }
    }
    //有空闲的block,desc->free->list不为空,从链表里面找出1个block进行分配
    block = list_pop(&desc->free_list);
    arena = get_block_arena(block);      //block对应的arena_t管理员结构体地址,页地址
    assert( (arena->magic == ONIX_MAGIC)  &&  !arena->large);

    memset(block,0,desc->block_size);
    arena->count--;   
    return block;

}

//假设ptr是从kmalloc分配得到的指针
void kfree(void *ptr)
{
    assert(ptr);
    block_t *block = (block_t *)ptr;
    arena_t *arena = get_block_arena(block);
    assert( arena->large == 1  || arena->large == 0 );
    assert(arena->magic == ONIX_MAGIC);

    //回收的是页
    if(arena->large)
    {
        free_kpage((u32)arena,arena->count);
        return;
    }

    list_push(&arena->desc->free_list,block);
    arena->count++;

    //回收完这页刚好空了,需要把这页free掉,对应kmalloc页满重新开一页的过程
    //if( (arena->count == arena->desc->total_block)  && (arena->desc->page_count) > BUF_COUNT)
    if( (arena->count == arena->desc->total_block))
    {
        //把这页已经划分好的block全部释放掉
        //对应kmalloc的划分block大小并加入free_list的过程,for(size_t i = 0; i < desc->total_block; i++)
        for(size_t i = 0; i < arena->desc->total_block; i++)
        {  
            block = get_arena_block(arena,i);
            assert(list_search(&arena->desc->free_list,block));
            list_remove(block);
            assert( !list_search(&arena->desc->free_list,block));
        }
        //arena->desc->page_count--;
        //assert(arena->desc->page_count >= BUF_COUNT);
        free_kpage( (u32)arena,1);
    }
}