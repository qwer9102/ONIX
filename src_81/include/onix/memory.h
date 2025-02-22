#ifndef ONIX_MEMORY_H
#define ONIX_MEMORY_H

#include <onix/types.h>

#define PAGE_SIZE 0X1000      //一页大小 4K
#define MEMORY_BASE 0X100000  //1M开始内存位置

// 内核占用的内存大小 16M
#define KERNEL_MEMORY_SIZE 0x1000000

//内核缓存地址
#define KERNEL_BUFFER_MEM 0x800000

//内核缓存大小
#define KERNEL_BUFFER_SIZE 0x400000

// 内存虚拟磁盘地址
#define KERNEL_RAMDISK_MEM (KERNEL_BUFFER_MEM + KERNEL_BUFFER_SIZE)

// 内存虚拟磁盘大小
#define KERNEL_RAMDISK_SIZE 0x400000

//用户栈顶地址 128M
#define USER_STACK_TOP     0X8000000

//定义用户栈最大2M
#define USER_STACK_SIZE 0X200000

//定义用户栈底128M-2M
#define USER_STACK_BOTTOM  (USER_STACK_TOP - USER_STACK_SIZE)

//页目录项和页表项映射位置,映射8M
#define KERNEL_PAGE_DIR   0X1000
/*//内核页表索引
static u32 KERNEL_PAGE_TABLE[] = {
    0X2000,
    0X3000,
};*/


//页表项结构体
typedef struct page_entry_t
{
    u8 present : 1;  // 是否映射在内存中
    u8 write : 1;    // 0 只读 1 可读可写
    u8 user : 1;     // 1 所有人 0 超级用户 DPL < 3
    u8 pwt : 1;      // page write through 1 直写模式，0 回写模式
    u8 pcd : 1;      // page cache disable 禁止该页缓冲
    u8 accessed : 1; // 被访问过，用于统计使用频率
    u8 dirty : 1;    // 脏页，表示该页缓冲被写过
    u8 pat : 1;      // page attribute table 页大小 4K/4M
    u8 global : 1;   // 全局，所有进程都用到了，该页不刷新缓冲
    u8 shared : 1;   // 共享内存页，与 CPU 无关
    u8 privat : 1;   // 私有内存页，与 CPU 无关
    u8 readonly : 1; // 只读内存页，与 CPU 无关
    u32 index : 20;  // 页索引
} _packed page_entry_t;

void memory_init(u32 magic , u32 addr);

void memory_map_init();
void mapping_init();
//void memory_test();

u32 get_cr2();            // 得到 cr2 寄存器,缺页异常时造成缺页的指令地址

u32 get_cr3();            // 得到 cr3 寄存器
void set_cr3(u32 pde);  // 设置 cr3 寄存器，参数是页目录的地址


u32 alloc_kpage(u32 count); //分配连续count个内核页面
void free_kpage(u32 vaddr,u32 count); //释放连续count个内核页面

void link_page(u32 vaddr);    //将vaddr映射物理内存
void unlink_page(u32 vaddr);  //取消vaddr对应的物理内存映射

//复制页目录
page_entry_t* copy_pde();
//释放页目录
void free_pde();

//系统调用:修改内存中堆内存的最大位置到addr
int32 sys_brk(void *addr);

#endif