#include <onix/global.h>
#include <onix/interrupt.h>
#include <onix/debug.h>
#include <onix/printk.h>
#include <onix/stdlib.h>
#include <onix/io.h>
#include <onix/assert.h>

//#define LOGK(fmt,args...) DEBUGK(fmt,##args)

#define ENTRY_SIZE 0X30

#define PIC_M_CTRL 0x20 // 主片的控制端口
#define PIC_M_DATA 0x21 // 主片的数据端口
#define PIC_S_CTRL 0xa0 // 从片的控制端口
#define PIC_S_DATA 0xa1 // 从片的数据端口
#define PIC_EOI    0x20 // 通知中断控制器中断结束

gate_t idt[IDT_SIZE];
pointer_t idt_ptr;//pointer_t 定义在global.h,和gdt_pointer一样

handler_t  handler_table[IDT_SIZE];
extern handler_t handler_entry_table[ENTRY_SIZE];
extern void syscall_handler();
extern void page_fault();

static char *messages[] = {
    "#DE Divide Error\0",
    "#DB RESERVED\0",
    "--  NMI Interrupt\0",
    "#BP Breakpoint\0",
    "#OF Overflow\0",
    "#BR BOUND Range Exceeded\0",
    "#UD Invalid Opcode (Undefined Opcode)\0",
    "#NM Device Not Available (No Math Coprocessor)\0",
    "#DF Double Fault\0",
    "    Coprocessor Segment Overrun (reserved)\0",
    "#TS Invalid TSS\0",
    "#NP Segment Not Present\0",
    "#SS Stack-Segment Fault\0",
    "#GP General Protection\0",
    "#PF Page Fault\0",
    "--  (Intel reserved. Do not use.)\0",
    "#MF x87 FPU Floating-Point Error (Math Fault)\0",
    "#AC Alignment Check\0",
    "#MC Machine Check\0",
    "#XF SIMD Floating-Point Exception\0",
    "#VE Virtualization Exception\0",
    "#CP Control Protection Exception\0",
};


// 通知中断控制器，中断处理结束
void send_eoi(int vector)
{
    if (vector >= 0x20 && vector < 0x28)    //pc中断默认级联:20-28设置为主片中断
    {
        outb(PIC_M_CTRL, PIC_EOI);
    }
    if (vector >= 0x28 && vector < 0x30)    //从片中断
    {
        outb(PIC_M_CTRL, PIC_EOI);
        outb(PIC_S_CTRL, PIC_EOI);
    }
}


// 注册中断处理函数:irq是中断号,handler是对应的中断处理函数
//irq是注册号,因为这里8259A只有两个片,只有15个中断
void set_interrupt_handler(u32 irq,handler_t handler)
{
    assert(irq >= 0 && irq < 16);
    handler_table[IRQ_MASTER_NR + irq] = handler;
}

//设置中断屏蔽字:通过控制对应的PIC_DATA位
void set_interrupt_mask(u32 irq,bool enable)
{
    assert( irq >= 0 &&  irq < 16 );
    u16 port ;
    if(irq < 8)
    {
        port = PIC_M_DATA;
    }
    else
    {
        port = PIC_S_DATA;
        irq -= 8;
    }
    
    if(enable)
    {
        //假设irq=2,那么~(1 << irq) = 011,就可以单独设置对应的irq位为0
        outb(port, inb(port) & ~(1 << irq) );   //设置对应的irq为0表示开启对应的中断
    }
    else
    {
        outb(port,inb(port) | (1 << irq));    //设置对应的irq为1表示禁用对应的中断
    }
}

//IF位相关,IF位在eflags寄存器第10位
//清除IF位,并返回之前的值
bool interrupt_disable()
{
    asm volatile(
        "pushfl\n"    //压入eflags
        "cli\n"       //清除IF位,外中断关闭
        "popl %eax\n" //弹出eflags到eax
        "shrl $9,%eax\n"
        "andl $1,%eax\n");
}  

//获得IF位值
bool get_interrupt_state()
{
    asm volatile(
        "pushfl\n"          //压入eflags
        "popl %eax\n"       //弹出eflags到eax
        "shrl $9,%eax\n"    //右移动得到IF位
        "andl $1,%eax\n");
}

//设置IF位值
void set_interrupt_state(bool state)
{
    if(state)
        asm volatile ("sti\n");   //开中断
    else
        asm volatile ("cli\n");   //关中断
} 

//extern void schedule();
void default_handler(int vector)
{
    send_eoi(vector);
    LOGK("[%x] default interrupt call %d...",vector);
    //schedule();   
}



void exception_handler(int vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags)
{
    char *message = NULL;
    if (vector < 22)
    {
        message = messages[vector];
    }
    else
    {
        message = messages[15];
    }

    //printk("EXCEPTION : [0x%02X] %s \n",vector,messages[vector]);
    printk("\nEXCEPTION : %s \n", message);
    printk("   VECTOR : 0x%02X\n", vector);
    printk("    ERROR : 0x%08X\n", error);
    printk("   EFLAGS : 0x%08X\n", eflags);
    printk("       CS : 0x%02X\n", cs);
    printk("      EIP : 0x%08X\n", eip);
    printk("      ESP : 0x%08X\n", esp);

    // 阻塞
   hang();
   
}

//初始化中断控制器
void pic_init()
{
    outb(PIC_M_CTRL, 0b00010001); // ICW1: 边沿触发, 级联 8259, 需要ICW4.
    outb(PIC_M_DATA, 0x20);       // ICW2: 起始中断向量号 0x20
    outb(PIC_M_DATA, 0b00000100); // ICW3: IR2接从片.
    outb(PIC_M_DATA, 0b00000001); // ICW4: 8086模式, 正常EOI

    outb(PIC_S_CTRL, 0b00010001); // ICW1: 边沿触发, 级联 8259, 需要ICW4.
    outb(PIC_S_DATA, 0x28);       // ICW2: 起始中断向量号 0x28
    outb(PIC_S_DATA, 2);          // ICW3: 设置从片连接到主片的 IR2 引脚
    outb(PIC_S_DATA, 0b00000001); // ICW4: 8086模式, 正常EOI

    outb(PIC_M_DATA, 0b11111111); // 关闭所有中断
    outb(PIC_S_DATA, 0b11111111); // 关闭所有中断
}


void idt_init()
{
    for(size_t i = 0;i < ENTRY_SIZE; i++)
    {
        gate_t *gate  = &idt[i];
        handler_t handler = handler_entry_table[i];
        gate->offset0 = (u32)handler & 0xffff; 
        gate->offset1 = ((u32)handler >> 16) & 0xffff;
        gate->selector = 1 << 3;//代码段
        gate->reserved = 0;     //保留不用
        gate->type = 0b1110;    //中断门
        gate->DPL = 0;     //内核态
        gate->segment = 0; //系统段
        gate->present = 1; //有效
    }

    for(size_t i = 0;i < 0x20; i++)
    {
        handler_table[i] = exception_handler;
    }
    //缺页异常函数处理
    handler_table[0xe] = page_fault;
    //初始化0x20-0x2f中断函数为default_handler
    for(size_t i = 0x20; i < ENTRY_SIZE; i++)
    {
        handler_table[i] = default_handler;
    }

    //初始化系统调用
    gate_t *gate = &idt[0x80];
    gate->offset0 = (u32)syscall_handler;
    gate->offset0 = (u32)syscall_handler & 0xffff; 
    gate->offset1 = ((u32)syscall_handler >> 16) & 0xffff;
    gate->selector = 1 << 3;//代码段
    gate->reserved = 0;     //保留不用
    gate->type = 0b1110;    //中断门
    gate->DPL = 3;     //用户态
    gate->segment = 0; //系统段
    gate->present = 1; //有效


    idt_ptr.base = (u32)idt;
    idt_ptr.limit = sizeof(idt) - 1;
    
    asm volatile("lidt idt_ptr\n");
}


void interrupt_init()
{
    pic_init();
    idt_init();
}