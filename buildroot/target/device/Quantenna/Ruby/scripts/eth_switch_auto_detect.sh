#!/bin/sh

emac1_check_for_auto_detect() {
	emac1_continuous_yes=0
	emac1_yes=0
	emac1_no=0
	count=0
	while [ $count -lt $emac1_test_count ]
	do
		sleep 1
		count=$((count+1))
		port_connected=`call_qcsapi -u get_eth_info $eth1_port link | grep yes`
		if [ "$port_connected" != "" ]; then
			emac1_yes=$((emac1_yes+1))
			emac1_continuous_yes=$((emac1_continuous_yes+1))

			if [ $emac1_continuous_yes -ge $((emac1_test_count/6)) ]; then
				break;
			elif [ $emac1_yes -ge $((emac1_test_count/2+1)) ]; then
				break;
			fi
		else
			emac1_continuous_yes=0
			emac1_no=$((emac1_no+1))
			if [ $emac1_no -ge $((emac1_test_count/2+1)) ]; then
				break;
			fi
		fi
	done
	echo "emac1_continuous_yes is $emac1_continuous_yes, emac1_yes is $emac1_yes, emac1_no is $emac1_no"
}

dhcp_request_for_auto_detect() {
	dhclient -4 -o $eth1_port &

	dhcp_response=0
	count=0
	while [ $count -lt $auto_detect_wait ]
	do
		sleep 1
		count=$((count+1))
		if [ -f /var/lib/dhcp/query_output ]; then
			dhcp_response=1
			break
		fi
	done
	echo "count is $count"

	if [ $dhcp_response == 0 ]; then
		echo "emac1 up but not active"
		echo "102" > /var/lib/dhcp/active_port
		killall dhclient
	else
		echo "emac1 up and active"
		echo "101" > /var/lib/dhcp/active_port
	fi
	rm -f /var/lib/dhcp/dhclient.leases
	rm -f /var/lib/dhcp/dhclient.pid
}

eth_switch_auto_detect() {
	#STB auto detect active eth/wifi port
	if [ -f /sbin/qserv ]; then
		if [ -f /mnt/jffs2/switch_mode ]; then
			active_port=`cat /mnt/jffs2/switch_mode`
			if [ "$active_port" != "auto" -a "$active_port" != "eth" -a "$active_port" != "wifi" ]; then
				active_port="auto"
				echo "$active_port" > /mnt/jffs2/switch_mode
			fi
		else
			active_port="auto"
			echo "$active_port" > /mnt/jffs2/switch_mode
		fi
		echo "active port is $active_port"
		if [ "$active_port" == "auto" ]; then
			auto_detect_wait=`get_bootval auto_detect_wait`
			if [ "$auto_detect_wait" == "0" ]; then
				auto_detect_wait=60
			fi
			echo "auto detect wait is $auto_detect_wait"

			emac1_test_count=`get_bootval emac1_test_count`
			if [ "$emac1_test_count" == "0" ]; then
				emac1_test_count=30
			fi
			echo "emac1 test count is $emac1_test_count"

			emac_swap=`get_bootval emac_swap`
			if [ "$emac_swap" == "1" ] ; then
				eth1_port="eth1_1"
			else
				eth1_port="eth1_0"
			fi

			emac1_check_for_auto_detect
			if [ $emac1_continuous_yes -ge $((emac1_test_count/6)) ]; then
				dhcp_request_for_auto_detect
			elif [ $emac1_yes -ge $((emac1_test_count/2+1)) ]; then
				dhcp_request_for_auto_detect
			else
				echo "emac1 down"
				echo "102" > /var/lib/dhcp/active_port
			fi
		elif [ "$active_port" == "wifi" ]; then
			echo "202" > /var/lib/dhcp/active_port
		elif [ "$active_port" == "eth" ]; then
			echo "301" > /var/lib/dhcp/active_port
		fi
		active_port=`cat /var/lib/dhcp/active_port`
		emac_swap=`get_bootval emac_swap`
		/scripts/cmdloop /sbin/qserv -i eth1_0 -p $active_port -e $emac_swap &
	fi
}
