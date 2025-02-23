#include <onix/io.h>
#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/task.h>

//计数器需要的寄存器
#define PIT_CHAN0_REG 0X40
#define PIT_CHAN2_REG 0X42
#define PIT_CTRL_REG 0X43

#define HZ 100    //10ms
#define OSCILLATOR 1193182
#define CLOCK_COUNTER (OSCILLATOR / HZ)
#define JIFFY (1000 / HZ)

#define SPEAKER_REG 0x61
#define BEEP_HZ 440
#define BEEP_COUNTER (OSCILLATOR / BEEP_HZ)
#define BEEP_MS 100

u32 volatile jiffies = 0;   //时间片计数器
u32 jiffy = JIFFY;         //全局时间片变量


bool volatile beeping = 0;
void start_beep()
{
    if (!beeping)
    {
        outb(SPEAKER_REG, inb(SPEAKER_REG) | 0b11);
    }
    beeping = jiffies + 5;
}

void stop_beep()
{
    if (beeping && jiffies > beeping)
    {
        outb(SPEAKER_REG, inb(SPEAKER_REG) & 0xfc);
        beeping = 0;
    }
}

//
void clock_handler(int vector)
{
    assert(vector == 0x20);
    send_eoi(vector);
    stop_beep();
    
    task_wakeup();

    jiffies++;
    //DEBUGK("clock jiffies %d ...",jiffies);

    //时间片到了进行调度
    task_t *task = running_task();
    assert(task->magic == ONIX_MAGIC);  //确保没有栈溢出
    
    task->jiffies = jiffies;
    task->ticks--;
    if(!task->ticks)
    {
        schedule();
    }
}


extern u32 startup_time;
time_t sys_time()
{
    //启动时间+启动多少秒
    return startup_time + (jiffies * JIFFY)/1000;
}

void pit_init()
{
    //配置计数器0时钟
    outb(PIT_CTRL_REG,0b00110100);  //数据意义见md文件,这里设置先输出低字节在输出高字节
    outb(PIT_CHAN0_REG,CLOCK_COUNTER & 0xff);        //输出低字节数据到clock_clonter
    outb(PIT_CHAN0_REG,(CLOCK_COUNTER >> 8) & 0xff); //输出低字节数据到clock_clonter
    
    //配置计数器2蜂鸣器
    outb(PIT_CTRL_REG,0b10110110);  //数据意义见md文件,这里设置先输出低字节在输出高字节
    outb(PIT_CHAN2_REG,(u8)BEEP_COUNTER);       //输出低字节数据到clock_clonter
    outb(PIT_CHAN2_REG,(u8)(BEEP_COUNTER >> 8)); //输出低字节数据到clock_clonter
}

void clock_init()
{
    pit_init();
    set_interrupt_handler(IRQ_CLOCK,clock_handler);
    set_interrupt_mask(IRQ_CLOCK,true);
}