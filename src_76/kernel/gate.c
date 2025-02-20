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

//#include <onix/ide.h>
//#include <onix/string.h>

//extern  ide_ctrl_t controllers[IDE_CTRL_NR];
//task_t *task = NULL;
static u32 sys_test()
{
/*
    u16 *buf = (u16 *)alloc_kpage(1);
    
    LOGK("pio read buffer %x",buf);
    ide_disk_t *disk = &controllers[0].disks[0];
    ide_pio_read(disk,buf,4,0);
    BMB;

    memset(buf,0x5a,SECTOR_SIZE);
    ide_pio_write(disk,buf,1,1); //buf写到第2个扇区

    free_kpage((u32)buf,1);
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
extern time_t sys_time();
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
    
    syscall_table[SYS_NR_EXIT] = task_exit;
    syscall_table[SYS_NR_FORK] = task_fork;
    syscall_table[SYS_NR_WAITPID] = task_waitpid;

    syscall_table[SYS_NR_GETPID] = sys_getpid;
    syscall_table[SYS_NR_GETPPID] = sys_getppid;

    syscall_table[SYS_NR_WRITE] = sys_write;
    syscall_table[SYS_NR_TIME] = sys_time;

    
}