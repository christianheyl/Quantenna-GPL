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
	<div class="righttop">ASSOCIATION TABLE</div>
	<div class="rightmain">
		<table class="tablemain">
			<tr>
				<td width="5%"  align="center" bgcolor="#96E0E2" ></td>
				<td width="20%" align="center" bgcolor="#96E0E2">
				<?php
				$mode=exec("call_qcsapi get_mode wifi0");
				if ($mode == "Access point")
				{
					echo "Station";
				}
				else
				{
					echo "Access Point";
				}
				?></td>
				<td width="8%"  align="center" bgcolor="#96E0E2"<?php if($mode=="Station"){echo "style=\"display:none\"";}?>>VAP</td>
				<td width="8%"  align="center" bgcolor="#96E0E2">RSSI</td>
				<td width="17%" align="center" bgcolor="#96E0E2">Rx Bytes</td>
				<td width="17%" align="center" bgcolor="#96E0E2">Tx Bytes</td>
				<td width="5%"  align="center" bgcolor="#96E0E2">Bw</td>
				<td width="20%" align="center" bgcolor="#96E0E2">Time Associated</td>
			</tr>
			<pre><?php
				$interface_index=0;
				$api_lost=0;
				while(1)
				{
					if (isset($_GET['id']))
					{
						$interface="wifi".substr($_GET['id'],0,1);
						$api_lost=1;
					}
					else
					{
						$interface=exec("call_qcsapi get_interface_by_index $interface_index");
						if(!(strpos($interface, "QCSAPI entry point") === false))
						{break;}
						if(is_qcsapi_error($interface))
						{
							if ($interface_index === 0)
							{
								$interface="wifi0";
								$api_lost=1;
							}
							//in case the SDK do not have this API
							else
								break;
						}
						$interface_index++;
					}
					$assoc_count=exec("call_qcsapi get_count_assoc $interface");
					if(is_qcsapi_error($assoc_count))
					{$assoc_count="0";}
					if( $assoc_count > 0)
					{
						$index = 0;
						while( $index<$assoc_count )
						{
							//mac
							$station_mac=exec("call_qcsapi get_associated_device_mac_addr $interface $index");
							if(is_qcsapi_error($station_mac))
							{
								$current_wifi_mode = trim(shell_exec("call_qcsapi get_mode wifi0"));
								if (strcmp($current_wifi_mode, "Access point" ) == 0)
								{
									$station_mac = "-";
								}
								else {
									$station_mac=exec("call_qcsapi get_bssid wifi0");
								}
							}

							$rssi=exec("call_qcsapi get_rssi_dbm $interface $index");
							if(is_qcsapi_error($rssi))
							{
								$rssi = "-";
							}

							$rx_bytes=exec("call_qcsapi get_rx_bytes $interface $index");
							if(is_qcsapi_error($rx_bytes))
							{
								$rx_bytes = "-";
							}

							$tx_bytes=exec("call_qcsapi get_tx_bytes $interface $index");
							if(is_qcsapi_error($tx_bytes))
							{
								$tx_bytes = "-";
							}

							$bw_assoc=exec("call_qcsapi get_assoc_bw $interface $index");
							if(is_qcsapi_error($bw_assoc) ||
								!(strpos($bw_assoc, "QCSAPI entry point") === false ))
							{
								$bw_assoc = "-";
							}

							$time_associated=exec("call_qcsapi get_time_associated $interface $index");
							if(is_qcsapi_error($time_associated) ||
								!(strpos($time_associated, "QCSAPI entry point") === false )
							)
							{
								$time_associated = "-";
							}
							$index++;
							echo "<tr>";
							echo "<td width=\"5%\"  align=\"center\" >$index</td>";
							echo "<td width=\"20%\" align=\"center\" >$station_mac</td>";
							if($mode!="Station")
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
					if($api_lost==1) break;
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
	<div><?php echo $str_copy ?></div>
</div>

</body>
</html>

