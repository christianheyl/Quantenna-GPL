#!/bin/sh

base_persistent_files="/mnt/jffs2"
base_default_conf_file="/etc"

. $base_scripts/build_config

stateless=`grep -o 'stateless=[0-9]*' /proc/cmdline | cut -d= -f2`

if [ "${STATELESS}" = "" ] && [ "$stateless" = "1" ]; then
	STATELESS=y
	export STATELESS
fi

wps_push_button_gpio=4
reset_device_gpio=5
rfenable_gpio=12

if [ -e /proc/amber ]
then
	wps_push_button_gpio=1
	rfenable_gpio=2
fi

default_ipaddr_ap=1.1.1.1
default_ipaddr_sta=1.1.1.2

#This option is only used for Topaz PCIe EP boards
en_tqe_sem=1

get_configuration_ip() {
	get_bootval ipaddr
}

echo_with_logging()
{
    logger $@
    echo $@
}

hex2dec()
{
    hexval=0x$(echo $1 | sed 's%0x%%')
    let decval=$hexval
    echo $decval
}

create_wireless_conf()
{
    echo "mode=ap&bw=40&region=none&channel=0&bf=1&pwr=19" >${base_persistent_files}/wireless_conf.txt
    echo_with_logging "Creating wireless configuration file ${base_persistent_files}/wireless_conf.txt"
}

list_contains()
{
	item=$1
	shift
	for i in $@
	do
		if [ "$i" = "$item" ]
		then
			return 0
		fi
	done
	return 1
}

get_hw_config_id()
{
	get_board_config board_id
}

hw_config_id_in_list()
{
	hw_config_id=$(get_hw_config_id)
	if [ $? -eq 0 ] ; then
		if list_contains $hw_config_id $@ ; then
			return 0
		else
			return 1
		fi
	else
		echo Error getting hw_config_id
	fi
	return 1
}

get_security_path()
{
	security_file_path=`call_qcsapi -u get_file_path security`
	error_check=`echo $security_file_path | cut -b 1-13`
	if [ "$error_check" = "QCS API error" ]; then
		echo_with_logging "Cannot get path to hostapd.conf and wpa_supplicant.conf."
		echo_with_logging "Using default of ${base_persistent_files}, but the web-base GUI likely will fail."
		security_file_path=${base_persistent_files}
	fi
}

check_default_security_files()
{
	wifi_mode=$1

	if [ "$wifi_mode" = "ap" ]; then
		security_config_file=${security_file_path}/hostapd.conf
		if [ ! -f ${security_config_file} ]; then
			cp $base_scripts/hostapd.conf ${security_config_file}
		fi
	else
		security_config_file=${security_file_path}/wpa_supplicant.conf
		if [ ! -f ${security_config_file} ]; then
			cp $base_scripts/wpa_supplicant.conf ${security_config_file}
		fi

		if [ ! -f ${security_file_path}/wpa_supplicant.conf.pp ]
		then
			touch ${security_file_path}/wpa_supplicant.conf.pp
		fi
	fi

}

start_security_daemon()
{
	wifi_mode=$1
	wifi_repeater=$2

	get_security_path
	check_default_security_files "$wifi_mode"
	check_wps "$security_config_file" "$wifi_mode"

        # disable ap pin in all BSSes
        local_wps_ap_cfg=`get_wifi_config wifi0 wps_ap_cfg`
        if [ "$wifi_mode" = "ap" ]; then
               if [ "$local_wps_ap_cfg" = "0" ]; then
                       echo "Disable ap pin setup in all BSSes"
                       sed -i "s;\(ap_setup_locked\)=.*$;\1=1;g" $security_config_file
               else
                       echo "Enable ap pin setup in all BSSes"
                       sed -i "s;\(ap_setup_locked\)=.*$;\1=0;g" $security_config_file
               fi
        fi

	if [ "$wifi_mode" = "ap" ]; then
		if pidof hostapd > /dev/null; then
			return
		fi
		qlink=`get_wifi_config wifi0 qlink`
		if [ $? -ne 0 -o "$qlink" = "0" ]; then
			cmd="hostapd -B $security_config_file"
		elif [ "$qlink" = "1" -o "$qlink" = "auto"  ]; then
			cmd="/usr/sbin/hostapd-proxy/hostapd -B $security_config_file"
		else
			cmd="/usr/sbin/hostapd-proxy/hostapd -I $qlink -B $security_config_file"
		fi
	else
		if pidof wpa_supplicant > /dev/null; then
			return
		fi
		if [ "$wifi_repeater" = "1" ]; then
			cmd="wpa_supplicant -B -q -iwifi0 -bbr0 -Dmadwifi -c $security_config_file -r"
		else
			cmd="wpa_supplicant -B -q -iwifi0 -bbr0 -Dmadwifi -c $security_config_file"
		fi
	fi

	$base_scripts/launch start 0 $cmd
}

free_boot_memory()
{
    # Firmwares are loaded, and can't be reloaded.
    # Let's delete all firmwares as they occupy RAM (tmpfs).
    rm /etc/firmware/*.bin
}

ipaddr_process() {
	if [ -f /mnt/jffs2/ipaddr ]
	then
		ipaddress=`cat /mnt/jffs2/ipaddr`
	fi

	if [ -f /mnt/jffs2/netmask ]
	then
		netmask_addr=`cat /mnt/jffs2/netmask`
	fi

	if [ "$ipaddress" == "" ] ; then
		cat /proc/cmdline | grep 'ip=' | sed 's/\(.*\)ip=\(.*\)/\2/' | \
			awk '{ print $1 }' > /etc/ipaddr
		if [ -s /etc/ipaddr ]
		then
			ipaddress=`cat /etc/ipaddr`
		fi
	fi

	if [ "$ipaddress" == "" ] ; then
		get_bootval ipaddr > /etc/ipaddr
		if [ -s /etc/ipaddr ]
		then
			ipaddress=`cat /etc/ipaddr`
		fi
	fi

	if [ "$ipaddress" == "" ] ; then
		#assign a default IP address to br0 ,avoid that wireless interface can't be up
		wifi_mode=`/scripts/get_wifi_config wifi0 mode`
		if [ $wifi_mode == "ap" ] ; then
			ipaddress="1.1.1.1"
		else
			ipaddress="1.1.1.2"
		fi
	fi

	sed -i "s/192.168.0.10/$ipaddress/g" /etc/network/interfaces
	echo "Using IP address $ipaddress"

	if [ "$netmask_addr" != "" ] ; then
		sed -i "s/255.255.255.0/$netmask_addr/g" /etc/network/interfaces
		echo "Using netmask $netmask_addr"
	else
		echo "Netmask is not set"
	fi
}

wifi_macaddr_configure()
{
	mac0addr="0"

	if [ "${STATELESS}" = "y" ]
	then
		mac0addr=`${base_scripts}/get_bootval wifi_mac_addr`
	fi

	if [ "${mac0addr}" = "0" -a -f ${base_persistent_files}/wifi_mac_addrs ]
	then
		mac0addr=`cat ${base_persistent_files}/wifi_mac_addrs | head -1`
	fi

	if [ "${mac0addr}" = "0" ]
	then
		mac0addrlow=`dd if=/dev/urandom count=1 2>/dev/null | md5sum | \
			sed 's/^\(..\)\(..\)\(..\).*$/\1:\2:\3/'`
		mac0addr="00:26:86:${mac0addrlow}"
		echo "Warning: Setting randomized MAC address! " $mac0addr
		echo $mac0addr > ${base_persistent_files}/wifi_mac_addrs
	fi
}

is_2_4_ghz_mode()
{
	mode=$1
	case "$mode" in
	11ng | 11b | 11g)
		echo "1"
		return
		;;
	esac
	echo "0"
}
