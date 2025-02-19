[bits 32]

global _start

_start:
    mov byte [0xb8000], 'K'  ; Print 'K' 表示进入内核
    jmp $
