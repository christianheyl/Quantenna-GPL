#! /bin/sh

base_scripts="/scripts"

. $base_scripts/start-helpers.sh

script=`basename $0`

show_help()
{
	echo "Usage:"
	echo "      $script <cmd> [peer=<mac address>] [channel=<channel>] [wds_key=<key>] [bw=<bw>]"
	echo "      Available <cmd> are: START-AP-RBS, START-STA-RBS,"
	echo "            MBS-CREATE-WDS-LINK, RBS-CREATE-WDS-LINK,"
	echo "	          MBS-REMOVE-WDS-LINK, RBS-REMOVE-WDS-LINK, REMOVE-WDS-LINK"
	echo "            and RBS-SET-CHANNEL"
	exit 1
}

get_verbose()
{
	verbose=`call_qcsapi -u get_extender_status wifi0 | grep verbose`
	verbose=`echo $verbose | awk '{print $2}'`
}

get_default_channel()
{
	if [ $channel -lt 36 ]; then
		echo 6
	else
		echo 36
	fi
}

verify_wds_mode()
{
	repeater_mode=`call_qcsapi -u verify_repeater_mode`
	if [ $repeater_mode -eq 1 ]; then
		echo "Error: WDS mode is not supported in repeater mode"
		exit 1
	fi
	curr_wds_mode=`call_qcsapi -u get_extender_status wifi0 | grep role`
	curr_wds_mode=`echo $curr_wds_mode | awk '{print $2}'`
	if [ $curr_wds_mode'x' == $wds_mode'x' ] ; then
		return 0
	else
		echo "Error: WDS mode is invalid, current mode is $curr_wds_mode"
		exit 1
	fi
}

get_sta_credential_params()
{
	wifi_mode=`call_qcsapi -u get_mode wifi0`
	if [ $wifi_mode'X' != "Station"'X' ]; then
		return
	fi

	bss_ssid=`call_qcsapi -u get_SSID wifi0`

	if [ $? -ne 0 ] ; then
		echo "$script : API get_SSID failed" > /dev/console
		return
	elif [ "$bss_ssid"'X' == 'X' ] ; then
		# not associated by accident? - unlikely
		echo "$script : get SSID with empty character" > /dev/console
		return
	fi

	if [ $verbose'X' == "2"'X' ]; then
		echo "$script : bss_ssid is $bss_ssid" > /dev/console
	fi

	bss_auth_mode=`call_qcsapi -u SSID_get_authentication_mode wifi0 $bss_ssid`
	if [ $? -ne 0 ] ; then
		echo "$script : API SSID_get_authentication_mode failed" > /dev/console
		return
	fi

	if [ $bss_auth_mode'X' == "NONE"'X' ] ; then
		if [ $verbose'X' == "2"'X' ]; then
			echo "$script : STA configured to security off mode" > /dev/console
		fi
		return
	fi

	bss_encryp_mode=`call_qcsapi -u SSID_get_encryption_modes wifi0 $bss_ssid`
	bss_passphrase=`call_qcsapi -u SSID_get_passphrase wifi0 $bss_ssid 0`
	bss_psk=`call_qcsapi -u SSID_get_pre_shared_key wifi0 $bss_ssid 0`
	bss_protocol=`call_qcsapi -u SSID_get_proto wifi0 $bss_ssid`

	if [ $verbose'X' == "2"'X' ]; then
		echo "$script : credential parameters are" > /dev/console
		echo "$script : bss_encryp_mode: $bss_encryp_mode" > /dev/console
		echo "$script : bss_passphrase : $bss_passphrase" > /dev/console
		echo "$script : bss_psk        : $bss_psk" > /dev/console
		echo "$script : bss_protocol   : $bss_protocol" > /dev/console
	fi
}

set_up_bss()
{
	# update credentail to same with MBS
	wifi_mode=`call_qcsapi -u get_mode wifi0`
	if [ "$wifi_mode" != "Access point" ]; then
		return
	fi

	if [ "$bss_ssid"'X' == 'X' ] ; then
		return
	fi

	if [ $verbose'X' == "2"'X' ]; then
		echo "$script : set_up_bss: bss_ssid is $bss_ssid" > /dev/console
	fi
	call_qcsapi -u set_SSID wifi0 $bss_ssid

	# note that in AP mode, API set_WPA_authentication_mode only supports
	#      PSKAuthentication and EAPAuthentication
	# use API set_beacon to configure AP with auth mode as no security
	if [ $bss_auth_mode'X' == "NONE"'X' ] ; then
		call_qcsapi -u set_beacon wifi0 Basic
		return
	fi

	call_qcsapi -u set_beacon wifi0 $bss_protocol
	call_qcsapi -u set_WPA_authentication_mode wifi0 $ssid $bss_auth_mode
	call_qcsapi -u set_WPA_encryption_modes wifi0 $bss_encryp_mode
	if [ $bss_passphrase'X' != "X" ]; then
		call_qcsapi -u set_passphrase wifi0 0 $bss_passphrase
	elif [ $bss_psk'X' != "X" ]; then
		call_qcsapi -u set_PSK wifi0 0 $bss_psk
	fi
}

start_ap_rbs()
{
	verify_wds_mode

	if [ $wds_key == "NULL" ]; then
		security_on=""
	else
		security_on="encrypt"
	fi

	get_sta_credential_params
	# For 'reload_in_mode', use of '-u' is forbidden since it may cause
	# new 'reload_in_mode' is triggered before the previous one finishes
	call_qcsapi reload_in_mode wifi0 ap
	# Set a default non DFS channel first to
	# ensure that the following cancel_scan works well
	call_qcsapi -u set_channel wifi0 $(get_default_channel)
	set_up_bss

	call_qcsapi -u set_bw wifi0 $bw
	# Cancel scan to avoid interrupting CAC
	call_qcsapi -u cancel_scan wifi0 force
	sleep 1
	call_qcsapi -u set_channel wifi0 $channel
	call_qcsapi -u wds_add_peer wifi0 $peer_mac $security_on
	if [ $security_on'X' != 'X' ]; then
		call_qcsapi -u wds_set_psk wifi0 $peer_mac $wds_key
	fi
	call_qcsapi -u wds_set_mode wifi0 $peer_mac rbs
}

start_sta_rbs()
{
	verify_wds_mode
	wifi_mode=`get_wifi_config wifi0 mode`
	if [ $wifi_mode"X" == "sta""X" ] ; then
		call_qcsapi -u wds_remove_peer wifi0 $peer_mac
		call_qcsapi reload_in_mode wifi0 sta
	fi
}

create_wds_link()
{
	verify_wds_mode

	if [ $wds_key == "NULL" ]; then
		security_on=""
	else
		security_on="encrypt"
	fi
	call_qcsapi -u wds_add_peer wifi0 $peer_mac $security_on
	if [ $security_on'X' != 'X' ]; then
		call_qcsapi -u wds_set_psk wifi0 $peer_mac $wds_key
	fi
}

mbs_create_wds_link()
{
	create_wds_link
	call_qcsapi -u wds_set_mode wifi0 $peer_mac mbs
}

rbs_create_wds_link()
{
	call_qcsapi -u set_channel wifi0 $channel
	create_wds_link
	call_qcsapi -u wds_set_mode wifi0 $peer_mac rbs
}

rbs_remove_wds_link()
{
	verify_wds_mode
	wifi_mode=`get_wifi_config wifi0 mode`
	if [ $wifi_mode"X" == "sta""X" ] ; then
		call_qcsapi -u reload_in_mode wifi0 sta
	else
		call_qcsapi -u wds_remove_peer wifi0 $peer_mac
	fi
}

mbs_remove_wds_link()
{
	verify_wds_mode
	call_qcsapi -u wds_remove_peer wifi0 $peer_mac
}

get_verbose
if [ $verbose'X' == "2"'X' ]; then
	echo "cmd: $script $*" > /dev/console
fi

for temp in $*
do
	case $temp in
	peer=*)
		peer_mac=`echo $temp | cut -d '=' -f2`
	;;
	wds_key=*)
		wds_key=`echo $temp | cut -d '=' -f2`
	;;
	channel=*)
		channel=`echo $temp | cut -d '=' -f2`
	;;
	bw=*)
		bw=`echo $temp | cut -d '=' -f2`
	esac
done

if [ $1'X' == "START-AP-RBS"'X' ]; then
	if [ $peer_mac'X' == 'X' -o $channel'X' == 'X' -o \
			$wds_key'X' == 'X' ]; then
		show_help
	fi
	wds_mode=RBS
	start_ap_rbs
elif [ $1'X' == "START-STA-RBS"'X' ]; then
	wds_mode=RBS
	start_sta_rbs
elif [ $1'X' == "MBS-CREATE-WDS-LINK"'X' ]; then
	if [ $peer_mac'X' == 'X' -o $wds_key'X' == 'X' ]; then
		show_help
	fi
	wds_mode=MBS
	mbs_create_wds_link
elif [ $1'X' == "RBS-CREATE-WDS-LINK"'X' ]; then
	if [ $peer_mac'X' == 'X' -o $channel'X' == 'X' -o \
			$wds_key'X' == 'X' ]; then
		show_help
	fi

	wifi_mode=`get_wifi_config wifi0 mode`
	if [ $wifi_mode"X" == "sta""X" ] ; then
		if [ $verbose'X' == "2"'X' ]; then
			echo "$script : Event $1 ignored by RBS (STA mode)" > /dev/console
		fi
		exit
	fi

	wds_mode=RBS
	rbs_create_wds_link
elif [ $1'X' == "MBS-REMOVE-WDS-LINK"'X' ]; then
	if [ $peer_mac'X' == 'X' ]; then
		show_help
	fi
	wds_mode=MBS
	mbs_remove_wds_link
elif [ $1'X' == "RBS-REMOVE-WDS-LINK"'X' ]; then
	if [ $peer_mac'X' == 'X' ]; then
		show_help
	fi
	wds_mode=RBS
	rbs_remove_wds_link
elif [ $1'X' == "RBS-SET-CHANNEL"'X' ]; then
	if [ $channel'X' == 'X' ]; then
		show_chelp
	fi
	call_qcsapi -u set_channel wifi0 $channel
elif [ $1'X' == "REMOVE-WDS-LINK"'X' ]; then
	if [ $peer_mac'X' == 'X' ]; then
		show_help
	fi
	call_qcsapi -u wds_remove_peer wifi0 $peer_mac
else
	show_help
fi
