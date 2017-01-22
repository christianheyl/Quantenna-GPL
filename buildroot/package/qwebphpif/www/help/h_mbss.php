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

<?php
include("../common.php");
?>

<body class="body" onload="focus();">
	<div class="top">
		<a class="logo" href="../status_device.php">
			<img src="../images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="tools_logs.php" id="mainform" name="mainform" method="post">

<div style="border:6px solid #9FACB7; width:800px; height:auto; background-color:#fff;">
	<div class="righttop">MBSS</div>
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%">MBSS</td>
					<td width="70%">
					Mutiple BSS can be configured on this device. Additional 7 BSS can be enabled individually. 
				</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(../images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Encryption</td>
					<td width="70%">
					<b>NONE-OPEN</b>:This option features no security on your wireless network.<br/>
					<b>WPA2-AES</b>: <br/>
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(../images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Passphrase</td>
					<td width="70%">
					Enter the Passphrase, which can have 8 to 63 ASCII characters or 64 Hexadecimal digits
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(../images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Broadcast</td>
					<td width="70%">
					Enable/Disable SSID broadcasting.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(../images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">VLAN</td>
					<td width="70%">
					Not supported right now. 
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(../images/divline.png);" colspan="2";></td>
				</tr>
		</table>
	</div>
</div>
</form>
</div>
<div class="bottom"><?php echo $str_copy ?></div>

</body>
</html>

