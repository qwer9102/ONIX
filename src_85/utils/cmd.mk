.PHONY:bochs
#bochs:$(BUILD)/master.img
bochs:$(IMAGES)
	$(BOCHS_BIN) -q -f  $(BOCHSRC) -unlock

QEMU:= qemu-system-i386   #虚拟机
QEMU+= -m 32M 	#内存
QEMU+= -audiodev pa,id=hda #音频设备
QEMU+= -machine pcspk-audiodev=hda #pcspeaker设备
QEMU+= -rtc base=localtime #设备本地时间
QEMU+= -drive file=$(BUILD)/master.img,if=ide,index=0,media=disk,format=raw  #主硬盘
QEMU+= -drive file=$(BUILD)/slave.img,if=ide,index=1,media=disk,format=raw  #从硬盘

QEMU_DEBUG:= -s -S

QEMU_DISK:=-boot c 

.PHONY:qemu
#qemu:$(BUILD)/master.img 
qemu:$(IMAGES) 
	$(QEMU)  $(QEMU_DISK)

.PHONY:qemug
#qemug:$(BUILD)/master.img
qemug:$(IMAGES)
	$(QEMU) $(QEMU_DISK) $(QEMU_DEBUG)


$(BUILD)/master.vmdk: $(BUILD)/master.img
	qemu-img convert -O vmdk $< $@ 

.PHONY:vmdk
vmdk:$(BUILD)/master.vmdk