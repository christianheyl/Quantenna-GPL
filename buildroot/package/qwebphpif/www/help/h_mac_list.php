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

<body class="body"  onload="focus();">
	<div class="top">
		<a class="logo" href="../status_device.php">
			<img src="../images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="tools_logs.php" id="mainform" name="mainform" method="post">

<div style="border:6px solid #9FACB7; width:800px; height:auto; background-color:#fff;">
	<div class="righttop">MAC ADDRESS LIST</div>
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%">MAC ADDRESS LIST</td>
					<td width="70%">
					 MAC Filtering refers to a security access control method whereby the 48-bit address assigned to each network card is used to determine access to the network. All relative MAC addresses can be stored in MAC address list. 
				</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(../images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">MAC Address Filtering</td>
					<td width="70%">
					<b>NONE</b>:This option features no security on your wireless network.<br/>
					<b>Deny if not authorized</b>:Only those MAC addresses authorized in MAC adress list will be having access to network. All other MAC addresses will be denied. MAC list works as a write list.<br/>
					<b>Authorize if not denied</b>:Only those MAC addresses denied in MAC address list will be denied. Other MAC will be having access. MAC list works as a black list.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(../images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">MAC Address</td>
					<td width="70%">
					<b>Deny</b>:Black list will be created while using Deny for MAC list. All MAC in the list will be denied.<br/>
					<b>Authorize</b>:White list will be created while using Authorize for MAC list. Only MAC in the list will be having access to network.
					</td>
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

