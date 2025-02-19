#include <onix/onix.h>
#include <onix/io.h>
#include <onix/types.h>
#include <onix/string.h>
#include <onix/console.h>
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
//3.string测试
char message[] = "hello onix!!!\n";
char buf[1024];

void kernel_init()
{
 /*   char *video = (char*)0xb8000;
    for(int i = 0; i < sizeof(message); i++)
    {   video[i*2] = message[i];   }  */
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
    /*string测试
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
    console_init();

    //u32 count = 30;
    while(true)
    {
        console_write(message,sizeof(message)-1);
    }

    return;

}