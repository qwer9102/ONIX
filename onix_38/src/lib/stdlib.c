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
