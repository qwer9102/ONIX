#ifndef ONIX_IDE_H
#define ONIX_IDE_H

#include <onix/types.h>
#include <onix/mutex.h>

#define SECTOR_SIZE 512     // 扇区大小
//#define CD_SECTOR_SIZE 2048 // 光盘扇区大小

#define IDE_CTRL_NR 2 // 控制器数量，固定为 2
#define IDE_DISK_NR 2 // 每个控制器可挂磁盘数量，固定为 2


// IDE 磁盘
typedef struct ide_disk_t
{
    char name[8];                  // 磁盘名称
    struct ide_ctrl_t *ctrl;       // 这个磁盘是哪个控制器指针控制的
    u8 selector;                   // 选择主从磁盘以及模式
    bool master;                   // 是否是主盘
    u32 total_lba;                 //可用扇区数量
    u32 cylinders;                 //柱面数
    u32 heads;                     //磁头数
    u32 sectors;                   //扇区数
} ide_disk_t;

// IDE 控制器
typedef struct ide_ctrl_t
{
    char name[8];                  // 控制器名称
    lock_t lock;                   // 控制器锁
    u16 iobase;                    // IO 寄存器基址,加上offset可以访问到其他寄存器
    ide_disk_t disks[IDE_DISK_NR]; // 磁盘
    ide_disk_t *active;            // 当前选择的磁盘
    u32 control;                   //控制字节
    struct task_t *waiter;         // 等待控制器的进程
} ide_ctrl_t;

//硬盘同步PIO读写
int ide_pio_read(ide_disk_t *disk, void *buf, u8 count, idx_t lba);
int ide_pio_write(ide_disk_t *disk, void *buf, u8 count, idx_t lba);

#endif