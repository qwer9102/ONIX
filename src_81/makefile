BOCHS_BIN := /home/qwas/software/bochs/bin/bochs
BOCHSRC := /home/qwas/software/bochs/bin/bochsrc
IMAGE := $(CURDIR)/master.img 

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
	$(shell mkdir -p $(dir $@))
	ld ${LDFLAGS} $^ -o $@
#	ld -m elf_i386 -static -o $@ $^ -Ttext $(ENTRYPOINT) 

$(BUILD)/system.bin: $(BUILD)/kernel.bin
	objcopy -O binary $< $@

$(BUILD)/system.map: $(BUILD)/kernel.bin
	nm $< | sort > $@

$(BUILD)/master.img: $(BUILD)/boot/boot.bin \
					 $(BUILD)/boot/loader.bin \
					 $(BUILD)/system.bin \
					 $(BUILD)/system.map	\
					 $(SRC)/utils/master.sfdisk				 
# 创建一个16M的硬盘印像
	yes | /home/qwas/software/bochs/bin/bximage -q -hd=16 -func=create -sectsize=512  -imgmode=flat  $@
# 将 boot.bin写入主引导扇区
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc	
# 将loader.bin写入硬盘
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=4 seek=2 conv=notrunc	
# 测试system.bin小于100K,否则需要修改count值
	test -n "$$(find $(BUILD)/system.bin -size -100k)"
# 将system.bin写入硬盘
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=200 seek=10 conv=notrunc	
# 对磁盘进行分区
	sfdisk $@ < $(SRC)/utils/master.sfdisk	

$(BUILD)/slave.img:
# 创建一个32M的硬盘印像(slave)
	yes | /home/qwas/software/bochs/bin/bximage -q -hd=32 -func=create -sectsize=512  -imgmode=flat  $@

#test:$(BUILD)/master.img

IMAGES:= $(BUILD)/master.img $(BUILD)/slave.img
image: $(IMAGES)


#生成iso文件
$(BUILD)/kernel.iso:$(BUILD)/kernel.bin $(SRC)/utils/grub.cfg
# 检测内存文件是否合法	
	grub-file --is-x86-multiboot2 $<
#创建 iso 目录
	mkdir -p $(BUILD)/iso/boot/grub
#拷贝内核文件
	cp $< $(BUILD)/iso/boot
#拷贝grub配置文件
	cp $(SRC)/utils/grub.cfg $(BUILD)/iso/boot/grub
#生成 iso 文件
	grub-mkrescue -o $@ $(BUILD)/iso


.PHONY:clean
clean:
	rm -rf $(BUILD)

.PHONY:bochs
bochs:$(BUILD)/master.img
	$(BOCHS_BIN) -f  $(BOCHSRC)

QEMU:= qemu-system-i386   #虚拟机
QEMU+= -m 32M 	#内存
QEMU+= -audiodev pa,id=hda #音频设备
QEMU+= -machine pcspk-audiodev=hda #pcspeaker设备
QEMU+= -rtc base=localtime #设备本地时间
QEMU+= -drive file=$(BUILD)/master.img,if=ide,index=0,media=disk,format=raw  #主硬盘
QEMU+= -drive file=$(BUILD)/slave.img,if=ide,index=1,media=disk,format=raw  #从硬盘

QEMU_DEBUG:= -s -S

QEMU_DISK:=-boot c \
#	-drive file=$(BUILD)/master.img,if=ide,index=0,media=disk,format=raw \

QEMU_CDROM:=-boot d \
#	-drive file=$(BUILD)/kernel.iso,media=cdrom \

.PHONY:qemu
qemu:$(BUILD)/master.img
#	$(QEMU)  
	$(QEMU)  $(QEMU_DISK)

.PHONY:qemug
qemug:$(BUILD)/master.img
#	$(QEMU) -s -S
	$(QEMU) $(QEMU_DISK) $(QEMU_DEBUG)

.PHONY:qemub
qemub:$(BUILD)/kernel.iso
	$(QEMU) $(QEMU_CDROM) \
	# $(QEMU_DEBUG)


$(BUILD)/master.vmdk: $(BUILD)/master.img
	qemu-img convert -O vmdk $< $@ 

.PHONY:vmdk
vmdk:$(BUILD)/master.vmdk