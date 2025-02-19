/*
#include <onix/onix.h>
#include <onix/io.h>
#include <onix/string.h>
#include <onix/console.h>
#include <onix/stdarg.h>
#include <onix/printk.h>
#include <onix/assert.h>
#include <onix/global.h>
#include <onix/task.h>
#include <onix/stdlib.h>
#include <onix/time.h>
#include <onix/rtc.h>
*/
#include <onix/types.h>
#include <onix/memory.h>
#include <onix/debug.h>
#include <onix/bitmap.h>
#include <onix/interrupt.h>

extern void  clock_init();
extern void  time_init();
extern void  rtc_init();

extern void  console_init();
extern void  gdt_init();    
extern void  interrupt_init();  
extern void  set_alarm(u32 secs);
//extern void  memory_map_init();
extern void bitmap_tests();
extern void  hang();
extern void memory_test();


void intr_test()
{
    bool intr = interrupt_disable();
    set_interrupt_state(intr);
}

void kernel_init()
{

    //console_init();
    //gdt_init();    
    memory_map_init();
    mapping_init();
    interrupt_init();  
    //task_init();
    //clock_init();
    //time_init();
    //rtc_init();
    //BMB;
    //memory_test();
    //bitmap_tests();
   /*
    BMB;
    //开启内存映射后访问超过映射位置会出现缺页异常,不开启不会
    char *ptr = (char*)(0x100000*20);
    ptr[0] = 'a';
    //asm volatile("sti");
    */
    bool intr = interrupt_disable();
    set_interrupt_state(true);
    LOGK("%d",intr);
    LOGK("%d",get_interrupt_state());
    BMB;
    intr = interrupt_disable();
    BMB;
    set_interrupt_state(true);
    LOGK("%d",intr);
    LOGK("%d",get_interrupt_state());


    hang();

}