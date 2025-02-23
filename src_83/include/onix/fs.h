#ifndef ONIX_FS_H
#define ONIX_FS_H

#include <onix/types.h>
#include <onix/list.h>

#define BLOCK_SIZE 1024
#define SECTOR_SIZE 512

#define MINIX1_MAGIC 0x137F
#define NAME_LEN 14

#define IMAP_NR 8
#define ZMAP_NR 8

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

typedef struct super_desc_t
{
    u16 inodes;         //节点数 
    u16 zones;          //逻辑块数
    u16 imap_blocks;    //i节点位图所占用的数据块数
    u16 zmap_blocks;    //逻辑块位图所占用的数据块数
    u16 firstdatezone;  //第一个数据逻辑块号
    u16 log_zone_size;  //log2(每逻辑块数据块数)
    u32 max_size;       //文件最大长度
    u16 magic;          //文件系统魔术
} super_desc_t;

// 目录结构
typedef struct dentry_t
{
    u16 nr;    // inode
    char name[NAME_LEN]; //文件名
} dentry_t;



#endif