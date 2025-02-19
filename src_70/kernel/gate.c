#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/syscall.h>
#include <onix/task.h>
#include <onix/console.h>
#include <onix/stdio.h>
#include <onix/memory.h>

#define SYSCALL_SIZE 256

handler_t  syscall_table[SYSCALL_SIZE];

void syscall_check(u32 nr)
{
    if(nr >= SYSCALL_SIZE)
    {
        panic("syscall nr error !!!");
    }
}

static void sys_default()
{
    panic("syscall not implemented !!!");
}

task_t *task = NULL;
static u32 sys_test()
{
    //LOGK("syscall test...");
/*
    if(!task)
    {
        task = running_task();
        task_block(task,NULL,TASK_BLOCKED);
    }
    else
    {
        task_unblock(task);
        task = NULL;
    }
*/
/*
    char *ptr;
    BMB;
    //ptr = (char *)0x1600000;
    //ptr[3] = 'T';
    link_page(0x1600000);
    BMB;
    ptr = (char*)0x1600000;
    ptr[3] = 'T';
    BMB;
    unlink_page(0x700000);
    BMB;
*/
    return 255;
}


int32 sys_write(fd_t fd,char *buf,u32 len)
{
    if(fd == stdout || fd == stderr)
    {
        return console_write(buf,len);
    }
    panic("write !!!");
    return 0;
} 

//extern void task_yield();

void syscall_init()
{
    for(size_t i = 0; i < SYSCALL_SIZE; i++)
    {
        syscall_table[i] = sys_default;
    }

    syscall_table[SYS_NR_TEST]  = sys_test;
    syscall_table[SYS_NR_SLEEP] = task_sleep;
    syscall_table[SYS_NR_YIELD] = task_yield;
    
    syscall_table[SYS_NR_BRK] = sys_brk;
    
    syscall_table[SYS_NR_FORK] = task_fork;

    syscall_table[SYS_NR_GETPID] = sys_getpid;
    syscall_table[SYS_NR_GETPPID] = sys_getppid;

    syscall_table[SYS_NR_WRITE] = sys_write;
    
}