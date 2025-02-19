#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/mutex.h>
#include <onix/printk.h>
#include <onix/task.h>
#include <onix/stdio.h>
#include <onix/arena.h>
#include <onix/stdlib.h>


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
    int status;
    while(true)
    {
        //test();  
        //printf("task is in user mode %d\n",count++);

        pid_t pid = fork();
        if(pid)
        {
            printf("fork after parent %d %d %d\n",pid,getpid(),getppid());
            //sleep(1000);
            pid_t child = waitpid(pid,&status);
            printf("wait pid %d status %d %d ",child,status,count++);
        }
        else
        {
            printf("fork after child %d %d %d\n",pid,getpid(),getppid());
            sleep(1000);
            exit(0);
        }
        //hang();
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
        //printf("test thread pid:%d ppid:%d counter:%d\n",getpid(),getppid(),counter++);
        //LOGK("test task %d...",counter++);  
        //BMB;
        sleep(5000) ;
    }
}