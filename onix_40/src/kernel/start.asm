[bits 32]

extern kernel_init
extern gdt_init
extern console_init
extern memory_init



global _start

_start:
    push ebx ;ards_count
    push eax ;magic
    
    call console_init 
    call gdt_init
    call memory_init
    call kernel_init
    
    ;int 0x80 ;调用0x80 系统调用中断函数

    jmp $
