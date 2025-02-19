#include <onix/syscall.h>

static _inline u32 _syscall0(u32 nr)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)     //调用返回值eax给到ret
        : "a"(nr));     //参数i(syscall_table[i]),就是系统调用号通过eax带给0x80系统中断
    return ret;
}
/*
static _inline u32 _syscall0(u32 nr)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr));
    return ret;
}
*/
u32 test()
{
    return _syscall0(SYS_NR_TEST);
}


void yield()
{
    _syscall0(SYS_NR_YIELD);
}