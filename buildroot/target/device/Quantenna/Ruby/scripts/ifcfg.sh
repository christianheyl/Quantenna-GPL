#!/bin/sh
#
ifcfg_bakfile="/mnt/jffs2/interfaces.bak"
ipaddr_file="/mnt/jffs2/ipaddr"

ifcfg_addr_get() {
	if_name=$1
	if [ -f $ifcfg_bakfile ] ; then
		grep -A1 $if_name $ifcfg_bakfile | awk '/address/ { print $2 }'
	elif [ -f $ipaddr_file ] ; then
		cat $ipaddr_file | awk '{ print $1 }'
	else
		cat /etc/ipaddr | awk '{ print $1 }'
	fi
}

ifcfg_addr_set() {
	if_name=$1
	ipaddr=$2
	line=`grep -n -A2 $if_name $ifcfg_bakfile | awk '/address/ {print $1}' | grep -o -e '^[0-9]*'`

        if [ "$line" != "" ] ; then
		sed -i "$line d" $ifcfg_bakfile
		sed -i "$line i\        address $ipaddr" $ifcfg_bakfile
	fi
}

ifcfg_netmask_get() {
	if_name=$1
	if [ -f $ifcfg_bakfile ]
	then
		grep -A2 $if_name $ifcfg_bakfile | awk '/netmask/ { print $2 }'
	elif [ -f $ipaddr_file ]
	then
		cat $ipaddr_file | awk '{ print $3 }'
	fi
}

ifcfg_netmask_set() {
	if_name=$1
	netmask=$2
	line=`grep -n -A2 $if_name $ifcfg_bakfile | awk '/netmask/ {print $1}' | grep -o -e '^[0-9]*'`

        if [ "$line" != "" ] ; then
		sed -i "$line d" $ifcfg_bakfile
		sed -i "$line i\        netmask $netmask" $ifcfg_bakfile
	fi
}

ifcfg_bakfile_getname() {
	echo "$ifcfg_bakfile"
}

param=$1
shift
if [ "$param" == "get_ipaddr" ] ; then
	if_name=$1

	ifcfg_addr_get $if_name
elif [ "$param" == "get_netmask" ] ; then
	if_name=$1

	ifcfg_netmask_get $if_name
elif [ "$param" == "set_ipaddr" ] ; then
	ifcfg_addr_set $1 $2
elif [ "$param" == "set_netmask" ] ; then
	ifcfg_netmask_set $1 $2
elif [ "$param" == "get_bakfile" ] ; then
	ifcfg_bakfile_getname
else
	echo "$ifcfg_bakfile"
fi

