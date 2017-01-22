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

<body class="body"  onload="focus();">
	<div class="top">
		<a class="logo" href="../status_device.php">
			<img src="../images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="tools_logs.php" id="mainform" name="mainform" method="post">

<div style="border:6px solid #9FACB7; width:800px; height:auto; background-color:#fff;">
	<div class="righttop">WDS</div>
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%">WDS</td>
					<td width="70%">
					A wireless distribution system (WDS) is a system enabling the wireless interconnection of access points in an IEEE 802.11 network. It allows a wireless network to be expanded using multiple access points without the traditional requirement for a wired backbone to link them. The notable advantage of WDS over other solutions is it preserves the MAC addresses of client frames across links between access points.<br/>
					There are maximun 8 WDS(WDS0-WDS7) can be configured in this device.
				</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">MAC Address</td>
					<td width="70%">
					Please enter the MAC address you want to set for WDS
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Passphrase</td>
					<td width="70%">
					Password for WDS. Only 64 ASCII characters password is allowed.
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

