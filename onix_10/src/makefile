BOCHS_BIN := /home/qwas/software/bochs/bin/bochs
BOCHSRC := /home/qwas/software/bochs/bin/bochsrc
IMAGE := $(CURDIR)/master.img 

BUILD:=../build
SRC:=.


ENTRYPOINT:=0x10000


$(BUILD)/boot/%.bin: $(SRC)/boot/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin  $< -o $@

$(BUILD)/kernel/%.o: $(SRC)/kernel/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32  $< -o $@

$(BUILD)/kernel.bin: $(BUILD)/kernel/start.o
	$(shell mkdir -p $(dir $@))
	ld -m elf_i386 -static -o $@ $^ -Ttext $(ENTRYPOINT) 

$(BUILD)/system.bin: $(BUILD)/kernel.bin
	objcopy -O binary $< $@

$(BUILD)/system.map: $(BUILD)/kernel.bin
	nm $< | sort > $@

$(BUILD)/master.img: $(BUILD)/boot/boot.bin \
					 $(BUILD)/boot/loader.bin \
					 $(BUILD)/system.bin \
					 $(BUILD)/system.map					 

	yes | /home/qwas/software/bochs/bin/bximage -q -hd=16 -func=create -sectsize=512  -imgmode=flat  $@
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc	
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=4 seek=2 conv=notrunc	
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=200 seek=10 conv=notrunc	


test:$(BUILD)/master.img


.PHONY:clean
clean:
	rm -rf $(BUILD)

.PHONY:bochs
bochs:$(BUILD)/master.img
	$(BOCHS_BIN) -f  $(BOCHSRC)