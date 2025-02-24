#ifndef ONIX_FS_H
#define ONIX_FS_H

#include <onix/types.h>
#include <onix/list.h>

#define BLOCK_SIZE 1024
#define SECTOR_SIZE 512

#define MINIX1_MAGIC 0x137F
#define NAME_LEN 14    //文件名长度

//inode位图占用的block数量,一个block有多个inode结构体,位图1bit对应1个结构体
#define IMAP_NR 8   
//数据块位图占用的block数量,1个bit对应1个数据块
#define ZMAP_NR 8

#define BLOCK_BITS (BLOCK_SIZE * 8)                          // 块位图大小

//保存在磁盘中的结构体
typedef struct inode_desc_t
{
    u16 mode;   //文件类型和属性
    u16 uid;
    u32 size;
    u32 mtime;
    u8 gid;
    u8 nlinks;
    u16 zone[9]; //直接(0-6),间接(7)或双重间接(8)逻辑块号 
} inode_desc_t;

//加载到内存的inode结构体
typedef struct inode_t
{

    inode_desc_t *desc; // inode 描述符
    struct buffer_t *buf; // inode 描述符对应 buffer
    dev_t dev;      // 设备号
    idx_t nr;       // i 节点号
    u32 count;      // 引用计数
    time_t atime;   // 访问时间
    time_t ctime;   // 创建时间
    list_node_t node;//链表节点
    dev_t mount;    // 安装设备

} inode_t;

//磁盘上的超级块
typedef struct super_desc_t
{
    u16 inodes;         //总的节点数 
    u16 zones;          //总的逻辑块数
    u16 imap_blocks;    //i节点位图所占用的数据块数
    u16 zmap_blocks;    //逻辑块位图所占用的数据块数
    u16 firstdatezone;  //第一个数据逻辑块号
    u16 log_zone_size;  //log2(每逻辑块数据块数)
    u32 max_size;       //文件最大长度
    u16 magic;          //文件系统魔术
} super_desc_t;

//加载到内存上的超级块
typedef struct super_block_t
{
    super_desc_t *desc;             // 磁盘分区的超级块描述符(就是第2个block的数据入口地址)
    struct buffer_t *buf;           // 超级块描述符 buffer
    struct buffer_t *imaps[IMAP_NR]; // inode位图 buffer
    struct buffer_t *zmaps[ZMAP_NR]; // 数据块位图 buffer
    dev_t dev;            // 设备号
    list_t inode_list;    // 使用中 inode 链表
    inode_t *iroot;       // 根目录 inode
    inode_t *imount;      // 安装到的 inode
} super_block_t;


// 目录结构
typedef struct dentry_t
{
    u16 nr;    // 指向这个数据块的inode结构体编号
    char name[NAME_LEN]; //文件名
} dentry_t;


super_block_t* get_super(dev_t dev);   //获得dev对应的超级快
super_block_t* read_super(dev_t dev);  //读取dev对应的超级快

idx_t balloc(dev_t dev);
idx_t bfree(dev_t dev,idx_t idx);
idx_t ialloc(dev_t dev);
idx_t ifree(dev_t dev,idx_t idx);

#endif