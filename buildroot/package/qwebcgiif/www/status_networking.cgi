#!/usr/bin/haserl
Content-type: text/html

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
	<title>Quantenna Communications</title>
	<link rel="stylesheet" type="text/css" href="/themes/style.css" media="screen" />

	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<meta http-equiv="expires" content="0" />
	<meta http-equiv="CACHE-CONTROL" content="no-cache" />
</head>
<script language="javascript" type="text/javascript" src="./js/cookiecontrol.js"></script>
<script language="javascript" type="text/javascript" src="./js/menu.js"></script>
<script language="javascript" type="text/javascript" src="./js/common_js.js"></script>

<script language="javascript" type="text/javascript">
var privilege=get_privilege(2);
</script>

<%
source ./common_sh.sh

curr_mode="`call_qcsapi get_mode wifi0`"
curr_version=`call_qcsapi get_firmware_version`
staticip_status=""
ip_br0=""
mac_br0=""
mac_pcie=""
mac_wifi0=""
netmask_br0=""
is_pcie="`ifconfig | grep pcie`"

func_get_value(){
	func_wr_net_status br0 ip
	ip_br0=$rval_func_wr_net_status

	func_wr_net_status br0 mask
	netmask_br0=$rval_func_wr_net_status

	if [ "$is_pcie" = "" ]
	then
		is_pcie=0
		mac_br0=`call_qcsapi get_mac_addr br0`
	else
		is_pcie=1
		mac_pcie=`call_qcsapi get_mac_addr pcie0`
	fi

	mac_wifi0=`call_qcsapi get_mac_addr wifi0`

	bssid=`call_qcsapi get_bssid wifi0`
}

func_get_value
%>

<script type="text/javascript">

function load_value()
{
	init_menu();
	var pcie='<% echo -n $is_pcie %>';
	if (pcie==0)
	{
		set_visible('tr_ethernetmac',true);
		set_visible('tr_pciemac',false);
	}
	else
	{
		set_visible('tr_ethernetmac',false);
		set_visible('tr_pciemac',true);
	}
}

function reload()
{
	window.location.href="status_networking.cgi";
}

</script>

<body class="body" onload="load_value();">
	<div class="top">
		<script type="text/javascript">
			createTop('<% echo -n $curr_version %>','<% echo -n $curr_mode %>');
		</script>
	</div>
	<div class="container">
		<div class="left">
			<script type="text/javascript">
				createMenu('<% echo -n $curr_mode %>',privilege);
			</script>
		</div>
		<div class="right">
			<div class="righttop">STATUS - NETWORKING</div>
			<div class="rightmain">
				<table class="tablemain">
					<tr>
						<td width="40%">IP Address:</td>
						<td width="60%"><% echo -n $ip_br0 %></td>
					</tr>
					<tr>
						<td>Netmask:</td>
						<td><% echo -n $netmask_br0 %></td>
					</tr>
					<tr id="tr_ethernetmac">
						<td>Ethernet MAC Address:</td>
						<td><% echo -n $mac_br0 %></td>
					</tr>
					<tr id="tr_pciemac">
						<td>PCIE MAC Address:</td>
						<td><% echo -n $mac_pcie %></td>
					</tr>
					<tr>
						<td>Wireless MAC Address:</td>
						<td><% echo -n $mac_wifi0 %></td>
					</tr>
					<tr>
						<td>BSSID:</td>
						<td><% echo -n $bssid %></td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
				</table>
				<div class="rightbottom">
					<button name="btn_refresh" id="btn_refresh" type="button" onclick="reload();"  class="button">Refresh</button>
				</div>
			</div>
		</div>
	</div>
<div class="bottom">
<tr>
	<script type="text/javascript">
	createBot();
	</script>
</tr>
</div>

</body>
</html>

