#include <onix/onix.h>
#include <onix/io.h>
#include <onix/types.h>
#include <onix/string.h>
#include <onix/console.h>
#include <onix/stdarg.h>
#include <onix/printk.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/global.h>

/*1.
int magic = ONIX_MAGIC;
char message[] = "hello, onix!!!";   //.text
char buf[1024];                      //.bss
*/
/*2.控制光标位置测试
#define CRT_ADDR_REG 0x3D4
#define CRT_DATA_REG 0x3D5
#define CRT_CURSOR_H 0x0E
#define CRT_CURSOR_L 0x0F
*/
/*
//3.string测试
char message[] = "hello onix!!!\n";
char buf[1024];
*/
/* 4.stdarg.h测试
void test_args(int cnt,...)
{
    va_list args;
    va_start(args,cnt);

    int arg;
    while(cnt--)
    {
        arg = va_arg(args,int);
    }
    va_end(args);
}
*/

void kernel_init()
{
 /*   
    char *video = (char*)0xb8000;
    for(int i = 0; i < sizeof(message); i++)
    {   video[i*2] = message[i];   }  
*/
/* 控制光标位置测试
    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    u16 pos = inb(CRT_DATA_REG) << 8;
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    pos |= inb(CRT_DATA_REG);
    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    outb(CRT_DATA_REG, 0);
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    outb(CRT_DATA_REG, 123);
*/
/*  //string测试
    int res;
    res = strcmp(buf,message);
    strcpy(buf,message);
    res = strcmp(buf,message);
    strcat(buf,message);
    res = strcmp(buf,message);
    res = strlen(message);
    res = sizeof(message);

    char *ptr = strchr(message,'!');
    ptr = strrchr(message,'!');

    memset(buf,0,sizeof(buf));
    res = memcmp(buf,message,sizeof(message));
    memcpy(buf,message,sizeof(message));
    res = memcmp(buf,message,sizeof(message));
    ptr = memchr(buf,'!',sizeof(message));
*/

/*  //console_write测试
    while(true)
    {
        console_write(message,sizeof(message)-1);
    }
*/

   //test_args(5,1,0xaa,5,0x55,10);
/*
    int cnt = 30;
    while(cnt--)
    {
        printk("hello onix %#010x\n",cnt);
    }
*/
/*
    assert(3<5);
    assert(3>5);
    panic("Out of Memory");
*/  
    console_init();
    //BMB;    //bochs才能停
    //DEBUGK("debug onix!!!");
    gdt_init();    
    return;

}