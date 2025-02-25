#include <onix/fs.h>
#include <onix/assert.h>
#include <onix/list.h>
#include <onix/debug.h>
#include <onix/buffer.h>
#include <onix/arena.h>
#include <onix/bitmap.h>
#include <onix/string.h>
#include <onix/stdlib.h>
#include <onix/syscall.h>

#define INODE_NR 64

static inode_t inode_table[INODE_NR];

//在inode_table[]中分配一个新的inode(加载到内存的inode)
static inode_t *get_free_inode()
{
    for(size_t i = 0; i < INODE_NR; i++)
    {
        inode_t *inode = &inode_table[i];
        if(inode->dev == EOF)    //初始化是设置的EOF
        {
            return inode;
        }
    }
    panic("no more inode!!!");
}

//释放static super_block_t *get_free_inode()获取的inode
static void put_free_inode(inode_t *inode)
{
    assert(inode != inode_table);
    assert(inode->count == 0);
    inode->dev = EOF;
}

//获取inode_table[0]的根目录地址
inode_t *get_root_inode()
{
    return inode_table;
}

//计算第nr个的inode对应的磁盘分区的块号
static inline idx_t inode_block(super_block_t *sb,idx_t nr)
{
    //inode编号从磁盘分区的(2 + sb->desc->imap_blocks + sb->desc->zmap_blocks)开始第1个编号,
    //起始编号为1,一个block内有多个inode结构体,见视频磁盘块分布图
    return 2 + sb->desc->imap_blocks + sb->desc->zmap_blocks + ( (nr - 1) / BLOCK_INODES);
}

//从super_block_t中已有的队列中查找第nr个inode的结构体地址
static inode_t *find_inode(dev_t dev,idx_t nr)
{
    super_block_t *sb = get_super(dev);
    assert(sb);
    list_t *list = &sb->inode_list;

    for(list_node_t *node = list->head.next; node != &list->tail; node = node->next)
    {
        inode_t *inode = element_entry(inode_t,node,node);
        if(inode->nr == nr)
        {
            return inode;
        }
    }
    return NULL;
}


//获得设备第nr个inode
inode_t *iget(dev_t dev,idx_t nr)
{
    //先从打开的超级块的链表中已经存在的inode中查找,找到返回
    inode_t *inode = find_inode(dev,nr);
    if(inode)
    {
        inode->count++;
        inode->atime = time();

        return inode;
    }
    //没有找到,那么需要创建一个内存inode并加入到内存超级块的inode队列中,在从磁盘中读出block加载到buffer
    super_block_t *sb = get_super(dev);  //找到dev设备的super_block_t入口地址
    assert(sb);

    assert(nr < sb->desc->inodes);
    //分配一个内存inode_t
    inode = get_free_inode();
    inode->dev = dev;
    inode->nr = nr;
    inode->count = 1;
    //inode_t加入到超级块的inode队列中
    list_push(&sb->inode_list,&inode->node);  
    //从磁盘中读出加载到buffer
    idx_t block = inode_block(sb,inode->nr);
    buffer_t *buf = bread(inode->dev,block);

    inode->buf = buf;
    inode->desc = &((inode_desc_t*)buf->data)[(inode->nr - 1) / BLOCK_INODES];
    
    inode->ctime = inode->desc->mtime;
    inode->atime = time();

    return inode;
}


//释放加载到内存中的inode
void iput(inode_t *inode)
{
    //inode是空直接返回
    if(!inode)
    {
        return;
    }
    //如果不空
    inode->count--;
    if(inode->count)  //还有引用,直接返回不需要释放
    {
        return;
    }
    brelse(inode->buf);         //释放inode对应的buffer    
    list_remove(&inode->node);  //从super_block_t的队列中移出这个inode
    put_free_inode(inode);      //释放inode内存
}


//初始化inode_table
void inode_init()
{
    for(size_t i = 0; i < INODE_NR; i++)
    {
        inode_t *inode = &inode_table[i];
        inode->dev = EOF;
    }
}