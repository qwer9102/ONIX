[bits 32]

extern kernel_init


global _start

_start:
    xchg bx, bx
    ;mov byte [0xb8000], 'K'  ; Print 'K' 表示进入内核
    call kernel_init
    xchg bx, bx 
    jmp $
