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
	<div class="righttop">Wireless config</div>
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%">Device Mode</td>
					<td width="70%">
					Choose device mode to AP or Station
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Wireless Band</td>
					<td width="70%">
					Choose device band to 5Gz or 2.4Gz
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">ESSID</td>
					<td width="70%">
					The SSID is the network name shared among all points in a wireless network. It is case-sensitive and must not exceed 32 characters (use any of the characters on the keyboard). For additional security, you'd better change the default SSID (Cisco) to a unique name.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Channel:</td>
					<td width="70%">
					Select the channel for device networking. If you are not sure which channel to select, keep the default, Auto.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Encryption</td>
					<td width="70%">
					<b>NONE-OPEN</b>:This option features no security on your wireless network.<br/>
					<b>WPA2-AES</b>:AES is based on a design principle known as a substitution-permutation network, and is fast in both software and hardware. Unlike its predecessor DES, AES does not use a Feistel network. AES is a variant of Rijndael which has a fixed block size of 128 bits, and a key size of 128, 192, or 256 bits.  <br/>
					<b>WPA2+WPA(Mixed mode)</b>:The Wi-Fi Alliance intended WPA as an intermediate measure to take the place of WEP pending the availability of the full IEEE 802.11i standard. WPA could be implemented through firmware upgrades on wireless network interface cards designed for WEP that began shipping as far back as 1999. However, since the changes required in the wireless access points (APs) were more extensive than those needed on the network cards, most pre-2003 APs could not be upgraded to support WPA.<br/>WPA2 has replaced WPA. WPA2, which requires testing and certification by the Wi-Fi Alliance, implements the mandatory elements of IEEE 802.11i. In particular, it introduces CCMP, a new AES-based encryption mode with strong security. Certification began in September, 2004; from March 13, 2006, WPA2 certification is mandatory for all new devices to bear the Wi-Fi trademark.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Passphrase</td>
					<td width="70%">Enter the Passphrase, which can have 8 to 63 ASCII characters or 64 Hexadecimal digits</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Broadcast SSID</td>
					<td width="70%">
					When wireless clients survey the local area for wireless networks to associate with, they will detect the SSID broadcast by the AP. To broadcast the AP's SSID, keep the default setting, Enable. If you do not want to broadcast the AP's SSID, then select Disable.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Bandwidth</td>
					<td width="70%">
					You can select the channel bandwidth manually for device connections. When it is set to 20MHz, only the 20MHz channel is used. When it is set to 40MHz, device connections will use 40MHz channel.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">WPS State</td>
					<td width="70%">
					Wi-Fi Protected Setup, a standard that attempts to automate secure wireless network set up and connection<br/>Three status can be choosen:
					<b>Configured</b>,
					<b>Not Configured</b>,
					<b>Disabled</b>
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">WPS PIN</td>
					<td width="70%">
					Please enter the password for WPS from here
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">WPS AP PIN</td>
					<td width="70%">
					Please generate the AP pin for WPS setting.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">TX Rate</td>
					<td width="70%">
					Transmit rate. Default value is "auto"
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Beacon Interval</td>
					<td width="70%">
					The default value is 100. The Beacon Interval value indicates the frequency interval of the beacon. A beacon is a packet broadcast by the Router to synchronize the wireless network.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">DTIM Period</td>
					<td width="70%">
					This value indicates the interval of the Delivery Traffic Indication Message (DTIM). A DTIM field is a countdown field informing clients of the next window for listening to broadcast and multicast messages. When the Router has buffered broadcast or multicast messages for associated clients, it sends the next DTIM with a DTIM Interval value. Its clients hear the beacons and awaken to receive the broadcast and multicast messages. The default value is 3.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">Short GI</td>
					<td width="70%">
					Short Guard Interval is used to ensure that distinct transmissions do not interfere with one another You can enable/disable it from here.
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td width="30%">MAC Address Filtering</td>
					<td width="70%">
					Wireless access can be filtered by using the MAC addresses of the wireless devices transmitting within your network radius.
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

