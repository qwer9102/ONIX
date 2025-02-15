#include <onix/syscall.h>

//0个参数
static _inline u32 _syscall0(u32 nr)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)     //调用返回值eax给到ret
        : "a"(nr));     //参数i(syscall_table[i]),就是系统调用号通过eax带给0x80系统中断
    return ret;
}

//一个参数
static _inline u32 _syscall1(u32 nr,u32 arg)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)     //调用返回值eax给到ret
        : "a"(nr),"b"(arg));  //参数i(syscall_table[i]),就是系统调用号通过eax带给0x80系统中断,arg给到ebx,nr给到eax
    return ret;
}

u32 test()
{
    return _syscall0(SYS_NR_TEST);
}


void yield()
{
    _syscall0(SYS_NR_YIELD);
}

void sleep(u32 ms)
{
    _syscall1(SYS_NR_SLEEP,ms);
}