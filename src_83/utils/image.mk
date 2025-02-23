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
#挂载设备
#sudo losetup -d /dev/loop0 2>/dev/null || true  # 清理旧设备
	sudo losetup /dev/loop0 --partscan $@
#创建minuxw文件系统,第一版,文件名字最长14
	sudo mkfs.minix -1 -n 14 /dev/loop0p1
#挂载文件系统
	sudo mount /dev/loop0p1 /mnt
#切换所有者
	sudo chown ${USER} /mnt
#创建目录
	mkdir -p /mnt/home
	mkdir -p /mnt/d1/d2/d3/d4
#创建文件
	echo "hello onix!!!,from root directory file..." >/mnt/hello.txt
	echo "hello onix!!!,from home directory file..." >/mnt/home/hello.txt
#卸载文件系统
	sudo umount /mnt
#卸载设备
	sudo losetup -d /dev/loop0



$(BUILD)/slave.img:$(SRC)/utils/slave.sfdisk
# 创建一个32M的硬盘印像(slave)
	yes | /home/qwas/software/bochs/bin/bximage -q -hd=32 -func=create -sectsize=512  -imgmode=flat  $@
# 对磁盘进行分区
	sfdisk $@ < $(SRC)/utils/slave.sfdisk	
#挂载设备
	sudo losetup /dev/loop1 --partscan $@
#创建minuxw文件系统,第一版,文件名字最长14
	sudo mkfs.minix -1 -n 14 /dev/loop1p1
#挂载文件系统
	sudo mount /dev/loop1p1 /mnt
#切换所有者
	sudo chown ${USER} /mnt
#创建文件
	echo "slave  root directory file..." >/mnt/hello.txt
#卸载文件系统
#sudo umount /mnt 2>/dev/null || true 
	sudo umount /mnt
#卸载设备
#sudo losetup -d /dev/loop0 2>/dev/null || true
	sudo losetup -d /dev/loop1


.PHONY: mount0
mount0: ${BUILD}/master.img
	sudo losetup -d /dev/loop0 2>/dev/null || true
	sudo losetup /dev/loop0 --partscan $<
	sudo mount /dev/loop0p1 /mnt
	sudo chown ${USER} /mnt

.PHONY: umount0
umount0: /dev/loop0
	-sudo umount /mnt
	-sudo losetup -d $<


.PHONY: mount1
mount1: ${BUILD}/slave.img
	sudo losetup -d /dev/loop0 2>/dev/null || true
	sudo losetup /dev/loop1 --partscan $<
	sudo mount /dev/loop1p1 /mnt
	sudo chown ${USER} /mnt

.PHONY: umount1
umount1: /dev/loop1
	-sudo umount /mnt
	-sudo losetup -d $<


IMAGES:= $(BUILD)/master.img \
	$(BUILD)/slave.img

image: $(IMAGES)

#卸载命令:failed to set up loop device: Device or resource busy 
#sudo umount /mnt 2>/dev/null      # 卸载可能的残留挂载
#sudo losetup -d /dev/loop0 2>/dev/null  # 强制删除 loop0 设备
#sudo losetup -d /dev/loop1 2>/dev/null  # 强制删除 loop1 设备