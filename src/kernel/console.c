#include <onix/console.h>
#include <onix/io.h>
#include <onix/string.h>
#include <onix/interrupt.h>


#define CRT_ADDR_REG 0x3D4 // CRT(6845)索引寄存器
#define CRT_DATA_REG 0x3D5 // CRT(6845)数据寄存器

#define CRT_START_ADDR_H 0xC // 显示内存起始位置 - 高位
#define CRT_START_ADDR_L 0xD // 显示内存起始位置 - 低位
#define CRT_CURSOR_H 0xE     // 光标位置 - 高位
#define CRT_CURSOR_L 0xF     // 光标位置 - 低位

#define MEM_BASE 0xB8000              // 显卡内存起始位置
#define MEM_SIZE 0x4000               // 显卡内存大小
#define MEM_END (MEM_BASE + MEM_SIZE) // 显卡内存结束位置
#define WIDTH 80                      // 屏幕文本列数
#define HEIGHT 25                     // 屏幕文本行数
#define ROW_SIZE (WIDTH * 2)          // 每行字节数
#define SCR_SIZE (ROW_SIZE * HEIGHT)  // 屏幕字节数

#define ASCII_NUL 0x00
#define ASCII_ENQ 0x05
#define ASCII_ESC 0x1B // ESC
#define ASCII_BEL 0x07 // \a
#define ASCII_BS 0x08  // \b
#define ASCII_HT 0x09  // \t
#define ASCII_LF 0x0A  // \n
#define ASCII_VT 0x0B  // \v
#define ASCII_FF 0x0C  // \f
#define ASCII_CR 0x0D  // \r
#define ASCII_DEL 0x7F


static u32 screen;  //显示器开始的内存位置
static u32 pos;     //当前光标的内存位置
static u32 x, y;    //当前光标坐标

static u8 attr = 7; //字符样式
static u16 erase = 0x0720;

//获取当前显示器开始的位置
static void  get_screen()
{
    outb(CRT_ADDR_REG,CRT_START_ADDR_H);
    screen = inb(CRT_DATA_REG) << 8;
    outb(CRT_ADDR_REG,CRT_START_ADDR_L);
    screen |= inb(CRT_DATA_REG);

    screen = screen << 1;       //显示一个字符需要字符+样式两个字节
    screen = screen + MEM_BASE; 
}

//设置当前显示器开始的位置为screen
static void  set_screen()
{
    outb(CRT_ADDR_REG,CRT_START_ADDR_H);
    outb(CRT_DATA_REG,((screen - MEM_BASE) >> 9) & 0xff );
    outb(CRT_ADDR_REG,CRT_START_ADDR_L);
    outb(CRT_DATA_REG,((screen - MEM_BASE) >> 1) & 0xff );
}

//获取光标位置
static void get_cursor()
{
    outb(CRT_ADDR_REG,CRT_CURSOR_H);
    pos = inb(CRT_DATA_REG) << 8;
    outb(CRT_ADDR_REG,CRT_CURSOR_L);
    pos |= inb(CRT_DATA_REG);   

    get_screen();
    pos <<= 1;
    pos += MEM_BASE;

    //获取在屏幕的相对位置
    u32 delta = (pos - screen) >> 1;
    x = delta % WIDTH;
    y = delta / WIDTH;    
}

//设置光标位置为pos
static void  set_cursor()
{
    outb(CRT_ADDR_REG,CRT_CURSOR_H);
    outb(CRT_DATA_REG,((pos - MEM_BASE) >> 9) & 0xff );
    outb(CRT_ADDR_REG,CRT_CURSOR_L);
    outb(CRT_DATA_REG,((pos - MEM_BASE) >> 1) & 0xff );
}


void console_clear()
{
    screen = MEM_BASE;
    pos = MEM_BASE;
    x = y = 0;
    set_cursor();  //设置光标位置为pos,也就是mem_base
    set_screen();

    //清空屏幕,全部设置为空格(erase)
    u16 *ptr = (u16*)MEM_BASE;
    while (ptr < (u16*)MEM_END )
    {
        *ptr++ = erase;
    }
}

//向上滚动一行
static void scroll_up()
{
    //screen是显示器开头位置,screen + SCR_SIZE表示翻页后的开头位置
    if(screen + SCR_SIZE + ROW_SIZE >= MEM_END)
     {   //超过映射位置,从头mem_base开始显示
        memcpy((void*)MEM_BASE,(void*)screen,SCR_SIZE);
        pos -= (screen-MEM_BASE);
        screen = MEM_BASE;
    }
    
    u32 *ptr = (u32*)(screen + SCR_SIZE);
    for(size_t i = 0; i < WIDTH; i++)
    {
        *ptr++ = erase;
    } 
    screen += ROW_SIZE;
    pos += ROW_SIZE;
    
    set_screen();
}

static void command_lf()
{
    if(y+1 < HEIGHT)
    {
        y++;
        pos += ROW_SIZE;
        return ;
    }
    scroll_up();    
}

static void commangd_cr()
{
    pos -= (x << 1);
    x = 0;
}

static void commangd_bs()
{
    if(x)
    {
        x--;
        pos -=2;
        *(u16*)pos = erase;
    }
}

static void commangd_del()
{
    *(u16*)pos = erase;
}

extern void start_beep();

void console_write(char *buf,u32 count)
{

    bool intr = interrupt_disable(); //禁止中断,防止竞争,改成mutex

    char ch;
    char *ptr = (char*)pos;
    while (count--)
    {
        ch = *buf++ ;
        switch(ch)
        {
        case ASCII_NUL:
            break;
        case ASCII_BEL:
            start_beep();
            break;
        case ASCII_BS:
            commangd_bs();
            break;
        case ASCII_HT:
            break;
        case ASCII_LF:
            command_lf();
            commangd_cr();
            break;
        case ASCII_VT:
            break;
        case ASCII_FF:
            command_lf();
            break;
        case ASCII_CR:
        commangd_cr();
            break;
        case ASCII_DEL:
        commangd_del();
            break; 

        default:
            //是否换行
            if(x >= WIDTH)
            {
                x -= WIDTH;
                pos -= ROW_SIZE;
                command_lf();
            }
            /*
            *ptr = ch; //从pos位置写字符
            ptr++;
            *ptr = attr;//写样式
            ptr++;
            pos +=2;*/
            *((char*)pos) = ch;
            pos++;
            *((char*)pos) = attr;
            pos++;

            x++;    
            break;
        }   
    }
    set_cursor();

    set_interrupt_state(intr); //恢复中断 
}


void console_init()
{
    //console_clear();    
    //get_screen();

    //screen = 80*2 +MEM_BASE; //设置屏幕从第二行开始显示,一行80个字符
    //set_screen();
    //get_screen();

    //pos = 124 + MEM_BASE;
    //set_cursor();
    console_clear();

}