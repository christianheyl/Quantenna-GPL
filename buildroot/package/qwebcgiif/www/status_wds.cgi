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
	window.location.href="status_wds.cgi";
}

</script>

<%
curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`

if [ -n "$curr_mode" -a "$curr_mode" = "Station" ]
then
	echo "<script langauge=\"javascript\">alert(\"Don\`t support in the Station mode.\");</script>"
	echo "<script language='javascript'>location.href='status_device.cgi'</script>"
	return
fi

wds0=`call_qcsapi wds_get_peer_address wifi0 0`
if [ $? -ne 0 ]
then
	wds0="N/A"
else
	signal_0=`call_qcsapi get_rssi_dbm wds0 0`
fi

wds1=`call_qcsapi wds_get_peer_address wifi0 1`
if [ $? -ne 0 ]
then
	wds1="N/A"
else
	signal_1=`call_qcsapi get_rssi_dbm wds1 0`
fi

wds2=`call_qcsapi wds_get_peer_address wifi0 2`
if [ $? -ne 0 ]
then
	wds2="N/A"
else
	signal_2=`call_qcsapi get_rssi_dbm wds2 0`
fi

wds3=`call_qcsapi wds_get_peer_address wifi0 3`
if [ $? -ne 0 ]
then
	wds3="N/A"
else
	signal_3=`call_qcsapi get_rssi_dbm wds3 0`
fi

wds4=`call_qcsapi wds_get_peer_address wifi0 4`
if [ $? -ne 0 ]
then
	wds4="N/A"
else
	signal_4=`call_qcsapi get_rssi_dbm wds4 0`
fi

wds5=`call_qcsapi wds_get_peer_address wifi0 5`
if [ $? -ne 0 ]
then
	wds5="N/A"
else
	signal_5=`call_qcsapi get_rssi_dbm wds5 0`
fi

wds6=`call_qcsapi wds_get_peer_address wifi0 6`
if [ $? -ne 0 ]
then
	wds6="N/A"
else
	signal_6=`call_qcsapi get_rssi_dbm wds6 0`
fi

wds7=`call_qcsapi wds_get_peer_address wifi0 7`
if [ $? -ne 0 ]
then
	wds7="N/A"
else
	signal_7=`call_qcsapi get_rssi_dbm wds7 0`
fi

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
			<div class="righttop">STATUS - WDS</div>
			<div class="rightmain">
				<table class="tablemain">
					<tr>
						<td width="10%">WDS</td>
						<td width="30%">MAC Address</td>
						<td width="60%">RSSI(dBm)</td>
					</tr>
					<tr<% if [ "$wds0" = "N/A" ]; then echo " style=\"display: none;\""; fi %>>
						<td>WDS0: </td>
						<td><% echo -n $wds0 %></td>
						<td><% echo -n $signal_0 %></td>
					</tr>
					<tr<% if [ "$wds1" = "N/A" ]; then echo " style=\"display: none;\""; fi %>>
						<td>WDS1: </td>
						<td><% echo -n $wds1 %></td>
						<td><% echo -n $signal_1 %></td>
					</tr>
					<tr<% if [ "$wds2" = "N/A" ]; then echo " style=\"display: none;\""; fi %>>
						<td>WDS2: </td>
						<td><% echo -n $wds2 %></td>
						<td><% echo -n $signal_2 %></td>
					</tr>
					<tr<% if [ "$wds3" = "N/A" ]; then echo " style=\"display: none;\""; fi %>>
						<td>WDS3: </td>
						<td><% echo -n $wds3 %></td>
						<td><% echo -n $signal_3 %></td>
					</tr>
					<tr<% if [ "$wds4" = "N/A" ]; then echo " style=\"display: none;\""; fi %>>
						<td>WDS4: </td>
						<td><% echo -n $wds4 %></td>
						<td><% echo -n $signal_4 %></td>
					</tr>
					<tr<% if [ "$wds5" = "N/A" ]; then echo " style=\"display: none;\""; fi %>>
						<td>WDS5: </td>
						<td><% echo -n $wds5 %></td>
						<td><% echo -n $signal_5 %></td>
					</tr>
					<tr<% if [ "$wds6" = "N/A" ]; then echo " style=\"display: none;\""; fi %>>
						<td>WDS6: </td>
						<td><% echo -n $wds6 %></td>
						<td><% echo -n $signal_6 %></td>
					</tr>
					<tr<% if [ "$wds7" = "N/A" ]; then echo " style=\"display: none;\""; fi %>>
						<td>WDS7: </td>
						<td><% echo -n $wds7 %></td>
						<td><% echo -n $signal_7 %></td>
					</tr>
					<tr>
						<td class="divline" colspan="3";></td>
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

