#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/mutex.h>

void idle_thread()
{
    set_interrupt_state(true);
    u32 count = 0;
    while (true)
    {
        LOGK("idle task..... %d",count++);
        asm volatile(
            "sti\n"     //开启中断
            "hlt\n"     //关闭cpu,进入暂停状态,等待外中断的到来
        );
        yield();        //放弃执行权,调度其他程序
    }
}


//mutex_t mutex;
lock_t lock;

void init_thread()
{
   // mutex_init(&mutex);
    lock_init(&lock);
    set_interrupt_state(true);
    u32 counter = 0;
    while (true)
    {
        //mutex_lock(&mutex);
        lock_acquire(&lock);
        LOGK("init task %d...",counter++);
        //mutex_unlock(&mutex);
        lock_release(&lock);
        //test();
        //sleep(500);
    }
}

void test_thread()
{
    set_interrupt_state(true);
    u32 counter = 0;
    while (true)
    {
        //mutex_lock(&mutex);
        lock_acquire(&lock);
        LOGK("test task %d...",counter++);
        //mutex_unlock(&mutex);
        lock_release(&lock);
        //test();
        //sleep(709);
    }
}