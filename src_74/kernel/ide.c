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

ide_ctrl_t controllers[IDE_CTRL_NR];

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

//从磁盘读一个扇区到buf
static void  ide_pio_read_sector(ide_disk_t *disk,u16 *buf)
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


//硬盘同步PIO方式读硬盘,从disk的第lba个扇区读取count个扇区到buf
int ide_pio_read(ide_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    assert(count > 0);
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
        ide_busy_wait(ctrl,IDE_SR_DRQ);
        u32 offset = ((u32)buf + i*SECTOR_SIZE);
        ide_pio_read_sector(disk,(u16*)offset);
    }
    lock_release(&ctrl->lock);
    return 0;
}

//硬盘同步PIO方式写硬盘,从buf的count字节写到disk
int ide_pio_write(ide_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    assert(count > 0);
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
        ide_busy_wait(ctrl,IDE_SR_NULL);
    }
    lock_release(&ctrl->lock);
    return 0;
}


//初始化ide控制器,这里有2个控制器
static void ide_ctrl_init()
{
    for(size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++)
    {
        ide_ctrl_t *ctrl = &controllers[cidx];  //依次设置控制器
        sprintf(ctrl->name,"ide%u",cidx);  //ide0,ide1
        lock_init(&ctrl->lock);
        ctrl->active = NULL;

        //设置当前控制器是主/从通道
        if(cidx)   //从通道,ide1是从通道
        {
            ctrl->iobase = IDE_IOBASE_SECONDARY;
        }
        else        //ide0是主通道 
        {
            ctrl->iobase = IDE_IOBASE_PRIMARY;
        }
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
        }
    }
}


void ide_init()
{
    LOGK("ide init ...");
    ide_ctrl_init();

    void *buf = (void*)alloc_kpage(1);
    BMB;
    LOGK("read buffer %x",buf);
    ide_pio_read(&controllers[0].disks[0],buf,1,0);
    BMB;
    memset(buf,0x5a,SECTOR_SIZE);
    ide_pio_write(&controllers[0].disks[0],buf,1,1); //buf写到第2个扇区

    free_kpage((u32)buf,1);
}