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

#define NR_TASKS 64  //任务最多数量

static task_t *task_table[NR_TASKS];
static list_t block_list;           //阻塞列表
static list_t sleep_list;           //睡眠列表
static task_t *idle_task;


//从task_table中获取一个空闲的任务(分配一页内存)
//task_table[64]总共64个进程,数组保存的是每个进程地址
static task_t* get_free_task()
{
    for(size_t i = 0; i < NR_TASKS; i++)
    {
        if(task_table[i] == NULL)
        {
            //task_table[i] = (task_t*)alloc_kpage(1);
            task_t *task = (task_t*)alloc_kpage(1);
            memset(task,0,PAGE_SIZE);
            task->pid = i;
            task_table[i] = task;
            return task;
        }
    }
    panic("No more tasks");
}

//获取进程的pid
pid_t sys_getpid()
{
    task_t *task = running_task();
    return task->pid;
}

//获取进程的ppid
pid_t sys_getppid()
{
    task_t *task = running_task();
    return task->ppid;
}
//从task_table中查找处于某种任务状态的任务(查找一个即可),自己除外
static task_t* task_search(task_state_t state)
{
    assert(!get_interrupt_state());  //确保是在关中断的情形下(原子操作)
    task_t *task = NULL;
    task_t *current = running_task();
    
    //开始遍历查找
    for(size_t i = 0; i < NR_TASKS; i++)
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

    list_insert_sort(&sleep_list,&current->node,element_node_offset(task_t ,node, ticks) );
/*
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
*/
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
    //memset(task,0,PAGE_SIZE);

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
    task->brk = KERNEL_MEMORY_SIZE;
    task->ipwd = get_root_inode();
    task->iroot = get_root_inode();
    
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

    //创建用户进程页表,每个进程的一个页表
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
    //esp指向没有映射的地址时,会触发缺页异常
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

extern void interrupt_exit();
//构建task任务的内核栈,task->stack
static void task_build_stack(task_t *task)
{
    u32 addr = (u32)task + PAGE_SIZE;
    addr -= sizeof(intr_frame_t);
    intr_frame_t *iframe = (intr_frame_t*)addr;
    iframe->eax = 0;        //子进程返回值是0

    addr -= sizeof(task_frame_t);
    task_frame_t *frame = (task_frame_t *)addr;

    frame->ebp = 0xaa55aa55;
    frame->ebx = 0xaa55aa55;
    frame->edi = 0xaa55aa55;
    frame->esi = 0xaa55aa55;

    frame->eip = interrupt_exit;   //利用task_switch创建
    task->stack = (u32 *)frame;
}


//复制进程
pid_t task_fork()
{
    task_t *task = running_task();
    //确保任务没有阻塞且正在执行
    assert(task->state ==TASK_RUNNING  &&  task->node.next == NULL &&  task->node.prev == NULL);

    //复制任务的PCB
    task_t *child = get_free_task();
    pid_t pid = child->pid;    //父进程需要返回子进程的pid值
    memcpy(child,task,PAGE_SIZE);   

    child->pid = pid;
    child->ppid = task->pid;
    child->ticks = child->priority;
    child->state = TASK_READY;

    //复制用户进程的虚拟内存位图
    child->vmap = kmalloc(sizeof(bitmap_t));
    memcpy(child->vmap,task->vmap,sizeof(bitmap_t));

    //复制虚拟位图缓存,
    void *buf= (void *)alloc_kpage(1);
    memcpy(buf,task->vmap->bits,PAGE_SIZE);
    child->vmap->bits = buf;

    //复制页目录,页表在调用函数里复制了,映射到的物理页需要改为只读,引用数++
    //映射到的物理页还没有复制,用到时通过缺页异常实现写时复制
    child->pde = (u32)copy_pde();

    //构造child内核栈
    task_build_stack(child);
    //schedule();

    return child->pid;   //父进程返回fork的子进程的pid
}


//进程退出,这里只是设置status和state状态,收拾是函数waitpid
void task_exit(int status)
{
    task_t *task = running_task();
    //确保任务没有阻塞且正在执行
    assert(task->state == TASK_RUNNING  &&  task->node.next == NULL &&  task->node.prev == NULL);

    task->state = TASK_DIED;
    task->status = status;

    //逆序释放fork()分配的内存
    free_pde();

    free_kpage((u32)task->vmap->bits,1);
    kfree(task->vmap);

    //父进程die,子进程托管给爷爷进程
    for(size_t i = 0; i < NR_TASKS; i++)
    {
        task_t *child = task_table[i];    
        if(!child)
            {continue;}
        if(child->ppid != task->pid)   //不是子进程
            {continue;}
        child->ppid = task->ppid;      //是子进程
    }
    LOGK("task 0x%p exit ...",task);

    //在waitpid函数中,pid进程如果还没有exit,会把父进程阻塞为TASK_WAITING,这里需要unblock父进程
    //(parent->waitpid == task->pid)这个是waitpid里面设置的
    task_t *parent = task_table[task->ppid];
    if( parent->state == TASK_WAITING &&  
            (parent->waitpid == -1) || (parent->waitpid == task->pid) )
    {
        task_unblock(parent);
    }

    schedule();
}

//给exit的进程收拾,pid是需要收拾的进程id,如果pid=-1,表示所有进程
pid_t task_waitpid(pid_t pid,int *status)
{
    task_t *task = running_task();
    task_t *child = NULL;

    while(true)
    {
        bool has_child = false;
        for(size_t i = 2; i < NR_TASKS; i++)
        {
            task_t *ptr = task_table[i];
            if(!ptr)
                { continue; }
            if(ptr->ppid != task->pid)  //不是子进程
                { continue; }
            if(pid != ptr->pid && (pid != -1))
                {continue;}
            
            //找到pid进程,并且state是TASK_DIED
            if(ptr->state == TASK_DIED)
            {
                child = ptr;   //child = 符合条件pid
                task_table[i] = NULL;
                goto rollback;
            }
            //这个标记是否遍历完,遍历完是true,没有遍历完task_table就找到pid是false
            has_child = true;
        }
        //遍历完没有找到pid这个进程,表示这个进程还没有exit,
        if(has_child)
        {
            task->waitpid = pid;   //设置pid的父进程task,waitpid
            task_block(task,NULL,TASK_WAITING); //阻塞pid的父进程
            continue;
        }
        break;
    }
    //没有找到符合条件的子进程
    return -1;

rollback:
    *status = child->status;   //status状态保存在数组中,这个status是exit时候设定的
    u32 ret = child->pid;      //waitpid函数返回值
    free_kpage((u32)child,1);  //回收内存
    return ret;
}



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
    //schedule(); 不用手动调度,时钟中断处理函数会调度
}