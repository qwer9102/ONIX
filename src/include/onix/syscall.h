#ifndef ONIX_SYSCALL_H
#define ONIX_SYSCALL_H

#include <onix/types.h>

typedef enum syscall_t
{
    SYS_NR_TEST,
    SYS_NR_YIELD,
    SYS_NR_SLEEP,
}syscall_t;

u32 test();
void yield();
void sleep(u32 ms);

#endif