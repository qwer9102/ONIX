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

QEMU_CDROM:=-boot d 

.PHONY:qemub
qemub:$(BUILD)/kernel.iso $(IMAGES)
	$(QEMU) $(QEMU_CDROM) \
	# $(QEMU_DEBUG)
