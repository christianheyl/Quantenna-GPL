#!/bin/sh
# (c) 2013 Quantenna Communications, Inc.

echo "Backing up u-boot partition ..."
device_env=`grep '\"uboot_env\"' /proc/mtd 2>/dev/null | awk -F: '{print $1}'`
if [ -z "$device_env" -o ! -c /dev/$device_env ]; then
	echo "[$?]: The partition 'uboot_env' not found!"
	exit 1
fi

device_bak=`grep '\"uboot_env_bak\"' /proc/mtd 2>/dev/null | awk -F: '{print $1}'`
if [ -z "$device_bak" ]; then
	echo "[$?]: The backup partition 'uboot_env_bak' not found!"
	exit 1
fi

mtdblock_env=`echo $device_env | sed 's/mtd/mtdblock/'`
mtdblock_bak=`echo $device_bak | sed 's/mtd/mtdblock/'`
size_env=`grep "$mtdblock_env$" /proc/partitions 2>/dev/null | awk '{print $3}'`
size_bak=`grep "$mtdblock_bak$" /proc/partitions 2>/dev/null | awk '{print $3}'`
if [ -z "$size_env" -o -z "$size_bak" ]; then
	echo "Failed to retrieve the partition size!"
	exit 1
fi
if [ $size_env -gt $size_bak ]; then
	echo "No enough space for backup!"
	exit 1
fi

echo "Erasing backup partition ..."
flash_eraseall /dev/$device_bak
if [ $? -ne 0 ]; then
	echo "[$?]: Failed to erase partition!"
	exit 1
fi

/bin/sync
usleep 500000

echo "Creating backup..."
dd if=/dev/$device_env of=/dev/$device_bak bs=1k count=$size_env
if [ $? -ne 0 ]; then
	echo "[$?]: Fail to create backup!"
	exit 1
fi

/bin/sync
usleep 500000

echo "Verifying backup..."
md5_env=`md5sum /dev/$device_env 2>/dev/null | awk '{print $1}'`
md5_bak=`md5sum /dev/$device_bak 2>/dev/null | awk '{print $1}'`
if [ $? -eq 0 -a "$md5_env" == "$md5_bak" ]; then
	echo "Done"
else
	echo "Fail"
	echo "md5_env: $md5_env"
	echo "md5_bak: $md5_bak"
	echo "md5_bak checksum not match"
	exit 2
fi
