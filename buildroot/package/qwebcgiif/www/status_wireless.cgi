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
<script type="text/javascript">

function reload()
{
	window.location.href="status_wireless.cgi";
}

function validate()
{
	popnew("assoc_table.cgi?id=0");
}

</script>
<%
curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`
curr_ch=`call_qcsapi get_channel wifi0`

read_bssid()
{
	curr_bssid=`call_qcsapi get_BSSID wifi0`
	if [ "$curr_bssid" = "00:00:00:00:00:00" ]
	then
		curr_bssid="Not Associated"
	fi
}

read_assoc()
{
	if [ "$curr_mode" = "Access point" ]
	then
		curr_assoc=`call_qcsapi get_count_assoc wifi0`
	else
		tmp=`call_qcsapi get_bssid wifi0`
		if [ "$tmp" = "00:00:00:00:00:00" ]
		then
			curr_assoc="Not Associated"
		else
			curr_assoc="Associated"
		fi
	fi
}

read_rssi()
{
	if [ "$curr_mode" = "Station" ]
	then
		curr_rssi=`call_qcsapi get_rssi_dbm wifi0 0`
		tmp=`echo -n $curr_rssi | grep error`
		if [ -n "$tmp" ]
		then
			curr_rssi="Not Associated"
		fi
	fi
}

read_bssid
read_assoc
read_rssi
%>

<body class="body" onload="init_menu();">
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
			<div class="righttop">STATUS - WIRELESS</div>
			<div class="rightmain">
				<table class="tablemain">
					<tr>
						<td width="40%">Device Mode:</td>
						<td width="60%"><% echo -n $curr_mode %></td>
					</tr>
					<tr>
						<td>Bandwidth:</td>
						<td><% call_qcsapi get_bw wifi0 %>MHz</td>
					</tr>
					<tr>
						<td>AP Mac Address (BSSID):</td>
						<td><% echo -n $curr_bssid %></td>
					</tr>
					<tr>
						<td>Channel:</td>
						<td><% echo -n $curr_ch %></td>
					</tr>
					<tr>
						<td>
						<%
						if [ "$curr_mode" = "Station" ]
						then
							echo "Association Status:"
						else
							echo "Associated Devices Count:"
						fi
						%>
						</td>
						<td><% echo -n $curr_assoc %>&nbsp;&nbsp;<input type="button" name="btn_assoc_table" id="btn_assoc_table" value="Association Table" class="button" style="width:150px;" onclick="validate();"/></td>
					</tr>
					<tr<%
						if [ "$curr_mode" = "Access point" ]
						then
							echo " style=\"display: none;\""
						fi
						%>>
						<td>RSSI:</td>
						<td><% echo -n $curr_rssi %></td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
					<tr>
						<td>Packets Received Successfully:</td>
						<td><% call_qcsapi get_counter64 wifi0 RX_packets %></td>
					</tr>
					<tr>
						<td>Bytes Received:</td>
						<td><% call_qcsapi get_counter64 wifi0 RX_bytes %></td>
					</tr>
					<tr>
						<td>Packets Transmitted Successfully:</td>
						<td><% call_qcsapi get_counter64 wifi0 TX_packets %></td>
					</tr>
					<tr>
						<td>Bytes Transmitted:</td>
						<td><% call_qcsapi get_counter64 wifi0 TX_bytes %></td>
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

