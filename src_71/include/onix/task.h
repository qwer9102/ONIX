#ifndef ONIX_TASK_H
#define ONIX_TASK_H

#include <onix/types.h>
#include <onix/list.h>

//定义target()函数是一个无参数,无返回类型
typedef void  target_t();

#define KERNEL_USER 0
#define NORMAL_USER 1
#define TASK_NAME_LEN 16   //任务名字长度


typedef enum task_state_t
{
    TASK_INIT,     // 初始化
    TASK_RUNNING,  // 执行
    TASK_READY,    // 就绪
    TASK_BLOCKED,  // 阻塞
    TASK_SLEEPING, // 睡眠
    TASK_WAITING,  // 等待
    TASK_DIED,     // 死亡
} task_state_t;


//task的PCB,一般放在分配的一页内存的起始位置,任务切换时的寄存器信息是在是从页末尾位置开始保存的
typedef struct task_t
{
    u32 *stack;             //内核栈
    list_node_t node;       //任务阻塞节点
    task_state_t state;     //任务状态
    u32 priority;           //任务优先级
    u32 ticks;              //剩余时间片
    u32 jiffies;            //上次执行时全局时间片 
    char name[TASK_NAME_LEN];//任务名
    u32 uid;                //任务id
    u32 pid;                //任务id
    u32 ppid;               //父任务id
    u32 pde;                //页目录物理地址
    struct bitmap_t *vmap;  //虚拟内存位图
    u32 brk;                //进程堆内存最高地址
    int status;             //进程特殊状态
    u32 magic;              //内核魔术,用于检测栈溢出
}task_t;

//这个一般用于线程和进程之间的切换
typedef struct task_frame_t
{
    u32 edi;
    u32 esi;
    u32 ebx;
    u32 ebp;
    void (*eip)(void);
}task_frame_t;


// 中断帧,用户态到内核态的切换要求更高,需要保存的信息更多
typedef struct intr_frame_t
{
    u32 vector;

    u32 edi;
    u32 esi;
    u32 ebp;
    // 虽然 pushad 把 esp 也压入，但 esp 是不断变化的，所以会被 popad 忽略
    u32 esp_dummy;

    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;

    u32 gs;
    u32 fs;
    u32 es;
    u32 ds;

    u32 vector0;

    u32 error;

    u32 eip;
    u32 cs;
    u32 eflags;
    u32 esp;
    u32 ss;
} intr_frame_t;



void task_init();
task_t *running_task();
void schedule();

void task_exit(int status);
pid_t task_fork();

void task_yield();
void task_block(task_t *task,list_t *blist,task_state_t state);
void task_unblock(task_t *task);

void task_sleep(u32 ms);
void task_wakeup();

void task_to_user_mode(target_t target);



pid_t sys_getpid();
pid_t sys_getppid();

#endif