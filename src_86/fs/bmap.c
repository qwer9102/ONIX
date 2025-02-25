#include <onix/fs.h>
#include <onix/debug.h>
#include <onix/bitmap.h>
#include <onix/assert.h>
#include <onix/string.h>
#include <onix/buffer.h>


//分配一个数据块,先加载到内存中来操作(从位图中找到一个空闲位并分配)
idx_t balloc(dev_t dev)
{
    super_block_t *sb = get_super(dev);  //在super_init()函数初始化好了
    assert(sb);

    buffer_t *buf = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    for(size_t i = 0; i < ZMAP_NR; i++)
    {
        buf = sb->zmaps[i];
        assert(buf);
        //磁盘位图数据给到map
        bitmap_make(&map,buf->data,BLOCK_SIZE,i * BLOCK_BITS + sb->desc->firstdatezone - 1);
        bit = bitmap_scan(&map,1);
        //从位图中找到一个空闲位分配并置为1
        if(bit != EOF)
        {
            //有空闲位分配好后,则标记缓冲区脏,需要写回硬盘
            assert(bit < sb->desc->zones);
            buf->dirty = true;
            break;
        }    
    }
    bwrite(buf);
    return bit;
}

//释放一个数据块,先加载到内存中来操作
void bfree(dev_t dev,idx_t idx)
{
    super_block_t *sb = get_super(dev);  //在super_init()函数初始化好了
    assert(sb != NULL);
    assert(idx < sb->desc->zones);

    buffer_t *buf;
    bitmap_t map;  //临时位图,用来保存磁盘加载出来的位图,内存操作更快

    for(size_t i = 0; i < ZMAP_NR; i++)
    {
        //跳过开始的块
        if(idx > BLOCK_BITS * (i + 1))
        {
            continue;
        }

        buf = sb->zmaps[i];
        assert(buf);
        
        bitmap_make(&map,buf->data,BLOCK_SIZE,i * BLOCK_BITS + sb->desc->firstdatezone - 1);
        //设置对应的位为0
        assert(bitmap_test(&map,idx));
        bitmap_set(&map,idx,0);
       
        //标记缓冲区脏
        buf->dirty = true;
        break;
    }
    bwrite(buf);
}


//分配一个inode块,先加载到内存中来操作(从位图中找到一个空闲位并分配)
idx_t ialloc(dev_t dev)
{
    super_block_t *sb = get_super(dev);  //在super_init()函数初始化好了
    assert(sb);

    buffer_t *buf = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    for(size_t i = 0; i < IMAP_NR; i++)
    {
        buf = sb->imaps[i];
        assert(buf);
        //磁盘位图数据给到map
        bitmap_make(&map,buf->data,BLOCK_SIZE,i * BLOCK_BITS );
        bit = bitmap_scan(&map,1);
        //从位图中找到一个空闲位分配并置为1
        if(bit != EOF)
        {
            //有空闲位分配好后,则标记缓冲区脏,需要写回硬盘
            assert(bit < sb->desc->inodes);
            buf->dirty = true;
            break;
        }    
    }
    bwrite(buf);
    return bit;
}

//释放一个数据块,先加载到内存中来操作
void ifree(dev_t dev,idx_t idx)
{
    super_block_t *sb = get_super(dev);  //在super_init()函数初始化好了
    assert(sb != NULL);
    assert(idx < sb->desc->inodes);

    buffer_t *buf;
    bitmap_t map;  //临时位图,用来保存磁盘加载出来的位图,内存操作更快

    for(size_t i = 0; i < IMAP_NR; i++)
    {
        //跳过开始的块
        if(idx > BLOCK_BITS * (i + 1))
        {
            continue;
        }

        buf = sb->imaps[i];
        assert(buf);
        
        bitmap_make(&map,buf->data,BLOCK_SIZE,i * BLOCK_BITS );
        //设置对应的位为0
        assert(bitmap_test(&map,idx));
        bitmap_set(&map,idx,0);
       
        //标记缓冲区脏
        buf->dirty = true;
        break;
    }
    bwrite(buf);
}

//获取inode第block块的索引
//如果不存在且creat = true,则创建
idx_t bmap(inode_t *inode,idx_t block,bool create)
{
    // 确保 block 合法
    assert(block >= 0 && block < TOTAL_BLOCK);

    // 数组索引
    u16 index = block;

    //inode_t *inode = (inode_t *)inode->desc;

    // 数组
    u16 *array = inode->desc->zone;

    // 缓冲区
    buffer_t *buf = inode->buf;

    // 用于下面的 brelse，传入参数 inode 的 buf 不应该释放
    buf->count += 1;

    // 当前处理级别
    int level = 0;

    // 当前子级别块数量
    int divider = 1;

    // 直接块
    if (block < DIRECT_BLOCK)
    {
        goto reckon;
    }

    block -= DIRECT_BLOCK;

    if (block < INDIRECT1_BLOCK)
    {
        index = DIRECT_BLOCK;
        level = 1;
        divider = 1;
        goto reckon;
    }

    block -= INDIRECT1_BLOCK;
    assert(block < INDIRECT2_BLOCK);
    index = DIRECT_BLOCK + 1;
    level = 2;
    divider = BLOCK_INDEXES;

reckon:
    for (; level >= 0; level--)
    {
        // 如果不存在 且 create 则申请一块文件块
        if (!array[index] && create)
        {
            array[index] = balloc(inode->dev);
            buf->dirty = true;
        }

        brelse(buf);

        // 如果 level == 0 或者 索引不存在，直接返回
        if (level == 0 || !array[index])
        {
            return array[index];
        }

        // level 不为 0，处理下一级索引
        buf = bread(inode->dev, array[index]);
        index = block / divider;
        block = block % divider;
        divider /= BLOCK_INDEXES;
        array = (u16 *)buf->data;
    }
}
