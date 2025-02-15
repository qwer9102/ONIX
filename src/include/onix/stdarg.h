#ifndef ONIX_STDARG_H
#define ONIX_STDARG_H

//va_list定义为字符指针类型
typedef char *va_list;

//计算类型t在栈中占用的空间。规则是：若t小于等于char*大小，则按char*对齐；否则直接取t的实际大小
#define stack_size(t) (sizeof(t) <= sizeof(char *) ? sizeof(char *) : sizeof(t))
//将ap指向第一个参数的栈位置
//(va_list)&v 的作用是获取最后一个命名参数 v (比如传进来的参数个数cnt)的地址，并将其转换为 va_list 类型/
#define va_start(ap, v) (ap = (va_list)&v + sizeof(char *))
//将ap指针移动stack_size(t)字节（指向下一个参数）,并且返回移动前地址里面的值（即当前参数)
//(ap += stack_size(t))将ap指向下一个参数,((ap += stack_size(t)) - stack_size(t))这个是根据ap值获取上一个参数的地址(当前参数的地址)
//地址转换成(t*)类型,取出值------整个过程就是先移动ap到下一个参数,然后根据更新后的ap值取出当前参数的地址,再取出值
#define va_arg(ap, t) (*(t *)((ap += stack_size(t)) - stack_size(t)))
//va_end 宏用于结束对可变参数的处理，将 ap 设置为 0
#define va_end(ap) (ap = (va_list)0)

#endif