#include <onix/buffer.h>
#include <onix/memory.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/device.h>
#include <onix/string.h>
#include <onix/task.h>

#define HASH_COUNT 31    //哈希表个数

//内存从8M开始的4M位置用于buffer,见视频内存分配管理图,buffer_t和buffer(data)是成对的
//KERNEL_BUFFER_MEM开始的8M位置是buffer_t结构体,里面的date指向的是buffer,buffer从这4M的末尾开始往8M方向分配
static buffer_t *buffer_start = (buffer_t*)KERNEL_BUFFER_MEM;
static u32 buffer_count = 0;   //已经分配的buffer个数
static buffer_t *buffer_ptr = (buffer_t *)KERNEL_BUFFER_MEM;
static void *buffer_data = (void*)(KERNEL_BUFFER_MEM + KERNEL_BUFFER_SIZE - BLOCK_SIZE);

static list_t free_list;    //空闲缓冲列表,保存使用过被释放的空闲块,链接的是buffer_t->rnode
static list_t wait_list;    //等待进程列表
static list_t hash_table[HASH_COUNT];  //缓存哈希表,数组里每个元素指向链表,链表的链接的是buffer_t结构体中的hnode

//哈希函数,根据设备号和块索引号,返回hash_table的index
u32 hash(dev_t dev, idx_t block)
{
    return (dev ^ block) % HASH_COUNT;  //异或运算,对称
}
 
//根据设备号和块号找到对应的buffer_t结构体指针
static buffer_t *get_from_hash_table(dev_t dev ,idx_t block)
{
    u32 idx = hash(dev,block);          //找到对应的哈希链表索引
    list_t *list  = &hash_table[idx];   //找到设备对应的哈希链表
    buffer_t *bf = NULL;

    //哈希链表的链接的是buffer_t的hnode
    for(list_node_t *node = list->head.next; node != &list->tail; node = node->next )
    {
        //ptr = hnode的buffer_t结构体的指针
        buffer_t *ptr = element_entry(buffer_t,hnode,node);
        //遍历找复合条件的buffer_t结构体指针
        if(ptr->dev == dev && ptr->block == block)
        {
            bf = ptr;
            break;    
        }
    }
    if(!bf)   //遍历完没有找到对应结构体buffer_t
    {
        return NULL;
    }
    //找到buffer_t结构体后,如果bf在空闲列表中,则移出
    if(list_search(&free_list,&bf->rnode))
    {
        list_remove(&bf->rnode);
    }
    return bf;
    
}

//将bf放到哈希表中
static void hash_locate(buffer_t *bf)
{
    u32 idx = hash(bf->dev,bf->block);
    list_t *list = &hash_table[idx]; 
    assert(!list_search(list,&bf->hnode));  //确保这个buffer_t没有在hash链表中,才可以加入
    list_push(list,&bf->hnode);
}

//将bf从哈希表中移出
static void hash_remove(buffer_t *bf)
{
    u32 idx = hash(bf->dev,bf->block);
    list_t *list = &hash_table[idx]; 
    assert(list_search(list,&bf->hnode));  //确保这个buffer_t在hash链表中才可以移出
    list_remove(&bf->hnode);
}


//分配一个新的buffer并初始化(分配一个buffer_t结构体,buffer_t->data就是需要的buffer)
static buffer_t *get_new_buffer()
{
    buffer_t *bf = NULL;
    //KERNEL_BUFFER_MEM开始的8M位置是buffer_t结构体,里面的date指向的是buffer,buffer是从12M的末尾开始往8M开始方向分配
    if((u32)buffer_ptr + sizeof(buffer_t) < (u32)buffer_data)
    {
        bf = buffer_ptr;
        bf->data = buffer_data;   //数据区指向buffer
        bf->dev = EOF;
        bf->block = 0;
        bf->count = 0;
        bf->dirty = false;
        bf->valid = false;
        lock_init(&bf->lock);
        buffer_count++;
        buffer_ptr++;
        buffer_data -= BLOCK_SIZE;
        LOGK("buffer count:%d",buffer_count);
    }
    return bf;
}

//获取空闲的buffer
static buffer_t *get_free_buffer()
{
    buffer_t *bf = NULL;
    while(true)
    {
        //如果内存够,直接分配即可
        bf = get_new_buffer();
        if(bf)
        {
            return bf;
        }
        //如果没有,从free_list分配,(这个free_list是回收的buffer,在brelse函数中回收的)   
        if(!list_empty(&free_list))
        {
            bf = element_entry(buffer_t ,rnode,list_popback(&free_list));
            hash_remove(bf);
            bf->valid = false;
            return bf;
        }     
        //如果都没有,那么加入等待队列,等待buffer释放
        task_block(running_task(),&wait_list,TASK_BLOCKED);
    }
}

//获得设备dev第block对应的缓冲
buffer_t *getblk(dev_t dev,idx_t block)
{
    buffer_t *bf = get_from_hash_table(dev,block);
    //已经有,直接返回
    if(bf)
        return bf;
    //如果没有需要分配并加入到哈希表中
    bf = get_free_buffer();
    assert(bf->count == 0);
    assert(bf->dirty == 0);

    bf->count = 1;
    bf->dev = dev;
    bf->block = block;
    hash_locate(bf);
    return bf;
}

//读取dev设备的block
buffer_t *bread(dev_t dev, idx_t block)
{
    buffer_t *bf = getblk(dev,block);
    assert(bf != NULL);
    //已经有读取到block中了
    if(bf->valid)
    {
        bf->count++;
        return bf;
    }
    //否则申请设备读
    device_request(bf->dev,bf->data,BLOCK_SECS,bf->block*BLOCK_SECS,0,REQ_READ);
    bf->dirty = false;
    bf->valid = true;
    
    return bf;
}

//写缓冲
void bwrite(buffer_t *bf)
{
    assert(bf);
    //dirty = 0情形,已经写过了???
    if(!bf->dirty)
        return ;
    //dirty = 1情形
    device_request(bf->dev,bf->data,BLOCK_SECS,bf->block*BLOCK_SECS,0,REQ_WRITE);
    bf->dirty = false;
    bf->valid = true;
}


//释放buffer
void brelse(buffer_t *bf)
{
    if(!bf)
        return;
    
    bf->count--;
    assert(bf->count >= 0);
    //count = 0,释放并加入到free_list中管理
    if(!bf->count)
    {
        if(bf->rnode.next)
        {
            list_remove(&bf->rnode);
        }
        //分配过的释放后加入到free_list中
        list_push(&free_list,&bf->rnode);
    }
    //检查是否写回
    if(bf->dirty)
    {
        bwrite(bf);
    }
    //检查wait_list队列,是否需要唤醒task
    if(!list_empty(&wait_list))
    {
        task_t *task = element_entry(task_t,node,list_popback(&wait_list));
        task_unblock(task);
    }
}


void buffer_init()
{
    LOGK("buffer_t size is %d ",sizeof(buffer_t));
    list_init(&free_list);
    list_init(&wait_list);

    for(size_t i = 0; i < HASH_COUNT; i++)
    {
        list_init(&hash_table[i]);
    }
}