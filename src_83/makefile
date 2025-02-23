BOCHS_BIN := /home/qwas/software/bochs/bin/bochs
BOCHSRC := /home/qwas/software/bochs/bin/bochsrc
#IMAGE := $(CURDIR)/master.img 

#表示onix/build
BUILD:=../build
#表示当前目录,就是onix/src
SRC:=.

# ENTRYPOINT:=0x10000
MULTIBOOT2:=0x10000
ENTRYPOINT:=$(shell python3 -c "print(f'0x{$(MULTIBOOT2) + 64:x}')")


CFLAGS:= -m32 					# 32 位的程序
CFLAGS+= -march=pentium			# pentium 处理器
CFLAGS+= -fno-builtin			# 不需要 gcc 内置函数
CFLAGS+= -nostdinc				# 不需要标准头文件
CFLAGS+= -fno-pic				# 不需要位置无关的代码  position independent code
CFLAGS+= -fno-pie				# 不需要位置无关的可执行程序 position independent executable
CFLAGS+= -nostdlib				# 不需要标准库
CFLAGS+= -fno-stack-protector	# 不需要栈保护
CFLAGS+= -Werror
CFLAGS+= -DONIX					# 定义 ONIX
CFLAGS+= -DONIX_DEBUG			# 定义 ONIX_DEBUG
CFLAGS+= -DONIX_VERSION='"$(ONIX_VERSION)"' # 定义 ONIX_VERSION

CFLAGS:=$(strip ${CFLAGS})

DEBUG:= -g
INCLUDE:=-I$(SRC)/include

$(BUILD)/boot/%.bin: $(SRC)/boot/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin  $< -o $@

$(BUILD)/%.o: $(SRC)/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG) $< -o $@

$(BUILD)/%.o: $(SRC)/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c  $< -o $@

#$(BUILD)/lib/%.o: $(SRC)/lib/%.c
#	$(shell mkdir -p $(dir $@))
#	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c  $< -o $@


LDFLAGS:= -m elf_i386 \
		-static \
		-Ttext $(ENTRYPOINT)\
		--section-start=.multiboot2=$(MULTIBOOT2)
LDFLAGS:=$(strip ${LDFLAGS})


# \后面不能有空格(有空格颜色会变白)
$(BUILD)/kernel.bin: $(BUILD)/kernel/start.o \
		$(BUILD)/kernel/main.o \
		$(BUILD)/kernel/io.o \
		$(BUILD)/kernel/console.o \
		$(BUILD)/kernel/printk.o \
		$(BUILD)/kernel/assert.o \
		$(BUILD)/kernel/debug.o \
		$(BUILD)/kernel/global.o \
		$(BUILD)/kernel/task.o \
		$(BUILD)/kernel/schedule.o \
		$(BUILD)/kernel/interrupt.o \
		$(BUILD)/kernel/handler.o \
		$(BUILD)/kernel/clock.o \
		$(BUILD)/kernel/time.o \
		$(BUILD)/kernel/rtc.o \
		$(BUILD)/kernel/memory.o \
		$(BUILD)/kernel/gate.o \
		$(BUILD)/kernel/thread.o \
		$(BUILD)/kernel/mutex.o \
		$(BUILD)/kernel/keyboard.o \
		$(BUILD)/kernel/arena.o \
		$(BUILD)/kernel/buffer.o \
		$(BUILD)/kernel/ide.o \
		$(BUILD)/kernel/device.o \
		$(BUILD)/lib/string.o \
		$(BUILD)/lib/vsprintf.o \
		$(BUILD)/lib/stdlib.o \
		$(BUILD)/lib/bitmap.o \
		$(BUILD)/lib/syscall.o \
		$(BUILD)/lib/list.o \
		$(BUILD)/lib/fifo.o \
		$(BUILD)/lib/printf.o \
		$(BUILD)/fs/super.o \
	$(shell mkdir -p $(dir $@))
	ld ${LDFLAGS} $^ -o $@
#	ld -m elf_i386 -static -o $@ $^ -Ttext $(ENTRYPOINT) 

$(BUILD)/system.bin: $(BUILD)/kernel.bin
	objcopy -O binary $< $@

$(BUILD)/system.map: $(BUILD)/kernel.bin
	nm $< | sort > $@

include utils/image.mk
include utils/cdrom.mk
include utils/cmd.mk


.PHONY:clean
clean:
	rm -rf $(BUILD)