#!/usr/lib/cgi-bin/php-cgi

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
	<title>Quantenna Communications</title>
	<link rel="stylesheet" type="text/css" href="../themes/style.css" media="screen" />

	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<meta http-equiv="expires" content="0" />
	<meta http-equiv="CACHE-CONTROL" content="no-cache" />
</head>

<body class="body" onload="focus();">
	<div class="top">
		<a class="logo" href="../status_device.php">
			<img src="../images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="tools_logs.php" id="mainform" name="mainform" method="post">

<div style="border:6px solid #9FACB7; width:800px; height:auto; background-color:#fff;">
	<div class="righttop">Networking</div>
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%">DHCP/Static IP</td>
					<td width="70%">
					Choose the <b>DHCP</b>/<b>Static IP</b> by which the AP obtains its IP address:<br/>
						<b>DHCP</b> - If your network includes a DHCP server for dynamic allocation of IP addresses, choose this option if you want DHCP to assign an IP address and subnet mask to the AP. Depending on your router, the default gateway, primary DNS server, and secondary DNS server may also be assigned. The DHCP server must be configured to allocate static IP addresses based on MAC addresses so that the AP always receives the same address.<br/>
						<b>Static IP</b> - Choose this option if you want to manually enter an IP address, subnet mask, default gateway, and DNS server IP addresses for the AP.
				</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">IP Address</td>
					<td width="70%">
					If you configured the AP for a static IP address, enter that IP address.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Netmask</td>
					<td width="70%">
					If you configured the AP for a static IP address, enter the subnet mask for the AP. Use the same value that is configured for the PCs on your network.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Ethernet MAC Address</td>
					<td width="70%">
					MAC address of Ethernet interface on the device.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Wireless MAC Address</td>
					<td width="70%">
					MAC address of Wireless Wifi0 interface on the device.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">BSSID</td>
					<td width="70%">
					MAC address of Wireless broadcasting interface on the device.
				</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
		</table>
	</div>
</div>
</form>
</div>
<div class="bottom">&copy; 2013 Quantenna Communications, Inc. All Rights Reserved.</div>

</body>
</html>

