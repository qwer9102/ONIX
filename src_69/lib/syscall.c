#include <onix/syscall.h>

//0个参数的系统调用
static _inline u32 _syscall0(u32 nr)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)     //调用返回值eax给到ret
        : "a"(nr));     //参数i(syscall_table[i]),就是系统调用号通过eax带给0x80系统中断
    return ret;
}

//1个参数的系统调用
static _inline u32 _syscall1(u32 nr, u32 arg)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)     //调用返回值eax给到ret
        : "a"(nr),"b"(arg));  //参数i(syscall_table[i]),就是系统调用号通过eax带给0x80系统中断,arg给到ebx,nr给到eax
    return ret;
}

//2个参数的系统调用
static _inline u32 _syscall2(u32 nr, u32 arg1, u32 arg2)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)     //调用返回值eax给到ret
        : "a"(nr),"b"(arg1),"c"(arg2));  //参数i(syscall_table[i]),就是系统调用号通过eax带给0x80系统中断,arg给到ebx,nr给到eax
    return ret;
}


//3个参数的系统调用
static _inline u32 _syscall3(u32 nr, u32 arg1, u32 arg2, u32 arg3)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)     //调用返回值eax给到ret
        : "a"(nr),"b"(arg1),"c"(arg2),"d"(arg3));  //参数i(syscall_table[i]),就是系统调用号通过eax带给0x80系统中断,arg给到ebx,nr给到eax
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

pid_t getpid()
{
    return _syscall0(SYS_NR_GETPID);
}

pid_t getppid()
{
    return _syscall0(SYS_NR_GETPPID);
}

int32 brk(void *addr)
{
    return _syscall1(SYS_NR_BRK,(u32)addr);
}

int32 write(fd_t fd,char *buf, u32 len)
{
    return _syscall3(SYS_NR_WRITE,fd,(u32)buf,len);
}