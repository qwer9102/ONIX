#include <onix/task.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/bitmap.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/memory.h>
#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/list.h>
#include <onix/global.h>
#include <onix/arena.h>

#define PAGE_SIZE 0X1000

extern bitmap_t kernel_map;
extern u32 volatile jiffies;
extern u32 jiffy;
extern tss_t tss;

extern void task_switch(task_t *next);

#define NR_TASK 64  //任务最多数量

static task_t *task_table[NR_TASK];
static list_t block_list;           //阻塞列表
static list_t sleep_list;           //睡眠列表
static task_t *idle_task;


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

    //找完一圈没有找到ready任务,就执行idle_task
    if(task == NULL && state == TASK_READY)
    {
        task = idle_task;
    }

    return task;
}

//交出cpu使用
void task_yield()
{
    schedule();
}

//任务阻塞
void task_block(task_t *task,list_t *blist,task_state_t state)
{
    assert(!get_interrupt_state());
    assert(task->node.next == NULL);
    assert(task->node.prev == NULL);

    if(blist == NULL)
    {
        blist = &block_list;
    }
    
    list_push(blist,&task->node);
    assert(state != TASK_READY || state != TASK_RUNNING);
    
    task->state = state;

    task_t *current = running_task();
    if(current == task)
    {
        schedule();
    }
}

//解除任务阻塞
void task_unblock(task_t *task)
{
    assert(!get_interrupt_state());
    
    list_remove(&task->node);
    
    assert(task->node.next == NULL);
    assert(task->node.prev == NULL);

    task->state = TASK_READY;

}

void task_sleep(u32 ms)
{
    assert(!get_interrupt_state(true));

    u32 ticks = ms / jiffy;  //需要多少个时间片
    ticks = ticks > 0 ?  ticks : 1;//最少一个时间片

    //记录目标任务的全局时间片,在那个时间片需要唤醒任务
    task_t *current = running_task();
    current->ticks = jiffies +ticks;

    //从睡眠中找到第一个比当前任务的时间点更晚的任务,进行插入排序
    list_t *list = &sleep_list;
    list_node_t *anchor = &list->tail;
    for(list_node_t *ptr = list->head.next; ptr != &list->tail; ptr = ptr->next)
    {
        //找到节点对应的task_t结构体地址
        task_t *task = element_entry(task_t,node,ptr);

        if(task->ticks > current->ticks)
        { 
            //目的是把node插入到anchor前面,如果找了一圈没有找到,那么node插入到最后,就是tail前面
            anchor = ptr; 
            break;
        }        
    }

    //确保当前节点没有加入到其他链表
    assert(current->node.next == NULL);
    assert(current->node.prev == NULL);

    //插入链表
    list_insert_before(anchor,&current->node);

    current->state = TASK_SLEEPING;

    schedule();
}

//唤醒任务
void task_wakeup()
{
    assert(!get_interrupt_state(true));

    list_t *list = &sleep_list;
    for(list_node_t *ptr = list->head.next; ptr != &list->tail; )
    {
        //找到节点对应的task_t结构体地址
        task_t *task = element_entry(task_t,node,ptr);
        if(task->ticks > jiffies)
        { 
            break;
        }
        ptr = ptr->next;
        task->ticks = 0;
        task_unblock(task);        
    }
}


void task_activate(task_t *task)
{
    assert(task->magic == ONIX_MAGIC);

    if(task->pde != get_cr3())
    {
        set_cr3(task->pde);   //页表切换到新的进程
    }

    if(task->uid != KERNEL_USER)
    {
        tss.esp0 = (u32)task + PAGE_SIZE;
    }
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

    task_activate(next);
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


//target是返回的函数入口,这个函数是保存上文信息,恢复下文信息在interrupt_exit
//调用该函数的地方不能有局部变量,调用前栈顶需要准备足够空间
void task_to_user_mode(target_t target)
{
    task_t *task = running_task();

    //创建用户进程虚拟内存位图
    //用户进程的bitmap不能和内核bitmap公用,这里重新设置用户进程的bitmap
    task->vmap = kmalloc(sizeof(bitmap_t)); //位图的内容
    void *buf = (void*)alloc_kpage(1);  //用一页存储位图,这里最大只能表示128M内存空间     
    bitmap_init(task->vmap,buf,PAGE_SIZE,KERNEL_MEMORY_SIZE/PAGE_SIZE);

    //创建用户进程页表,每个进程的页表不一样
    task->pde = (u32)copy_pde();
    set_cr3(task->pde);

    u32 addr = (u32)task +PAGE_SIZE;

    addr -= sizeof(intr_frame_t);
    intr_frame_t *iframe = (intr_frame_t*)(addr);

    iframe->vector = 0x20;
    iframe->edi = 1;
    iframe->esi = 2;
    iframe->ebp = 3;
    iframe->esp_dummy = 4;
    iframe->ebx = 5;
    iframe->edx = 6;
    iframe->ecx = 7;
    iframe->eax = 8;

    iframe->gs = 0;
    iframe->ds = USER_DATA_SELECTOR;
    iframe->es = USER_DATA_SELECTOR;
    iframe->fs = USER_DATA_SELECTOR;
    iframe->ss = USER_DATA_SELECTOR;
    iframe->cs = USER_CODE_SELECTOR;

    iframe->error = ONIX_MAGIC;

    //u32 stack3 = alloc_kpage(1);
    iframe->eip = (u32)target;
    iframe->eflags = (0 << 12 | 0b10 | 1 << 9);
    //iframe->esp = stack3 + PAGE_SIZE;
    iframe->esp = USER_STACK_TOP;

    asm volatile(
        "movl %0 ,%%esp\n"    //把iframe地址给到esp,然后跳转到interrupt_exit
        "jmp interrupt_exit\n" ::"m"(iframe) );  
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

/*
u32  thread_a()
{
    set_interrupt_state(true);
    while(true)
    {
        printk("A ");
        //schedule();  //改进:手动调度改成时钟中断调度
        //yield();
        test();
    }
}

u32  thread_b()
{
    set_interrupt_state(true);
    while(true)
    {
        printk("B ");
        test();
    }
}

u32  thread_c()
{
    set_interrupt_state(true);
    while(true)
    {
        printk("C ");
        test();
    }
}
*/

extern void idle_thread();
extern void init_thread();
extern void test_thread();

void task_init()
{
    list_init(&block_list);
    list_init(&sleep_list);
 
    task_setup();

    
    idle_task = task_creat((void*)idle_thread,"idle",1,KERNEL_USER);
    task_creat((void*)init_thread,"init",5,NORMAL_USER);
    task_creat((void*)test_thread,"test",5,KERNEL_USER);
    //task_creat(thread_a,"a",5,KERNEL_USER);
    //task_creat(thread_b,"b",5,KERNEL_USER);
    //task_creat(thread_c,"c",5,KERNEL_USER);
    //schedule(); 不用手动调度,时钟中断处理函数会调度
}