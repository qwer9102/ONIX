[org 0x7c00]

;设置屏幕模式为文本模式
mov ax,3
int 0x10


;设置段寄存器
mov ax,0
mov ds,ax
mov es,ax  
mov ss,ax
mov sp,0x7c00


mov si, booting
call print

;打印字符
;mov ax, 0xb800
;mov ds, ax
;mov byte [0], 'H'


;读取扇区，使用LBA模式
mov edi,0x1000 ;读取到0x1000处
mov ecx,2 ;起始扇区，这里读取0扇区
mov bl,4  ;读取扇区的大小，读取一个扇区

call read_disk

cmp word [0x1000],0x55aa
jnz error
jmp 0:0x1002 ;跳转到 mov si, loading   call print 



jmp $   ;阻塞到$这一行

read_disk:
    ;设置读写扇区的数量，设置0x1f2寄存器
    mov dx, 0x1f2
    mov al,bl
    out dx,al ;将al中数据写到【dx】地址指向的寄存器中，这里是0x1f2

    ;设置读写扇区的起始地址，设置0x1f3~0x1f6寄存器
    inc dx; 0x1f3--0x1f6前4bit都是起始扇区的 0 ~ 27 位-----LBA模式扇区地址是28bit或48bit
    mov al,cl ;cl是ecx的低8位
    out dx,al ;设置*0x1f3地址的内容为ecx的低8位cl

    inc dx; 0x1f4
    shr ecx, 8
    mov al, cl; 起始扇区ecx中的中八位
    out dx, al

    inc dx; 0x1f5
    shr ecx, 8
    mov al, cl; 起始扇区的高八位
    out dx, al

    inc dx; 0x1f6
    shr ecx, 8
    and cl, 0b1111; 将高四位置为 0

    mov al, 0b1110_0000;
    or  al, cl
    out dx, al; 主盘 - LBA 模式

    inc dx; 0x1f7
    mov al,0x20
    out dx,al; 发送读取命令

    xor ecx,ecx; 将ecx清空  ;mov ecx,0
    mov cl,bl; 读取的扇区数

    .read:
        push cx
        call .waits  ;等待数据函数
        call .reads  ;读取一个扇区数据函数
        pop cx
        loop .read
    ret

    .waits:
        mov dx,0x1f7
        .check:
            in al,dx
            jmp $+2
            jmp $+2
            jmp $+2
            and al,0b1000_1000
            cmp al,0b0000_1000
            jnz .check
        ret

    .reads:
        mov dx,0x1f0
        mov cx,256   ; 一个扇区 256 字
        .readw:
            in ax,dx
            jmp $+2
            jmp $+2
            jmp $+2
            mov [edi],ax
            add edi,2
            loop .readw
        ret



print:
    mov ah, 0x0e
.next:
    mov al, [si]
    cmp al, 0
    jz .done
    int 0x10
    inc si
    jmp .next
.done:
    ret

booting:
    db "Booting Onix...", 10, 13, 0; \n\r


error:
    mov si, .msg
    call print
    hlt; 让 CPU 停止
    jmp $
    .msg db "Booting Error!!!", 10, 13, 0






;阻塞到0x55aa填充0
times 510-($-$$) db 0

;最后一个字节必须是0x55aa
;dw 0xaa55
db 0x55, 0xaa

