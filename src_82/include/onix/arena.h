#ifndef  ONIX_ARENA_H
#define  ONIX_ARENA_H


#include <onix/types.h>
#include <onix/list.h>

#define DESC_COUNT 7

typedef list_node_t block_t;   //内存block是list_node_t类型的


//一页的内存描述符,初始化时里面的block_size分成7个不同大小类型16字节--1024
typedef struct arena_descriptor_t
{
    u32 total_block;   //这个页的内存分成了多少块
    u32 block_size;    //这个页的块大小,分配按照块分配,不足也分一块
    int page_count;    //已经分配的同个block大小的页数(针对整个内存空间的页来说的)
    list_t free_list;  //这个在用的页内的空闲块列表
}arena_descriptor_t;


//一页或者多页内存
typedef struct arena_t
{
    arena_descriptor_t *desc;      
    u32 count;    //剩余的块数(块为单位分配)或者页数(页为单位分配),取决于large
    //large = true ,表示超过分配的大小超过1024字节,超过1024字节是按照页为单位分配的,不是块为单位
    //large = false,表示没有超过1024字节,按照块为单位进行分配
    u32 large;    
    u32 magic;
}arena_t;


void* kmalloc(size_t size);
void kfree(void *ptr);

#endif
