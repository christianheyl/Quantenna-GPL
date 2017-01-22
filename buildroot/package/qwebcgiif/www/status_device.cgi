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

<script language="javascript" type="text/javascript">
var privilege=get_privilege(2);
</script>
<script type="text/javascript">

function reload()
{
	window.location.href="status_device.cgi";
}

</script>
<%
curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`

get_mode()
{
	mode=`call_qcsapi get_mode wifi0`
	if [ "$mode" = "Station" ]
	then
		echo "[&nbsp;] Access Point (AP)&nbsp;&nbsp;<b style=\"font-weight:bold\">[X] Station (STA) </b>"
	else
		echo "<b style=\"font-weight:bold\">[X] Access Point (AP)</b>&nbsp;&nbsp;[&nbsp;] Station (STA) "
	fi
}
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
		<div class="righttop">STATUS - DEVICE</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr>
					<td width="35%">Device Name:</td>
					<td width="65%">Quantenna Wireless Adapter</td>
				</tr>
				<tr>
					<td>Uptime:</td>
					<td><% uptime | awk -F\, '{print $1}' | awk '{print $3,$4}' %></td>
				</tr>
				<tr>
					<td>Device Mode:</td>
					<td><% get_mode %></td>
				</tr>
				<tr>
					<td>Software Version:</td>
					<td><% call_qcsapi get_firmware_version wifi0 %></td>
				</tr>
				<tr>
					<td>Hardware Version:</td>
					<td><% cat /proc/hw_revision %></td>
				</tr>
				<tr>
					<td>MAC Address:</td>
					<td><% call_qcsapi get_mac_addr br0 %></td>
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

