
#ifndef ONIX_DEBUG_H
#define ONIX_DEBUG_H

void debugk(char *file, int line, const char *fmt, ...);

#define BMB asm volatile("xchgw %bx, %bx") // bochs magic breakpoint
//args...表示可以一个or多个参数
//##args 的作用是处理可变参数的传递。它会将 args 中的参数展开并传递给 debugk 函数。
//特别地，## 符号在这里的作用是如果没有传入任何参数（即 args 是空的），则会删除 , 逗号，使得调用 debugk 时的参数列表是有效的。
#define DEBUGK(fmt, args...) debugk(__BASE_FILE__, __LINE__, fmt, ##args)

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#endif
