#include <onix/stdlib.h>

//延迟函数
void delay(u32 count)
{
    while(count --)
        ;
    
}

//阻塞函数
void hang()
{
    while (true)
        ;
}

u8 bcd_to_bin(u8 value)
{
    return (value & 0xf) + (value >> 4) * 10;
}
u8 bin_to_bcd(u8 value)
{
    return (value / 10) * 0x10 + (value % 10);
}

//计算num 分成多少个size
u32 div_round_up(u32 num,u32 size)
{
    return (num + size - 1) / size;
}
