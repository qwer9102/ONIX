#include <onix/onix.h>
#include <onix/io.h>
#include <onix/string.h>
#include <onix/console.h>
#include <onix/stdarg.h>
#include <onix/printk.h>
#include <onix/assert.h>
#include <onix/global.h>
#include <onix/stdlib.h>
#include <onix/time.h>
#include <onix/rtc.h>
#include <onix/types.h>
#include <onix/memory.h>
#include <onix/debug.h>
#include <onix/bitmap.h>
#include <onix/interrupt.h>
#include <onix/task.h>
#include <onix/memory.h>

extern void clock_init();
extern void syscall_init();
extern void list_test();
extern void keyboard_init();
extern void arena_init();
extern void time_init();
extern void ide_init();
extern void buffer_init();
extern void super_init();
/*
extern void  time_init();
extern void  rtc_init();
extern void  console_init();
extern void  gdt_init();    
extern void  interrupt_init();  
extern void  set_alarm(u32 secs);
extern void  bitmap_tests();
extern void  hang();
extern void  memory_test();
*/

void kernel_init()
{
    //console_init();  //移到start.asm调用了
    //gdt_init();   
    //memory_init();
    tss_init();
    memory_map_init();
    mapping_init();
    arena_init();

    interrupt_init();  
    clock_init();
    keyboard_init();
    time_init();
    ide_init();
    buffer_init();

    task_init();
    syscall_init();
    
    super_init();
    set_interrupt_state(true);
    
    //list_test();
    //time_init();
    //rtc_init();
    //BMB;
    //memory_test();
    //bitmap_tests();

}