#!/bin/sh

rc=`readmem e0000010 | awk '{print $NF}' | cut -dx -f2 | cut -b8`

if [ "$rc" = "4" ]
then
	flash_mount_point=`call_qcsapi get_file_path security`
	if [ ! -e "$flash_mount_point/soc_event_cntr" ]
	then
		res=`echo 1 > $flash_mount_point/soc_event_cntr`
	else
		swc=`cat $flash_mount_point/soc_event_cntr`
		let swc=$swc+1
		res=`echo $swc > $flash_mount_point/soc_event_cntr`
	fi
fi