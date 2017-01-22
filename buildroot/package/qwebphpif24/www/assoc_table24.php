#!/usr/lib/cgi-bin/php-cgi

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
<script language="javascript" type="text/javascript" src="./js/webif.js"></script>
<?php
include("common.php");
$privilege = get_privilege(2);
?>

<script type="text/javascript">
var privilege="<?php echo $privilege; ?>";
function reload()
{
	window.location.reload();
}
</script>

<body class="body">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<div style="border:6px solid #9FACB7; width:800px; background-color:#fff;">
	<div class="righttop">2.4G WI-FI ASSOCIATION TABLE</div>
	<div class="rightmain">
		<table class="tablemain">
			<tr>
				<td width="5%"  align="center" bgcolor="#96E0E2" ></td>
				<td width="20%" align="center" bgcolor="#96E0E2">
				<?php
				$mode=exec("qweconfig get mode.wlan1");
				if ($mode == "0")
				{
					echo "Station";
				}
				else
				{
					echo "Access Point";
				}
				?></td>
				<td width="8%"  align="center" bgcolor="#96E0E2"<?php if($mode!="0"){echo "style=\"display:none\"";}?>>VAP</td>
				<td width="8%"  align="center" bgcolor="#96E0E2">RSSI</td>
				<td width="17%" align="center" bgcolor="#96E0E2">Rx Bytes</td>
				<td width="17%" align="center" bgcolor="#96E0E2">Tx Bytes</td>
				<td width="5%"  align="center" bgcolor="#96E0E2">Bw</td>
				<td width="20%" align="center" bgcolor="#96E0E2">Time Associated</td>
			</tr>
			<pre><?php
				if (isset($_GET['id']))
				{
					$passed_id=substr($_GET['id'],0,1);
					switch ($passed_id)
					{
						case "0":
							$interface="ap";
							break;
						case "1":
							$interface="vap0";
							break;
						case "2":
							$interface="vap1";
							break;
						case "3":
							$interface="vap2";
							break;
						case "4":
							$interface="vap3";
							break;
						default:
							$interface="ap";
							break;
					}
				}
				else
				{
					$interface="ap";
				}
				$assoc_count=exec("qweaction wlan1 stainfo $interface | grep mac | wc -l");
				if( $assoc_count > 0)
				{
					$index = 0;
					while( $index<$assoc_count )
					{
						//mac
						$current_wifi_mode = exec("qweconfig get mode.wlan1");
						$tmp=$index+1;
						if ($current_wifi_mode=="0")
						{
							$station_mac=exec("qweaction wlan1 stainfo $interface | grep mac | head -$tmp | awk '{print$2}'");
						}
						else {
							$station_mac = "qweaction wlan1 status rootap | grep mac | awk '{print $2}'";
						}
						$tmp_rssi=exec("qweaction wlan1 stainfo $interface | grep rssi | head -$tmp | awk '{print$2}'");
						$rssi=$tmp_rssi-100;
						$rx_bytes=exec("qweaction wlan1 stainfo $interface | grep rx_bytes | head -$tmp | awk '{print$2}'");
						$tx_bytes=exec("qweaction wlan1 stainfo $interface | grep tx_bytes | head -$tmp | awk '{print$2}'");
						$bw_assoc=trim(exec("qweaction wlan1 stainfo $interface | grep bandwidth | head -$tmp | awk -F ':' '{print$2}'"));
						$time_associated=trim(exec("qweaction wlan1 stainfo $interface | grep duration | head -$tmp | awk -F ':' '{print$2}'"))." s";
						$index++;
						echo "<tr>";
						echo "<td width=\"5%\"  align=\"center\" >$index</td>";
						echo "<td width=\"20%\" align=\"center\" >$station_mac</td>";
						if($mode!="rp")
						{
							echo "<td width=\"8%\"  align=\"center\" >$interface</td>";
						}
						echo "<td width=\"8%\"  align=\"center\" >$rssi dbm</td>";
						echo "<td width=\"17%\" align=\"center\" >$rx_bytes</td>";
						echo "<td width=\"17%\" align=\"center\" >$tx_bytes</td>";
						echo "<td width=\"5%\"  align=\"center\" >$bw_assoc</td>";
						echo "<td width=\"20%\" align=\"center\" >$time_associated</td>";
						echo "</tr>";
					}
				}
			?></pre>
			</table>
		<div class="rightbottom">
			<button name="btn_refresh" id="btn_refresh" type="button" onclick="reload();"  class="button">Refresh</button>
		</div>
	</div>
</div>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> <br />
	<div>&copy; 2013 Quantenna Communications, Inc. All Rights Reserved.</div>
</div>

</body>
</html>

