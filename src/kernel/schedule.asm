[bits 32]

section .text


;intel格式汇编:mov dest,src
global task_switch
task_switch:
    ;保存当前任务的寄存器
    push ebp       ;保存当前任务的ebp
    mov  ebp,esp   ;这两条指令把ebp/esp同时只项esp+1的位置,形成新栈帧

    push ebx
    push esi
    push edi

    mov eax,esp   ;对齐后就是当前任务的栈顶
    and eax,0xfffff000;eax=current任务的栈顶指针

    mov [eax],esp  ;保存现在的栈顶指针到[eax],就是current任务的栈顶
    mov eax,[ebp+8];取出传进来的参数(next任务的栈顶)到esp
    mov esp,[eax]
    ;弹出next任务自己的寄存器数据
    pop edi
    pop esi
    pop ebx
    pop ebp

    ret