#include <onix/ide.h>
#include <onix/io.h>
#include <onix/printk.h>
#include <onix/stdio.h>
#include <onix/memory.h>
#include <onix/interrupt.h>
#include <onix/task.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/device.h>

// IDE 寄存器基址
#define IDE_IOBASE_PRIMARY 0x1F0   // 主通道基地址
#define IDE_IOBASE_SECONDARY 0x170 // 从通道基地址

// IDE 寄存器偏移
#define IDE_DATA 0x0000       // 数据寄存器
#define IDE_ERR 0x0001        // 错误寄存器
#define IDE_FEATURE 0x0001    // 功能寄存器
#define IDE_SECTOR 0x0002     // 扇区数量
#define IDE_LBA_LOW 0x0003    // LBA 低字节
//#define IDE_CHS_SECTOR 0x0003 // CHS 扇区位置
#define IDE_LBA_MID 0x0004    // LBA 中字节
//#define IDE_CHS_CYL 0x0004    // CHS 柱面低字节
#define IDE_LBA_HIGH 0x0005   // LBA 高字节
//#define IDE_CHS_CYH 0x0005    // CHS 柱面高字节
#define IDE_HDDEVSEL 0x0006   // 磁盘选择寄存器
#define IDE_STATUS 0x0007     // 状态寄存器
#define IDE_COMMAND 0x0007    // 命令寄存器
#define IDE_ALT_STATUS 0x0206 // 备用状态寄存器
#define IDE_CONTROL 0x0206    // 设备控制寄存器
#define IDE_DEVCTRL 0x0206    // 驱动器地址寄存器

// IDE 命令
#define IDE_CMD_READ 0x20       // 读命令
#define IDE_CMD_WRITE 0x30      // 写命令
#define IDE_CMD_IDENTIFY 0xEC   // 识别命令


// IDE 控制器状态寄存器
#define IDE_SR_NULL 0x00 // NULL
#define IDE_SR_ERR 0x01  // Error
#define IDE_SR_IDX 0x02  // Index
#define IDE_SR_CORR 0x04 // Corrected data
#define IDE_SR_DRQ 0x08  // Data request
#define IDE_SR_DSC 0x10  // Drive seek complete
#define IDE_SR_DWF 0x20  // Drive write fault
#define IDE_SR_DRDY 0x40 // Drive ready
#define IDE_SR_BSY 0x80  // Controller busy

// IDE 控制寄存器
#define IDE_CTRL_HD15 0x00 // Use 4 bits for head (not used, was 0x08)
#define IDE_CTRL_SRST 0x04 // Soft reset
#define IDE_CTRL_NIEN 0x02 // Disable interrupts

// IDE 错误寄存器
#define IDE_ER_AMNF 0x01  // Address mark not found
#define IDE_ER_TK0NF 0x02 // Track 0 not found
#define IDE_ER_ABRT 0x04  // Abort
#define IDE_ER_MCR 0x08   // Media change requested
#define IDE_ER_IDNF 0x10  // Sector id not found
#define IDE_ER_MC 0x20    // Media change
#define IDE_ER_UNC 0x40   // Uncorrectable data error
#define IDE_ER_BBK 0x80   // Bad block

#define IDE_LBA_MASTER 0b11100000 // 主盘 LBA
#define IDE_LBA_SLAVE 0b11110000  // 从盘 LBA

typedef enum PART_FS
{
    PART_FS_FAT12 = 1,       //FAT12
    PART_FS_EXTENDED = 5,    //扩展分区
    PART_FS_MINIX = 0x80,    //minux
    PART_FS_LINUX = 0x83,    //linux
}PART_FS;

typedef struct ide_params_t
{
    u16 config;                 // 0 General configuration bits
    u16 cylinders;              // 01 cylinders
    u16 RESERVED;               // 02
    u16 heads;                  // 03 heads
    u16 RESERVED[5 - 3];        // 05
    u16 sectors;                // 06 sectors per track
    u16 RESERVED[9 - 6];        // 09
    u8 serial[20];              // 10 ~ 19 序列号
    u16 RESERVED[22 - 19];      // 10 ~ 22
    u8 firmware[8];             // 23 ~ 26 固件版本
    u8 model[40];               // 27 ~ 46 模型数
    u8 drq_sectors;             // 47 扇区数量
    u8 RESERVED[3];             // 48
    u16 capabilities;           // 49 能力
    u16 RESERVED[59 - 49];      // 50 ~ 59
    u32 total_lba;              // 60 ~ 61
    u16 RESERVED;               // 62
    u16 mdma_mode;              // 63
    u8 RESERVED;                // 64
    u8 pio_mode;                // 64
    u16 RESERVED[79 - 64];      // 65 ~ 79 参见 ATA specification
    u16 major_version;          // 80 主版本
    u16 minor_version;          // 81 副版本
    u16 commmand_sets[87 - 81]; // 82 ~ 87 支持的命令集
    u16 RESERVED[118 - 87];     // 88 ~ 118
    u16 support_settings;       // 119
    u16 enable_settings;        // 120
    u16 RESERVED[221 - 120];    // 221
    u16 transport_major;        // 222
    u16 transport_minor;        // 223
    u16 RESERVED[254 - 223];    // 254
    u16 integrity;              // 校验和
} _packed ide_params_t;

ide_ctrl_t controllers[IDE_CTRL_NR];


//磁盘中断处理函数
static void ide_handler(int vector)
{
    send_eoi(vector);  
    //得到对应的磁盘控制器
    ide_ctrl_t *ctrl = &controllers[vector - IRQ_HARDDISK - 0x20];

    //读取磁盘的状态寄存器
    u8 state = inb(ctrl->iobase + IDE_STATUS);
    LOGK("harddisk interrupt vector:%d state:0x%x",vector,state);
     //如果有进程阻塞,则取消阻塞
    if(ctrl->waiter)
    {  
        task_unblock(ctrl->waiter);
        ctrl->waiter = NULL;
    }
}


//错误检测
static u32 ide_error(ide_ctrl_t *ctrl)
{
    u8 error = inb(ctrl->iobase + IDE_ERR);
    if(error & IDE_ER_BBK)
        LOGK("bad block");
    if(error & IDE_ER_UNC)
        LOGK("uncorrectable data");
    if(error & IDE_ER_MC)
        LOGK("media change");
    if(error & IDE_ER_IDNF)
        LOGK("id not found");
    if(error & IDE_ER_MCR)
        LOGK("media change request");
    if(error & IDE_ER_ABRT)
        LOGK("abort");
    if(error & IDE_ER_TK0NF)
        LOGK("trace 0 not found");
    if(error & IDE_ER_AMNF)
        LOGK("address mark not found");
}

//等待就绪
static u32 ide_busy_wait(ide_ctrl_t *ctrl,u8 mask)
{
    while(true)
    {
        u8 state = inb(ctrl->iobase + IDE_ALT_STATUS);
        if(state & IDE_SR_ERR)   //有错误
        {
            ide_error(ctrl);
        }
        if(state & IDE_SR_BSY)  //驱动忙则继续等待,后面使用中断更好
        {
            continue;
        }
        if((state & mask) == mask)   //等待就绪了
            return 0;
    }
}

// 重置硬盘控制器
static void ide_reset_controller(ide_ctrl_t *ctrl)
{
    outb(ctrl->iobase + IDE_CONTROL, IDE_CTRL_SRST);
    ide_busy_wait(ctrl, IDE_SR_NULL);
    outb(ctrl->iobase + IDE_CONTROL, ctrl->control);
    ide_busy_wait(ctrl, IDE_SR_NULL);
}

//选择磁盘
static void ide_select_drive(ide_disk_t *disk)
{
    outb(disk->ctrl->iobase + IDE_HDDEVSEL,disk->selector);
    disk->ctrl->active = disk;
}

//选择扇区
static void ide_selector_sector(ide_disk_t *disk, u32 lba, u8 count)
{
    //输出功能,可省略
    outb(disk->ctrl->iobase + IDE_FEATURE,0);
    //读写扇区的数量,和boot.asm读写的c版本
    outb(disk->ctrl->iobase + IDE_SECTOR,count);
    //LBA低字节
    outb(disk->ctrl->iobase + IDE_LBA_LOW,(lba & 0xff));
    //LBA中字节
    outb(disk->ctrl->iobase + IDE_LBA_MID,((lba >> 8) & 0xff));
    //LBA高字节
    outb(disk->ctrl->iobase + IDE_LBA_HIGH,((lba >> 16) & 0xff));
    //LBA最高四位 + 磁盘选择
    outb(disk->ctrl->iobase + IDE_HDDEVSEL,((lba >> 24) & 0xf) | disk->selector);
    
    disk->ctrl->active = disk;
}


//从磁盘读一个扇区到buf
static void ide_pio_read_sector(ide_disk_t *disk,u16 *buf)
{
    for(size_t i = 0; i < (SECTOR_SIZE / 2); i++)
    {
        buf[i] = inw(disk->ctrl->iobase + IDE_DATA);
    }
}

//写一个扇区到磁盘
static void  ide_pio_write_sector(ide_disk_t *disk,u16 *buf)
{
    for(size_t i = 0; i < (SECTOR_SIZE / 2); i++)
    {
        outw(disk->ctrl->iobase + IDE_DATA,buf[i]);
    }
}

//磁盘控制
int ide_pio_ioctrl(ide_disk_t *disk,int cmd, void *args, int flags)
{
    switch(cmd)
    {
    case DEV_CMD_SECTOR_START:
        return 0;
    case DEV_CMD_SECTOR_COUNT:
        return disk->total_lba;
    default:
        panic("device command %d can't recognize !!!",cmd);
        break;
    }
}

//硬盘PIO方式读硬盘,从disk的第lba个扇区读取count个扇区到buf
int ide_pio_read(ide_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    assert(count > 0);
    assert(!get_interrupt_state());   //异步PIO时,这个函数不能中断

    ide_ctrl_t *ctrl = disk->ctrl;
    lock_acquire(&ctrl->lock);

    //选择磁盘
    ide_select_drive(disk);
    //等待就绪
    ide_busy_wait(ctrl,IDE_SR_DRDY);
    //选择扇区
    ide_selector_sector(disk,lba,count);
    //发送读命令
    outb(ctrl->iobase + IDE_COMMAND,IDE_CMD_READ);
    
    for(size_t i = 0; i < count; i++)
    {
        //异步方式,发送完读命令后将自己阻塞
        task_t *task = running_task();
        if(task->state == TASK_RUNNING)
        {
            //磁盘准备数据需要时间,这里阻塞自己,等中断来了查看是否准备好
            ctrl->waiter = task;
            task_block(task,NULL,TASK_BLOCKED);
        }
        //同步异步共用
        ide_busy_wait(ctrl,IDE_SR_DRQ);
        u32 offset = ((u32)buf + i*SECTOR_SIZE);
        ide_pio_read_sector(disk,(u16*)offset);
    }
    lock_release(&ctrl->lock);
    return 0;
}

//硬盘PIO方式写硬盘,从buf的count字节写到disk
int ide_pio_write(ide_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    assert(count > 0);
    assert(!get_interrupt_state());   //异步PIO时,这个函数不能中断
    
    ide_ctrl_t *ctrl = disk->ctrl;
    lock_acquire(&ctrl->lock);

    LOGK("write lba 0x%x",lba);

    //选择磁盘
    ide_select_drive(disk);
    //等待就绪
    ide_busy_wait(ctrl,IDE_SR_DRDY);
    //选择扇区
    ide_selector_sector(disk,lba,count);
    //发送写命令
    outb(ctrl->iobase + IDE_COMMAND,IDE_CMD_WRITE);
    
    for(size_t i = 0; i < count; i++)
    { 
        u32 offset = ((u32)buf + i*SECTOR_SIZE);
        ide_pio_write_sector(disk,(u16*)offset);

        //异步方式,发送完写命令后将自己阻塞
        task_t *task = running_task();
        if(task->state == TASK_RUNNING)
        {
            //阻塞自己等待中断的到来,并等待磁盘准备数据
            ctrl->waiter = task;
            task_block(task,NULL,TASK_BLOCKED);
        }
        LOGK("write sector wait 1s,pid %d",task->pid);
        task_sleep(100);
        ide_busy_wait(ctrl,IDE_SR_NULL);
    }
    lock_release(&ctrl->lock);
    return 0;
}

//分区控制
int ide_pio_part_ioctl(ide_part_t *part,int cmd, void *args, int flags)
{
    switch(cmd)
    {
    case DEV_CMD_SECTOR_START:
        return part->start;
    case DEV_CMD_SECTOR_COUNT:
        return part->count;
    default:
        panic("device command %d can't recognize !!!",cmd);
        break;
    }
}

// 读分区
int ide_pio_part_read(ide_part_t *part, void *buf, u8 count, idx_t lba)
{
    return ide_pio_read(part->disk, buf, count, part->start + lba);
}

// 写分区
int ide_pio_part_write(ide_part_t *part, void *buf, u8 count, idx_t lba)
{
    return ide_pio_write(part->disk, buf, count, part->start + lba);
}

static void ide_swap_pairs(char *buf,u32 len)
{
    for(size_t i = 0; i < len; i+=2)
    {
        register char ch = buf[i];
        buf[i] = buf [i + 1];
        buf[i + 1] = ch;
    }
    buf[len - 1] = '\0';
}

//识别ide磁盘
static u32 ide_identify(ide_disk_t *disk, u16 *buf)
{
    LOGK("identifing disk %s...", disk->name);
    lock_acquire(&disk->ctrl->lock);
    ide_select_drive(disk);

    outb(disk->ctrl->iobase + IDE_COMMAND, IDE_CMD_IDENTIFY);

    ide_busy_wait(disk->ctrl, IDE_SR_NULL);

    ide_params_t *params = (ide_params_t *)buf;

    ide_pio_read_sector(disk, buf);

    LOGK("disk %s total lba %d", disk->name, params->total_lba);

    u32 ret = EOF;
    if(params->total_lba == 0)
    {
        goto rollback;
    }

    ide_swap_pairs(params->serial,sizeof(params->serial));
    LOGK("disk %s serial number %s", disk->name, params->serial);

    ide_swap_pairs(params->firmware, sizeof(params->firmware));
    LOGK("disk %s firmware version %s", disk->name, params->firmware);

    ide_swap_pairs(params->model, sizeof(params->model));
    LOGK("disk %s model number %s", disk->name, params->model);

    disk->total_lba = params->total_lba;
    disk->cylinders = params->cylinders;
    disk->heads = params->heads;
    disk->sectors = params->sectors;
    ret = 0;

rollback:
    lock_release(&disk->ctrl->lock);
    return ret;
}

//ide磁盘分区初始化
static void ide_part_init(ide_disk_t *disk,u16 *buf)
{
    // 磁盘不可用
    if (!disk->total_lba)
        return;

    // 读取主引导扇区,buf是ide_ctrl_init里面申请的一页内存
    ide_pio_read(disk, buf, 1, 0);

    // 初始化主引导扇区
    boot_sector_t *boot = (boot_sector_t *)buf;

    for (size_t i = 0; i < IDE_PART_NR; i++)
    {
        part_entry_t *entry = &boot->entry[i];  
        ide_part_t *part = &disk->parts[i];
        if (!entry->count)
            continue;

        sprintf(part->name, "%s%d", disk->name, i + 1);

        LOGK("part %s ", part->name);
        LOGK("    bootable %d", entry->bootable);
        LOGK("    start %d", entry->start);
        LOGK("    count %d", entry->count);
        LOGK("    system 0x%x", entry->system);

        part->disk = disk;
        part->count = entry->count;
        part->system = entry->system;
        part->start = entry->start;

        //暂时不支持扩展分区
        if (entry->system == PART_FS_EXTENDED)
        {
            LOGK("Unsupported extended partition!!!");

            boot_sector_t *eboot = (boot_sector_t *)(buf + SECTOR_SIZE);
            ide_pio_read(disk, (void *)eboot, 1, entry->start);

            for (size_t j = 0; j < IDE_PART_NR; j++)
            {
                part_entry_t *eentry = &eboot->entry[j];
                if (!eentry->count)
                    continue;
                LOGK("part %d extend %d", i, j);
                LOGK("    bootable %d", eentry->bootable);
                LOGK("    start %d", eentry->start);
                LOGK("    count %d", eentry->count);
                LOGK("    system 0x%x", eentry->system);
            }
        }
    }
}

//初始化ide控制器,这里有2个控制器
static void ide_ctrl_init()
{
    u16 *buf = (u16*)alloc_kpage(1);   //用来读硬盘的属性
    for(size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++)
    {
        ide_ctrl_t *ctrl = &controllers[cidx];  //依次设置控制器
        sprintf(ctrl->name,"ide%u",cidx);  //ide0,ide1
        lock_init(&ctrl->lock);
        ctrl->active = NULL;
        ctrl->waiter = NULL;

        //设置当前控制器是主/从通道
        if(cidx)   //从通道,ide1是从通道
        {
            ctrl->iobase = IDE_IOBASE_SECONDARY;
        }
        else        //ide0是主通道 
        {
            ctrl->iobase = IDE_IOBASE_PRIMARY;
        }

        ctrl->control = inb(ctrl->iobase + IDE_CONTROL);

        //初始化控制器对应的2磁盘,
        //ide0->disk[0]设置为主盘,ide0->disk[1]设置为从盘,ide1->disk[0]设置为主盘,ide1->disk[1]设置为从盘,
        for(size_t didx = 0; didx < IDE_DISK_NR; didx++)
        {
            ide_disk_t *disk = &ctrl->disks[didx];
            sprintf(disk->name,"hd%c",'a' + cidx*2 + didx);  //hda/hdb
            disk->ctrl = ctrl;
            if(didx)   //从盘
            {
                disk->master = false;
                disk->selector = IDE_LBA_SLAVE;
            }
            else  //主盘
            {
                disk->master = true;
                disk->selector = IDE_LBA_MASTER;
            }
            ide_identify(disk,buf);
            ide_part_init(disk,buf);
        }
    }
    free_kpage((u32)buf,1);
}


static void ide_install()
{
    for(size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++)
    {
        ide_ctrl_t *ctrl = &controllers[cidx];
        for(size_t didx = 0; didx < IDE_DISK_NR; didx++)
        {
            ide_disk_t *disk = &ctrl->disks[didx];
            //磁盘是空,没有扇区
            if(! disk->total_lba)   
                continue;
            //磁盘有扇区,安装磁盘设备并返回设备号
            dev_t dev = device_install(
                DEV_BLOCK,DEV_IDE_DISK,disk,disk->name,0,
                ide_pio_ioctrl,ide_pio_read,ide_pio_write);
            //安装磁盘内的分区设备
            for(size_t i = 0; i < IDE_PART_NR; i++)
            {
                ide_part_t *part = &disk->parts[i];
                if(! part->count)
                    continue;
                device_install(
                    DEV_BLOCK,DEV_IDE_PART,part,part->name,dev,
                    ide_pio_part_ioctl, ide_pio_part_read, ide_pio_part_write);
            }
        }  
    } 
}


void ide_init()
{
    LOGK("ide init ...");
    ide_ctrl_init();
    
    ide_install();

    set_interrupt_handler(IRQ_HARDDISK,ide_handler);
    set_interrupt_handler(IRQ_HARDDISK2,ide_handler);
    set_interrupt_mask(IRQ_HARDDISK,true);
    set_interrupt_mask(IRQ_HARDDISK2,true);
    set_interrupt_mask(IRQ_CASCADE,true);   //打开中断级联


}