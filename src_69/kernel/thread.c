#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/mutex.h>
#include <onix/printk.h>
#include <onix/task.h>
#include <onix/stdio.h>
#include <onix/arena.h>


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


static void  user_init_thread()
{
    u32 count = 0;
    char ch;
    while(true)
    {
        //test();  
        //printf("task is in user mode %d\n",count++);
        //printk("task is in user mode %d\n",count++);
/*        BMB;
        char *ptr = (char *)0x900000;
        brk(ptr);
        BMB;
        ptr -= 0x1000;
        ptr[3] = 0xff;
        BMB;
        brk((char *)0x800000);
        BMB;
*/ 
        printf("init thread pid:%d ppid:%d count:%d\n",getpid(),getppid(),count++);
        sleep(1000);

    }
}


void init_thread()
{
    char temp[100];
    task_to_user_mode(user_init_thread);
}


void test_thread()
{
    set_interrupt_state(true);
    u32 counter = 0;
    while (true)
    { 
        printf("test thread pid:%d ppid:%d counter:%d\n",getpid(),getppid(),counter++);
        //LOGK("test task %d...",counter++);  
        //BMB;
        sleep(2000) ;
    }
}