#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/mutex.h>
#include <onix/printk.h>

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

void init_thread()
{
   // mutex_init(&mutex);
    //lock_init(&lock);
    set_interrupt_state(true);
    u32 counter = 0;

    char ch;
    while (true)
    {
        //mutex_lock(&mutex);
        //lock_acquire(&lock);
        //LOGK("init task %d...",counter++);
        //mutex_unlock(&mutex);
        //lock_release(&lock);
        //test();
        //sleep(500);

        bool intr = interrupt_disable();
        keyboard_read(&ch,1);  //进入阻塞恢复后从这里开始执行,不是从头开始执行
        printk("%c",ch);
        set_interrupt_state(intr);
    }
}

void test_thread()
{
    set_interrupt_state(true);
    u32 counter = 0;
    while (true)
    {
        //mutex_lock(&mutex);
        //lock_acquire(&lock);
        //LOGK("test task %d...",counter++);
        //mutex_unlock(&mutex);
        //lock_release(&lock);
        //test();
        sleep(709);
    }
}