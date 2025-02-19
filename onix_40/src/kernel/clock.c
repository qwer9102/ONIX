#include <onix/io.h>
#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/debug.h>

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
u32 jiffy = JIFFY;


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




void clock_handler(int vector)
{
    assert(vector == 0x20);
    send_eoi(vector);

    //检测是否beep
    if(jiffies % 200 == 0)
    {  start_beep();  }

    jiffies++;
    DEBUGK("clock jiffies %d ...",jiffies);

    stop_beep();
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