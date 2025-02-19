#include <onix/task.h>
#include <onix/printk.h>
#include <onix/debug.h>

#define PAGE_SIZE 0X1000

extern void task_switch(task_t *next);

//定义任务a,b及其内存地址
task_t *task_a = (task_t *)0x1000;
task_t *task_b = (task_t *)0x2000;

//函数返回是保存在eax寄存器中
task_t* running_task()
{
    //AT&T汇编格式
    //page大小是0x1000,当前栈顶后面三位取000就是任务开始的入口,这里任务是设置从0x1000/0x2000开始的
    asm volatile(
        "movl %esp, %eax\n"
        "andl $0xfffff000,%eax\n");
}

void schedule()
{
    //获取当前任务
    task_t *current = running_task();
    //根据current设置next任务,    
    task_t *next = (current == task_a ? task_b :task_a); 
    //上下文切换到next任务
    task_switch(next);
}


u32 thread_a()
{
    while(true)
    {
        printk("A ");
        schedule();
    }
}

u32 thread_b()
{
    while(true)
    {
        printk("B ");
        schedule();
    }
}


//task是任务,
static void task_creat(task_t* task,target_t target)
{
    //stack指向栈顶,栈是高向低
    u32 stack = (u32)task + PAGE_SIZE;
    //栈顶保存几个寄存器(任务切换时实现方需要保存的寄存器)的值
    stack = stack - sizeof(task_frame_t);   
    task_frame_t *frame = (task_frame_t *)stack;
    frame->ebx = 0x11111111;
    frame->esi = 0x22222222;
    frame->edi = 0x33333333;
    frame->ebp = 0x44444444;
    frame->eip = (void *)target;
    
    task->stack = (u32*)stack;
}


void task_init()
{
    task_creat(task_a,thread_a);
    task_creat(task_b,thread_b);
    schedule();
}