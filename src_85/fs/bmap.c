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
idx_t bfree(dev_t dev,idx_t idx)
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
idx_t ifree(dev_t dev,idx_t idx)
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