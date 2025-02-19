#include <onix/task.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/bitmap.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/memory.h>
#include <onix/interrupt.h>
#include <onix/syscall.h>

#define PAGE_SIZE 0X1000

extern bitmap_t kernel_map;
extern void task_switch(task_t *next);

#define NR_TASK 64  //任务最多数量
static task_t *task_table[NR_TASK];

//从task_table中获取一个空闲的任务(分配一页内存)
static task_t* get_free_task()
{
    for(size_t i = 0; i < NR_TASK; i++)
    {
        if(task_table[i] == NULL)
        {
            task_table[i] = (task_t*)alloc_kpage(1);
            return task_table[i];
        }
    }
    panic("No more tasks");
}


//从task_table中查找处于某种任务状态的任务(查找一个即可),自己除外
static task_t* task_search(task_state_t state)
{
    assert(!get_interrupt_state());  //确保是在关中断的情形下(原子操作)
    task_t *task = NULL;
    task_t *current = running_task();
    
    //开始遍历查找
    for(size_t i = 0; i < NR_TASK; i++)
    {
        task_t *ptr = task_table[i];
        //没有任务跳出查找下一个
        if(ptr == NULL)
            continue;
        //状态不相等跳出
        if(ptr->state != state)
            continue;
        //查找不包括当前任务
        if(ptr == current)
            continue;
        //避免hungry,选择tick更长的,jiffies更短的
        //疑问:这里task是谁??? 是current??
        if(task == NULL || task->ticks < ptr->ticks || ptr->jiffies < task->jiffies)
            task = ptr;
    }
    return task;
}

//交出cpu使用
void task_yield()
{
    schedule();
}

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
    assert(!get_interrupt_state());  //不可中断
    task_t *current = running_task();  
    //从ready状态中选择next任务
    task_t *next = task_search(TASK_READY); 
    
    assert(next != NULL);
    assert(next->magic == ONIX_MAGIC);
    
    //如果当前任取状态是running需要改成ready状态
    if(current->state == TASK_RUNNING)
    {
        current->state = TASK_READY;
    }
    
    if(!current->ticks)
    {
        current->ticks = current->priority;
    }
    //修改next任务的状态
    next->state = TASK_RUNNING;
    if(next == current)       //next任务还是自己,跳过上下文切换
        return;

    //上下文切换到next任务
    task_switch(next);
}


//task是任务,
static task_t* task_creat(target_t target,const char *name,u32 priority,u32 uid)
{
    //假设申请的内存区域是从地址 0x1000 到 0x1FFF，那么返回的地址通常是 0x1000,不是0x1FFF
    task_t *task = get_free_task();
    memset(task,0,PAGE_SIZE);

    //让stack指向栈顶,栈是高向低
    u32 stack = (u32)task + PAGE_SIZE;
    //栈顶保存几个寄存器(任务切换时实现方需要保存的寄存器)的值
    stack  -=  sizeof(task_frame_t);   
    task_frame_t *frame = (task_frame_t *)stack;
    frame->ebx = 0x11111111;
    frame->esi = 0x22222222;
    frame->edi = 0x33333333;
    frame->ebp = 0x44444444;
    frame->eip = (void *)target;

    strcpy( (char *)task->name, name);

    task->stack = (u32*)stack;
    task->priority = priority;
    task->ticks = task->priority;
    task->jiffies = 0;
    task->state = TASK_READY;
    task->uid = uid;
    task->pde = KERNEL_PAGE_DIR;
    task->vmap = &kernel_map;
    task->magic = ONIX_MAGIC;     //这个magic位置是在是从页地址+task_t,stack是往magic方向增长
    
    return task;
}

//第一个任务,进行task_table初始化
//在没有tasn_creat之前,程序执行的是loader的0X1000内存
static void task_setup()
{
    task_t *task = running_task();
    task->magic = ONIX_MAGIC;
    task->ticks = 1;

    memset(task_table,0,sizeof(task_table));
}


u32  thread_a()
{
    set_interrupt_state(true);
    while(true)
    {
        printk("A ");
        //schedule();  //改进:手动调度改成时钟中断调度
        yield();
    }
}

u32  thread_b()
{
    set_interrupt_state(true);
    while(true)
    {
        printk("B ");
        yield();
        
    }
}

u32  thread_c()
{
    set_interrupt_state(true);
    while(true)
    {
        printk("C ");
        yield();
    }
}



void task_init()
{
    task_setup();

    task_creat(thread_a,"a",5,KERNEL_USER);
    task_creat(thread_b,"b",5,KERNEL_USER);
    task_creat(thread_c,"c",5,KERNEL_USER);
    //schedule(); 不用手动调度,时钟中断处理函数会调度
}