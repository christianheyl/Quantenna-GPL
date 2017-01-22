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

<body class="body" style="background:url(../images/backgrond.png) repeat-x #CCC;" onload="focus();">
	<div class="top">
		<a class="logo" href="../status_device.php">
			<img src="../images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="tools_logs.php" id="mainform" name="mainform" method="post">

<div style="border:6px solid #9FACB7; width:800px; height:auto; background-color:#fff;">
	<div class="righttop">Hidden</div>
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%">WPS Pairing Protection</td>
					<td width="70%">
					The Wireless Cient device (i.e. STB) passes device name to WAP (Wireless Access Point) during the setup of WPS.  The WAP only accepts the specific device name received from Wireless Devices to allow WSTBs to be paired with WAP but not any other Wireless devices.
				</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">MS Pairing Protection</td>
					<td width="70%">
					A different way of Pairing protection from above. This Pairing Protection will check Pairing ID right after the association, not before.
				</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Region</td>
					<td width="70%">
					Choose the running area for device.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">SCS</td>
					<td width="70%">
					Smart Channel Switch will automatically select the channel for device.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">VSP</td>
					<td width="70%">
					Video Stream Protection.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">BF</td>
					<td width="70%">
					Beamforming or spatial filtering is a signal processing technique used in sensor arrays for directional signal transmission or reception. This is achieved by combining elements in a phased array in such a way that signals at particular angles experience constructive interference while others experience destructive interference. Beamforming can be used at both the transmitting and receiving ends in order to achieve spatial selectivity. The improvement compared with omnidirectional reception/transmission is known as the receive/transmit gain (or loss).
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">COC</td>
					<td width="70%">
					a voluntary commitment of individual companies, with the aim of reducing energy consumption of products and/or systems through the setting of agreed targets in a defined development timescale.
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
<div class="bottom">Quantenna Communications</div>

</body>
</html>

