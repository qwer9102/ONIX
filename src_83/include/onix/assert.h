#ifndef ONIX_ASSERT_H
#define ONIX_ASSERT_H


void assertion_failure(char *exp,char *file,char *base,int line);

//函数宏定义,exp会填充到下面函数中
#define assert(exp) \
    if(exp)         \
        ;           \
    else            \
        assertion_failure(#exp,__FILE__,__BASE_FILE__,__LINE__)


void panic(const char *fmt, ...);

#endif
