#include <onix/fs.h>
#include <onix/buffer.h>
#include <onix/assert.h>
#include <onix/device.h>
#include <onix/string.h>
#include <onix/debug.h>

#define SUPER_NR 16      //最大支持16个文件系统挂载在文件系统里面
static super_block_t super_table[SUPER_NR]; //超级块表
static super_block_t *root;                 //根文件系统超级快


//在super_table[]中分配一个新的内存超级块
static super_block_t *get_free_super()
{
    for(size_t i = 0; i < SUPER_NR; i++)
    {
        super_block_t *sb = &super_table[i];
        if(sb->dev == EOF)    //初始化是设置的EOF
        {
            return sb;
        }
    }
    panic("no more super block");
}

//获得dev对应的内存super_table[]的地址
super_block_t* get_super(dev_t dev)
{
    for(size_t i = 0; i < SUPER_NR; i++)
    {
        super_block_t *sb = &super_table[i];
        if(sb->dev == dev)
        {
            return sb;
        }
    }
    return NULL;
}

//读取dev对应的磁盘分区超级块super_desc_t的数据到内存超级块super_block_t中(保存在super_table[]中), 
super_block_t* read_super(dev_t dev)
{
    //在super_table[]中如果已经分配有dev对应的超级块,直接返回
    super_block_t *sb = get_super(dev);
    if(sb)
    {
        return sb;
    }
    //没有分配新分配一个,并将dev对应的超级块内容读到super_table[]表中
    LOGK("Reading super block of device %d",dev);
    
    //在super_table[]中分配一个super_block_t
    sb = get_free_super();
    //读取设备对应的磁盘分区的超级块到内存buffer
    buffer_t *buf = bread(dev,1);

    sb->buf = buf;
    sb->desc = (super_desc_t *)buf->data;  //就是磁盘分区的第2块的地址入口
    sb->dev = dev;

    assert(sb->desc->magic == MINIX1_MAGIC);
    //位图数据清0
    memset(sb->imaps,0,sizeof(sb->imaps));
    memset(sb->zmaps,0,sizeof(sb->zmaps));
    
    //读取inode位图数据
    int idx= 2;  //inode位图是从第2块开始的,第0块是引导块,第1块是superblock
    for(int i = 0; i < sb->desc->imap_blocks; i++)
    {
        assert(i < IMAP_NR);
        if((sb->imaps[i] = bread(dev,idx)))
            idx++;
        else
            break; 
    }

    //读取数据块位图,imap读完idx会指向zmaps
    for(int i = 0; i < sb->desc->zmap_blocks; i++)
    {
        assert(i < ZMAP_NR);
        if((sb->zmaps[i] = bread(dev,idx)))
            idx++;
        else
            break; 
    }

    return sb;
}

//加载磁盘分区的超级块到super_table[]中
static void mount_root()
{
    LOGK("Mount root file system...");
    //主硬盘的第一个分区是根文件系统,找到磁盘分区设备
    device_t *device = device_find(DEV_IDE_PART,0);
    assert(device);
    //读根文件系统超级块
    root = read_super(device->dev);

    //找的第二个磁盘分区应该是从盘
    device = device_find(DEV_IDE_PART,1);
    assert(device);
    super_block_t *sb = read_super(device->dev);

    idx_t idx = ialloc(sb->dev);
    ifree(sb->dev,idx);

    idx = balloc(sb->dev);
    bfree(sb->dev,idx);    
}

//加载到内存中的超级块的初始化
void super_init()
{
    for(size_t i = 0; i < SUPER_NR; i++)
    {
        super_block_t *sb = &super_table[i];
        sb->dev = EOF;
        sb->desc = NULL;
        sb->buf  = NULL;
        sb->iroot  = NULL;
        sb->imount = NULL;
        list_init(&sb->inode_list);
    }
    mount_root();
}



/*
void super_init()
{
    //superblock位置和imap,zmap在硬盘中的位置分布见视频
    //硬盘分布依次是boot/superblock/imap/zmap/inode
    device_t *device = device_find(DEV_IDE_PART,0);
    assert(device);
    
    buffer_t *boot = bread(device->dev,0);
    buffer_t *super = bread(device->dev,1);

    super_desc_t * sb = (super_desc_t*)super->data;
    assert(sb->magic == MINIX1_MAGIC);

    buffer_t *imap = bread(device->dev,2);
    buffer_t *zmap = bread(device->dev,2 + sb->imap_blocks);
    //读取第一个inode
    buffer_t *buf1 = bread(device->dev,2 + sb->imap_blocks + sb->zmap_blocks);
    inode_desc_t *inode = (inode_desc_t *)buf1->data;

    buffer_t *buf2 = bread(device->dev,inode->zone[0]);

    dentry_t *dir = (dentry_t *)buf2->data;
    inode_desc_t *helloi = NULL;
    //目录数据块中有. .. d1,hello.txt,这4个的inode不同,dir->nr++是依次指向这些inode
    while(dir->nr)
    {
        LOGK("inode %04d, name %s",dir->nr,dir->name);
        if(!strcmp(dir->name ,"hello.txt"))
        {
            helloi = &((inode_desc_t *)buf1->data)[dir->nr - 1];
            strcpy(dir->name,"world.txt");
            buf2->dirty = true;
            bwrite(buf2);
        }
        dir++;
    }

    buffer_t *buf3 = bread(device->dev,helloi->zone[0]);
    LOGK("content %s",buf3->data);

    strcpy(buf3->data,"This is modefied content!!!!!\n");
    buf3->dirty = true;
    bwrite(buf3);
    
    helloi->size = strlen(buf3->data);
    buf1->dirty = true;
    bwrite(buf1);
}
*/
