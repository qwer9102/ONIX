[bits 32]

extern handler_table

section .text
;INTERRUPT_HANDLER 2  汇编宏定义,表示接受两个参数
;处理程序名称: interrupt_handler_%1，其中 %1 是中断号
;%ifn %2--检查第二个参数是否为0。如果是0，则推送一个特定的值到堆栈
%macro INTERRUPT_HANDLER 2
interrupt_handler_%1:
    ;xchg bx,bx
%ifn %2
    push 0x20220205
%endif
    push %1
    jmp interrupt_entry
%endmacro

interrupt_entry:
    push ds
    push es
    push fs
    push gs
    pusha

    mov eax,[esp + 12 * 4]  ;

    push eax

    call [handler_table + eax * 4];调用对应的中断处理函数
   
global interrupt_exit
interrupt_exit:
    ;xchg bx,bx
    add esp, 4

    popa
    pop gs
    pop fs
    pop es
    pop ds
    
    add esp,8
    ;xchg bx,bx
    iret 

INTERRUPT_HANDLER 0x00, 0; divide by zero
INTERRUPT_HANDLER 0x01, 0; debug
INTERRUPT_HANDLER 0x02, 0; non maskable interrupt
INTERRUPT_HANDLER 0x03, 0; breakpoint

INTERRUPT_HANDLER 0x04, 0; overflow
INTERRUPT_HANDLER 0x05, 0; bound range exceeded
INTERRUPT_HANDLER 0x06, 0; invalid opcode
INTERRUPT_HANDLER 0x07, 0; device not avilable

INTERRUPT_HANDLER 0x08, 1; double fault
INTERRUPT_HANDLER 0x09, 0; coprocessor segment overrun
INTERRUPT_HANDLER 0x0a, 1; invalid TSS
INTERRUPT_HANDLER 0x0b, 1; segment not present

INTERRUPT_HANDLER 0x0c, 1; stack segment fault
INTERRUPT_HANDLER 0x0d, 1; general protection fault
INTERRUPT_HANDLER 0x0e, 1; page fault
INTERRUPT_HANDLER 0x0f, 0; reserved

INTERRUPT_HANDLER 0x10, 0; x87 floating point exception
INTERRUPT_HANDLER 0x11, 1; alignment check
INTERRUPT_HANDLER 0x12, 0; machine check
INTERRUPT_HANDLER 0x13, 0; SIMD Floating - Point Exception

INTERRUPT_HANDLER 0x14, 0; Virtualization Exception
INTERRUPT_HANDLER 0x15, 1; Control Protection Exception
INTERRUPT_HANDLER 0x16, 0; reserved
INTERRUPT_HANDLER 0x17, 0; reserved

INTERRUPT_HANDLER 0x18, 0; reserved
INTERRUPT_HANDLER 0x19, 0; reserved
INTERRUPT_HANDLER 0x1a, 0; reserved
INTERRUPT_HANDLER 0x1b, 0; reserved

INTERRUPT_HANDLER 0x1c, 0; reserved
INTERRUPT_HANDLER 0x1d, 0; reserved
INTERRUPT_HANDLER 0x1e, 0; reserved
INTERRUPT_HANDLER 0x1f, 0; reserved

INTERRUPT_HANDLER 0x20, 0; clock中断
INTERRUPT_HANDLER 0x21, 0
INTERRUPT_HANDLER 0x22, 0
INTERRUPT_HANDLER 0x23, 0
INTERRUPT_HANDLER 0x24, 0
INTERRUPT_HANDLER 0x25, 0
INTERRUPT_HANDLER 0x26, 0
INTERRUPT_HANDLER 0x27, 0
INTERRUPT_HANDLER 0x28, 0;rtc 实时中断
INTERRUPT_HANDLER 0x29, 0
INTERRUPT_HANDLER 0x2a, 0
INTERRUPT_HANDLER 0x2b, 0
INTERRUPT_HANDLER 0x2c, 0
INTERRUPT_HANDLER 0x2d, 0
INTERRUPT_HANDLER 0x2e, 0;harddisk1,硬盘主通道控制器
INTERRUPT_HANDLER 0x2f, 0;harddisk2,硬盘从通道控制器


section .data
global handler_entry_table
handler_entry_table:
    dd interrupt_handler_0x00
    dd interrupt_handler_0x01
    dd interrupt_handler_0x02
    dd interrupt_handler_0x03
    dd interrupt_handler_0x04
    dd interrupt_handler_0x05
    dd interrupt_handler_0x06
    dd interrupt_handler_0x07
    dd interrupt_handler_0x08
    dd interrupt_handler_0x09
    dd interrupt_handler_0x0a
    dd interrupt_handler_0x0b
    dd interrupt_handler_0x0c
    dd interrupt_handler_0x0d
    dd interrupt_handler_0x0e
    dd interrupt_handler_0x0f
    dd interrupt_handler_0x10
    dd interrupt_handler_0x11
    dd interrupt_handler_0x12
    dd interrupt_handler_0x13
    dd interrupt_handler_0x14
    dd interrupt_handler_0x15
    dd interrupt_handler_0x16
    dd interrupt_handler_0x17
    dd interrupt_handler_0x18
    dd interrupt_handler_0x19
    dd interrupt_handler_0x1a
    dd interrupt_handler_0x1b
    dd interrupt_handler_0x1c
    dd interrupt_handler_0x1d
    dd interrupt_handler_0x1e
    dd interrupt_handler_0x1f
    
    dd interrupt_handler_0x20
    dd interrupt_handler_0x21
    dd interrupt_handler_0x22
    dd interrupt_handler_0x23
    dd interrupt_handler_0x24
    dd interrupt_handler_0x25
    dd interrupt_handler_0x26
    dd interrupt_handler_0x27
    dd interrupt_handler_0x28
    dd interrupt_handler_0x29
    dd interrupt_handler_0x2a
    dd interrupt_handler_0x2b
    dd interrupt_handler_0x2c
    dd interrupt_handler_0x2d
    dd interrupt_handler_0x2e
    dd interrupt_handler_0x2f


section .text

extern syscall_check
extern syscall_table
global syscall_handler
syscall_handler:
    ;xchg bx, bx

    ; 验证系统调用号
    push eax
    call syscall_check
    add esp, 4

    push 0x20220205

    push 0x80

    ; 保存上文寄存器信息
    push ds
    push es
    push fs
    push gs
    pusha

    push 0x80; 向中断处理函数传递参数中断向量 vector
    ;xchg bx, bx

    ;push ebp; 第六个参数
    ;push edi; 第五个参数
    ;push esi; 第四个参数
    push edx; 第三个参数
    push ecx; 第二个参数
    push ebx; 第一个参数

    ; 调用系统调用处理函数，syscall_table 中存储了系统调用处理函数的指针
    call [syscall_table + eax * 4]

    ;xchg bx, bx
    add esp, (3 * 4); 系统调用结束恢复栈
    ;add esp, (6 * 4); 系统调用结束恢复栈

    ; 修改栈中 eax 寄存器，设置系统调用返回值
    mov dword [esp + 8 * 4], eax

    ; 跳转到中断返回
    jmp interrupt_exit