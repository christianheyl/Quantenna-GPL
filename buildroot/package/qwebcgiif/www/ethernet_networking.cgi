#!/usr/bin/haserl
Content-type: text/html


<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
	<title>Quantenna Communications</title>
	<link rel="stylesheet" type="text/css" href="./themes/style.css" media="screen" />

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
	process_id=`ps | grep "dhclient -4" | grep -v "grep" | awk '{print $1}'`
	if [ -n "$process_id" ]
	then
		staticip_status=0
	else
		staticip_status=1
	fi

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

	mac_wifi0=`call_qcsapi get_macaddr wifi0`
	bssid=`call_qcsapi get_bssid wifi0`
}

func_set_value(){
	if [ -n "$POST_staticip_status" -a "$POST_staticip_status" != "$staticip_status" ]
	then
		func_wr_wireless_conf staticip $POST_staticip_status
		if [ "$POST_staticip_status" -eq "1" ]
		then
			if [ -n $process_id ]
			then
				kill $process_id
			fi
		else
			dhclient -4 br0 &
			confirm "Please use new ip address to login"
		fi
	fi

	if [ "$POST_ip_br0" != "$ip_br0" -o "$POST_netmask_br0" != "$netmask_br0" ]
	then
		ifconfig br0 $POST_ip_br0 netmask $POST_netmask_br0
		confirm "Please use new IP address to login"
	fi
}

func_get_value
if [ -n "$POST_action" ]
then
	func_set_value
	func_get_value
fi
%>

<script language="javascript" type="text/javascript">
function load_value()
{
	init_menu();
	set_control_value('txt_ip','<% echo -n $ip_br0 %>', 'text');
	set_control_value('txt_netmask','<% echo -n $netmask_br0 %>', 'text');
	set_control_value('txt_ethernetmac','<% echo -n $mac_br0 %>', 'text');
	set_control_value('txt_pciemac','<% echo -n $mac_pcie %>', 'text');
	set_control_value('txt_wirelessmac','<% echo -n $mac_wifi0 %>', 'text');
	set_control_value('txt_bssid','<% echo -n $bssid %>', 'text');
	set_control_value('ckb_staticip','<% echo -n $staticip_status %>', 'radio');
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

function modechange(obj)
{
	var tmp1=document.getElementById("action");
	if (obj.name == "ckb_staticip")
	{
		if(obj.value==0)
		{
			set_disabled("ipaddress",true);
			set_disabled("netmask",true);
		}
		else if (obj.value==1)
		{
			set_disabled("ipaddress",false);
			set_disabled("netmask",false);
		}
	}
}

function checkIP(ipaddr, netmask){
	var pattern = /^\d{1,3}(\.\d{1,3}){3}$/;
	if (!pattern.exec(ipaddr) || !pattern.exec(netmask)) {
		return false;
	}

	var aryIP = ipaddr.split('.');
	var aryMask = netmask.split('.');
	var preMask = 255;
	var is_net_allzero = 1;
	var is_net_allone = 1;
	var is_host_allzero = 1;
	var is_host_allone = 1;

	if (parseInt(aryIP[0]) >= 224 || parseInt(aryIP[0]) == 127) {
		return false;
	}

	for(key in aryIP)
	{
		if (parseInt(aryIP[key]) > 255 || parseInt(aryIP[key])< 0) {
			return false;
		}

		var curMask = parseInt(aryMask[key]);
		if (curMask > 255 || curMask < 0 || (preMask < 255 && curMask > 0)) {
			//Invalid netmask, out of range
			return false;
		}

		if (curMask < 255 && ((255 - curMask) & (256 - curMask))) {
			//Invalid netmask, against the rule of continouse 1s and then 0s
			return false;
		}
		preMask = curMask;

		if (curMask > 0) {
			//check net address, all 0s or all 1s are invalid
			var netaddr = parseInt(aryIP[key]) & curMask;
			if (is_net_allzero > 0 && netaddr > 0) {
				is_net_allzero = 0;
			}

			if (is_net_allone > 0 && netaddr < curMask) {
				is_net_allone = 0;
			}
		}
		if (curMask < 255) {
			//check host address, all 0s or all 1s are invalid
			var hostaddr = parseInt(aryIP[key]) & (255 - curMask);
			if (is_host_allzero > 0 && hostaddr > 0) {
				is_host_allzero = 0;
			}

			if (is_host_allone > 0 && hostaddr < (255 - curMask)) {
				is_host_allone = 0;
			}
		}
	}
	return !(is_net_allzero + is_net_allone + is_host_allzero + is_host_allone);
}

function validate()
{
		var cn=document.config_networking;
		var mf=document.mainform;

		var newIP = document.getElementById("txt_ip");
		var newMask = document.getElementById("txt_netmask");

		if(!checkIP(newIP.value, newMask.value)) {
			alert("Invalid ip or netmask");
			return;
		}
		if (mf.ckb_staticip[0].checked==true)
		{
			cn.staticip_status.value=0;
		}
		else if (mf.ckb_staticip[1].checked==true)
		{
			cn.staticip_status.value=1;
		}
		cn.ip_br0.value=mf.txt_ip.value;
		cn.netmask_br0.value=mf.txt_netmask.value;

		cn.submit();
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
			<div class="righttop">ETHERNET - NETWORKING</div>
			<div class="rightmain">
				<form name="mainform">
				<table class="tablemain">
					<tr>
						<td width="35%"></td>
						<td width="65%">DHCP:<input name="ckb_staticip" type="radio" value="0" onclick="modechange(this);"/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Static IP:<input name="ckb_staticip" type="radio" value="1" onclick="modechange(this);"/>
						</td>
					</tr>
					<tr>
						<td width="35%">IP Address:</td>
						<td width="65%">
							<input name="txt_ip" type="text" id="txt_ip"  class="textbox" />
						</td>
					</tr>
					<tr>
					<td>Netmask:</td>
						<td>
							<input name="txt_netmask" type="text" id="txt_netmask"  class="textbox" />
						</td>
					</tr>
					<tr id="tr_ethernetmac">
						<td>Ethernet MAC Address:</td>
						<td>
							<input name="txt_ethernetmac" type="text" id="txt_ethernetmac" class="textbox" disabled="disabled"/>
						</td>
					</tr>
					<tr id="tr_pciemac">
						<td>PCIE MAC Address:</td>
						<td>
							<input name="txt_pciemac" type="text" id="txt_pciemac" class="textbox" disabled="disabled"/>
						</td>
					</tr>
					<tr>
						<td>Wireless MAC Address:</td>
						<td>
							<input name="txt_wirelessmac" type="text" id="txt_wirelessmac" class="textbox" disabled="disabled"/>
						</td>
					</tr>
					<tr>
						<td>BSSID:</td>
						<td><input name="txt_bssid" type="text" id="txt_bssid" class="textbox" disabled="disabled"/></td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
				</table>
				<div class="rightbottom">
					<button name="btn_save" id="btn_save" type="button" onclick="validate();"  class="button">Save</button>
					<button name="btn_cancel" id="btn_cancel" type="button" onclick="load_value();" class="button">Cancel</button>
				</div>
				</form>
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

	<form name="config_networking" method="POST" action="ethernet_networking.cgi">
		<input type="hidden" name="action" value="action" />
		<input type="hidden" name="staticip_status" />
		<input type="hidden" name="ip_br0" />
		<input type="hidden" name="netmask_br0" />
	</form>


</body>
</html>

