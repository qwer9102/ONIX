[org 0x1000]

dw 0x55aa

;xchg bx, bx ;魔术断点

mov si, loading
call print

;xchg bx, bx

detect_memory:
    xor ebx,ebx; ebx = 0

    ;设置es:di结构体的缓存位置-- 0：adrs_buffer
    mov ax,0
    mov es,ax
    mov edi,ards_buffer

    mov edx,0x534d4150; 固定签名"SMAP"

.next:
    mov eax,0xe820

    mov ecx,20
    ;到这设置好了5个寄存器的参数，可以执行检测命令
    int  0x15      

    jc error; cf=1,表示错误   jc命令就是检测进位标志位CF的
    ;缓存指针指向下一个结构体
    add di,cx; di = di + cx
    ;结构体数量加1
    inc dword [ards_count]; ards_count = ards_count + 1

    ;检测了多少段内存
    ;mov si, detect_count
    ;call print

    cmp ebx,0  ;初始设置为0,执行后会自动变化,最后一个时会置零
    jnz .next  ;如果ebx不为0，继续执行

    mov si, detecting
    call print

    ;xchg bx, bx
    ;mov byte [0xb8000], 'P'

    ;检测成功，跳转到保护模式函数入口
    jmp prepare_protected_mode


prepare_protected_mode:
    ;xchg bx, bx; 断点

    cli; 关闭中断

    ; 打开 A20 线
    in al, 0x92
    or al, 0b10
    out 0x92, al

    lgdt [gdt_ptr]; 加载 gdt

    ; 启动保护模式
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; 用跳转来刷新缓存，启用保护模式
    jmp dword code_selector:protect_mode


;打印函数
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

loading:
    db "Loading Onix...", 10, 13, 0 ; \nr
detecting:
    db "Detecting Memory Success...", 10, 13, 0; \n\r
detect_count:
    db "Detected_Memory Count + 1 ", 10, 13, 0; \n\r

error:
    mov si, .msg
    call print
    hlt; 让 CPU 停止
    jmp $
    .msg db "Loading Error!!!", 10, 13, 0


[bits 32]
protect_mode:
    xchg bx, bx; 断点
    mov ax, data_selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax; 初始化段寄存器

    mov esp, 0x10000; 修改栈顶

    ;xchg bx, bx; 断点
    ;mov byte [0xb8000], 'P'
    ;mov byte [0x200000], 'P'

    ;读取扇区，使用LBA模式
    mov edi,0x10000 ;读取到0x10000处
    mov ecx,10 ;起始扇区，这里读取10扇区
    mov bl,200  ;读取扇区的大小，读取200扇区
    call read_disk
    ;xchg bx, bx; 断点
    jmp dword code_selector:0x10000

    ud2;表示出错


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

;底下是准备数据代码
code_selector equ (1 << 3)
data_selector equ (2 << 3)

memory_base equ 0; 内存开始的位置：基地址
; 内存界限 4G / 4K - 1
memory_limit equ ((1024 * 1024 * 1024 * 4) / (1024 * 4)) - 1

gdt_ptr:
    dw (gdt_end - gdt_base) - 1
    dd gdt_base
gdt_base:
    dd 0, 0; NULL 描述符
gdt_code:
    dw memory_limit & 0xffff; 段界限 0 ~ 15 位
    dw memory_base & 0xffff; 基地址 0 ~ 15 位
    db (memory_base >> 16) & 0xff; 基地址 16 ~ 23 位
    ; 存在 - dlp 0 - S _ 代码 - 非依从 - 可读 - 没有被访问过
    db 0b_1_00_1_1_0_1_0;
    ; 4k - 32 位 - 不是 64 位 - 段界限 16 ~ 19
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf;
    db (memory_base >> 24) & 0xff; 基地址 24 ~ 31 位
gdt_data:
    dw memory_limit & 0xffff; 段界限 0 ~ 15 位
    dw memory_base & 0xffff; 基地址 0 ~ 15 位
    db (memory_base >> 16) & 0xff; 基地址 16 ~ 23 位
    ; 存在 - dlp 0 - S _ 数据 - 向上 - 可写 - 没有被访问过
    db 0b_1_00_1_0_0_1_0;
    ; 4k - 32 位 - 不是 64 位 - 段界限 16 ~ 19
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf;
    db (memory_base >> 24) & 0xff; 基地址 24 ~ 31 位
gdt_end:

ards_count:
    dd 0
ards_buffer:
