#!/bin/sh
# (c) 2013 Quantenna Communications, Inc.

# Carve the uboot_env partition into two parts.
#
# The mtdparts geometry is displayed.
# mtdparts=size[@offset](name)[,size[@offset](name)]
#
# Returns:
#   0 if mtdparts is different to /proc/cmdline
#   non-zero if mtdparts is the same as /proc/cmdline
#
# Sample usage:
#
# 1) create the mtdparts boot variable
#      set_bootval mtdparts `carve_env_partition.sh`

# The following value must be consistent with CONFIG_ENV_SIZE (u-boot) and
# BOOT_CFG_SIZE (bootcfg driver).  The minimum value is double the size of the data
# (for splitting the partition for backup), rounded up to the erase block boundary.

# RFIC version 1 is 5GHz only, 2 is dual 5GHz and 2.4GHz
rf_chipid=`call_qcsapi -u get_board_parameter rf_chipid`
if [ $? -ne 0 ]
then
        rf_chipid="1"
fi
if [ "$rf_chipid" == "1" ]
then
	CONFIG_ENV_SIZE=24
else
	CONFIG_ENV_SIZE=96
fi

mtdfile=/proc/mtd
cmdfile=/proc/cmdline

mtdparts_orig=`grep -o 'mtdparts=[^ ]*' $cmdfile 2>/dev/null`
env_bak=`grep '\"uboot_env_bak\"' $mtdfile 2>/dev/null | wc -l`

show_mtdparts()
{
	if [ $# -gt 0 ]; then
		echo "$*"
	fi
}

if [ "$env_bak" -ne 0 ]; then
	show_mtdparts $mtdparts_orig
	exit 1
fi

device_env=`grep '\"uboot_env\"' $mtdfile 2>/dev/null | awk '{print $1}'`
if [ -z "$device_env" ]; then
	show_mtdparts $mtdparts_orig
	exit 2
fi

size_env=$((0x`grep '\"uboot_env\"' $mtdfile 2>/dev/null | awk '{print $2}'` / 1024))
erase_sz=$((0x`grep '\"uboot_env\"' $mtdfile 2>/dev/null | awk '{print $3}'` / 1024))
if [ "$size_env" -eq 0 -o "$erase_sz" -eq 0 ]; then
	show_mtdparts $mtdparts_orig
        exit 3
fi

# round up new_size_env to the boundary of erase block
new_size_env=$CONFIG_ENV_SIZE
rest=$(($CONFIG_ENV_SIZE % $erase_sz))
if [ $rest -ne 0 ]; then
	new_size_env=$(($new_size_env + $erase_sz - $rest))
fi
new_size_bak=$new_size_env

if [ $size_env -lt $(($new_size_env * 2)) ]; then
	# no enough number of blocks
	show_mtdparts $mtdparts_orig
        exit 4
fi

# mtdparts is present in /proc/cmdline and uboot_env_bak does not exist; parse and modify it
offset=0
new_off_bak=0
if [ -n "$mtdparts_orig" -a -z "`echo $mtdparts_orig | grep -o 'uboot_env_bak'`" ]; then
	mtdparts="spi_flash:"
	# split out 'size[@off](name)' or '-(name)' for partitions
	for p in `echo $mtdparts_orig | sed -e 's/mtdparts=spi_flash://' -e 's/,/ /g'`
	do
		name=`echo ${p} | sed 's/[^(]*(\([^ ]*\))/\1/g'`
		p2=`echo ${p} | sed 's/[^@0-9]*//g'`
		size=`echo ${p2} | awk -F@ '{print $1}'`
		offs=`echo ${p2} | awk -F@ '{print $2}'`

		if [ "$name" == "uboot_env" ]; then
			# carve 'uboot_env'
			mtdparts="${mtdparts}${new_size_env}k($name),"
			new_off_bak=$(($offset + $new_size_env))
		elif [ -n "$offs" ]; then
			# no need change
			mtdparts="${mtdparts}${p},"
			offset=$offs
		elif [ "$name_prev" == "uboot_env" ]; then
			# specify offset for next to 'uboot_env'
			size=$((0x`grep "\"$name\"" $mtdfile 2>/dev/null | awk '{print $2}'` / 1024))
			mtdparts="${mtdparts}${size}k@${offset}k($name),"
		elif [ -z "$size" ]; then
			# specify size for full-fill last partition '-(data)'
			size=$((0x`grep "\"$name\"" $mtdfile 2>/dev/null | awk '{print $2}'` / 1024))
			mtdparts="${mtdparts}${size}k($name),"
		else
			# no need change
			mtdparts="${mtdparts}${p},"
		fi

		offset=$(($offset + $size))
		name_prev=$name
	done

	echo "mtdparts=${mtdparts}${new_size_bak}k@${new_off_bak}k(uboot_env_bak)"
	exit 0
fi

# mtdparts is not present in /proc/cmdline. Create the u-boot variable for kernel use
# The partitions are assumed to be contiguous.
sizes=`sed '1 d' $mtdfile 2>/dev/null | awk '{print $2}'`
mtdparts="spi_flash:"
i=0
offset=0
for s in ${sizes}; do
	s=$((0x${s} / 1024))
	name=`grep "mtd$i" $mtdfile 2>/dev/null | cut -d'"' -f2`
	if [ -z "$name" ]; then
		exit 5
	fi

	if [ "$name" == "uboot_env" ]; then
	# carve 'uboot_env'
		new_off_bak=$(($offset + $new_size_env))
		mtdparts="${mtdparts}${new_size_env}k($name),"
	elif [ "${name_prev}" == "uboot_env" ]; then
	# specify offset for next to 'uboot_env'
		mtdparts="${mtdparts}${s}k@${offset}k($name),"
	else
	# only size is enough
		mtdparts="${mtdparts}${s}k($name),"
	fi

	offset=$(($offset + ${s}))
	i=$(($i + 1))
	name_prev=$name
done

echo "mtdparts=${mtdparts}${new_size_bak}k@${new_off_bak}k(uboot_env_bak)"
