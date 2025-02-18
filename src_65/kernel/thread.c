#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/mutex.h>
#include <onix/printk.h>
#include <onix/task.h>
#include <onix/stdio.h>
#include <onix/arena.h>

//extern int printf(const char *fmt, ...);

void idle_thread()
{
    set_interrupt_state(true);
    u32 count = 0;
    while (true)
    {
        //LOGK("idle task..... %d",count++);
        asm volatile(
            "sti\n"     //开启中断
            "hlt\n"     //关闭cpu,进入暂停状态,等待外中断的到来
        );
        yield();        //放弃执行权,调度其他程序
    }
}


//mutex_t mutex;
lock_t lock;
extern u32 keyboard_read(char *buf,u32 count);


static void  real_init_thread()
{
    //BMB;
    u32 count = 0;
    char ch;
    while(true)
    {
        BMB;
        //asm volatile("in $0x92,%ax\n");
        //sleep(100);
        //printf("task is in user mode %d\n",count++);
        //printk("task is in user mode %d\n",count++);
    }
}


void init_thread()
{
    char temp[100];
    task_to_user_mode(real_init_thread);
   
}


void test_thread()
{
    set_interrupt_state(true);
    u32 counter = 0;
    while (true)
    {
        void *ptr = kmalloc(1200);
        LOGK("kmalloc 0x%p...",ptr);
        kfree(ptr);

        ptr = kmalloc(1024);
        LOGK("kmalloc 0x%p...",ptr);
        kfree(ptr);
        
        ptr = kmalloc(54);
        LOGK("kmalloc 0x%p...",ptr);
        kfree(ptr);

        sleep(5000) ;
    }
}