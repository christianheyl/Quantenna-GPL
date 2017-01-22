#!/bin/sh

confirm()
{
	echo "<script langauge=\"javascript\">alert(\"$1\");</script>";
}


func_wr_wireless_conf(){
wireless_conf_line=`cat /mnt/jffs2/wireless_conf.txt`
if [ $# = 1 ]
then
	rval_func_wr_wireless_conf=`echo "$wireless_conf_line" | sed 's/\&/\n/g' | grep "$1=" | cut -d '=' -f 2`
elif [ $# = 2 ]
then
	test_for_exist=`echo "$wireless_conf_line"| sed -n -e "/$1=/p"`
	if [ -z $test_for_exist ]
	then
		rval_func_wr_wireless_conf=`echo "$wireless_conf_line" | sed -e "s/mode=.*/&\&$1=$2/g" > /mnt/jffs2/wireless_conf.txt`
	else
		rval_func_wr_wireless_conf=`echo "$wireless_conf_line" | sed "s/$1=[^&]*/$1=$2/g" > /mnt/jffs2/wireless_conf.txt`
	fi
fi
}

func_wr_net_status(){
if [ $# = 3 ]
then
	ifconfig $1 $2 netmask $3
elif [ $# = 2 ]
then
	output_ifconfig=`ifconfig $1`
	if [ "$2" = "mac" ]
	then
		rval_func_wr_net_status=`echo "$output_ifconfig" | grep HWaddr | awk '{print $5}'`
	elif [ "$2" = "ip" ]
	then
		rval_func_wr_net_status=`echo "$output_ifconfig" | grep Mask | awk '{print $2}'|awk -F ':' '{print $2}'`
	elif [ "$2" = "mask" ]
	then
		rval_func_wr_net_status=`echo "$output_ifconfig" | grep Mask | awk '{print $4}'|awk -F ':' '{print $2}'`
	else
		rval_func_wr_net_status=`echo "parameter invalid"`
	fi
elif [ $# = 1 ]
then
	rval_func_wr_net_status=`ifconfig $1`
else
	rval_func_wr_net_status=`ifconfig`
fi
}