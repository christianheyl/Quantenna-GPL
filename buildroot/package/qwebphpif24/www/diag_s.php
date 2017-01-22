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
$privilege = get_privilege(0);

?>


<script type="text/javascript">
var privilege="<?php echo $privilege; ?>";
</script>

<?php

if(isset($_POST['action']))
{
	$tmp_output=$_POST['customer_info'];
	if($tmp_output != "")
	{
		exec("echo $tmp_output > /mnt/jffs2/cus_info");
	}
	else
	{
		exec("echo \"none\" > /mnt/jffs2/cus_info");
	}
}

$Linux_version=exec("cat /var/log/messages | grep  \"Linux version\"| head -n 1 | cut -d ']' -f2");
if($Linux_version=="")
{$Linux_version=exec("uname -a");}
$hw_version=exec("cat /proc/hw_revision");
$customer_version=exec("/scripts/get_rev_num");
$curr_mode=exec("call_qcsapi get_mode wifi0");
$ssid_wifi0=exec("call_qcsapi get_SSID wifi0");
$curr_region=exec("cat /etc/region");
$curr_channel=exec("call_qcsapi get_channel wifi0");
$tx_power=exec("call_qcsapi get_tx_power wifi0 $curr_channel");
$scs_status=exec("call_qcsapi get_scs_status wifi0");
$vsp_status=exec("call_qcsapi vsp wifi0 show");
$has_file=exec("ls /mnt/jffs2/ | grep cus_info |wc -l");
$cus_info="";
if ($has_file != 0)
{
	$cus_info=exec("cat /mnt/jffs2/cus_info");
}
else
{
	$cus_info="none";
}


function read_uptime()
{
	$info=exec("uptime");
	$arraylist=split(",",$info);
	$arraylist=split(" ",$arraylist[0]);
	$res=$arraylist[3].$arraylist[4];
	return $res;
}

function Get_info()
{
	exec("echo \"calcmd 31 0 4 0\" > /sys/devices/qdrv/control; ");
	exec("info;cat /var/log/messages |tail -n 11 | cut -d ']' -f 2 >/tmp/calibration");
}

function Backup_jffs2_and_proc()
{
	exec("mkdir /tmp/configuration");
	exec("mkdir /var/www/download");
	exec("cp -rf /mnt/jffs2/ /tmp/configuration/");
	exec("cp -rf /proc/bootcfg/ /tmp/configuration/");
}


function Write_system_configurations($title,$command)
{
	$buff_file = "/tmp/config_buff.txt";
	$file = "/tmp/configuration/configuration.txt";

	exec("$command > /tmp/config_buff.txt 2>&1 ");
	$addLine = file_get_contents($buff_file);
	$addLine = "[".$title."]"."\n".$addLine;
	$buff_conf_file = file_get_contents($file);
	$f = fopen($file,"w");
	fwrite($f, $buff_conf_file."\n".$addLine);
	fclose($f);
}

function Collect_information()
{
	file_put_contents("/tmp/configuration/configuration.txt", "Running board information\n");

	Write_system_configurations("Linux_version", "cat /var/log/messages | grep  \"Linux version\"| head -n 1 | cut -d ']' -f2");
	Write_system_configurations("Chip_version", "cat /proc/hw_revision");
	Write_system_configurations("System_uptime", "uptime");
	Write_system_configurations("Software_version", "/scripts/get_rev_num");
	Write_system_configurations("Interface_info", "ifconfig");
	Write_system_configurations("WLAN", "cat /tmp/iwconfig");
	Write_system_configurations("PS", "ps");
	Write_system_configurations("Calibration", "cat /tmp/calibration");

	exec("tar -cvf /var/www/download/configuration.tar /tmp/configuration");
}

Get_info();
Backup_jffs2_and_proc();
Collect_information();

?>

<script language="javascript">
function ToDynamic() {
	window.location.href="diag_d.php";
}
</script>

<body class="body" onload="focus();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="diag_s.php" id="cusinfo" name="cusinfo" method="post">
<div style="border:6px solid #9FACB7; width:1200px; height:auto; background-color:#fff;">
	<div class="righttop">Qdiagnostic Static</div>
		<!--choose table-->
		<div class="rightmain">
			<input type="button"  name="Static_page" id="Static_page" value="Go To Static Page" style="width:180px;" disabled="disabled" />
			<input type="button"  name="Dynamic_page" id="Dynamic_page" value="Go To Dynamic Page" style="width:180px" onclick="ToDynamic();"/>
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%"></td>
					<td width="70%"></td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
			</table>
		<div class="tablemain"><b>Customer Information</b></div>
		<div style="width:auto;">
			<tr border="1"><input style="width:1000px; " name="customer_info" type="text" id="customer_info" value="<?php echo $cus_info ?>" class="textbox"/></tr>
			<tr><input type="submit"  name="save" id="save" value="Save" class="button"/></tr>

		</div>
		<input id="action" name="action" type="hidden" value="1">
		<table class="tablemain" style=" height:auto">
			<tr>
				<td width="30%"></td>
				<td width="70%"></td>
			</tr>
			<tr>
				<td class="divline" colspan="2";></td>
			</tr>
		</table>
	<!--data table-->
	<table class="tablemain">
		<div ><b>Basic Information</b></div>
		<tr>
			<td width="200px">Current Mode</td>
			<td>
				<?php echo $curr_mode?>
			</td>
		</tr>
		<tr>
			<td>Wifi0 SSID</td>
			<td>
				<?php echo $ssid_wifi0?>
			</td>
		</tr>
		<tr>
			<td >Software Version</td>
			<td>
				<?php echo $customer_version?>
			</td>
		</tr>
		<tr>
			<td >VSP status</td>
			<td>
				<?php if($vsp_status == "VSP is not enabled") echo "Disabled" ; else echo "Enabled"?>
			</td>
		</tr>
		<tr>
			<td >SCS status</td>
			<td>
				<?php echo $scs_status?>
			</td>
		</tr>
		<tr>
			<td >Current Channel</td>
			<td>
				<?php echo $curr_channel?>
			</td>
		</tr>
		<tr>
			<td >Get_tx_power</td>
			<td>
				<?php echo $tx_power?>
			</td>
		</tr>
		<tr>
			<td>Chip Version</td>
			<td>
				<?php echo $hw_version?>
				</td>
		</tr>
		<tr>
			<td>Linux Version</td>
			<td>
				<?php echo $Linux_version?>
			</td>
		</tr>
		<tr>
			<td>System Uptime</td>
			<td>
				<?php $tmp = read_uptime(); echo $tmp;?>
			</td>
		</tr>
		<tr>
			<td class="divline" colspan="2";></td>
		</tr>
	</table>


<!--ouput table-->
	<table class="tablemain">
		<div ><b>Advanced Information</b></div>
		<tr>
			<td style="width:200px;">WLAN Configuration</td>
			<td>
				<pre style="width:900px; overflow: auto;"><?php passthru("iwconfig >/tmp/iwconfig 2>&1 ; cat /tmp/iwconfig");?></pre>
			</td>
		</tr>
		<tr>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr>
			<td>Interface Information</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("ifconfig");?></pre>
			</td>
		</tr>
		<tr>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr>
			<td>U-boot Parameters</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("cat /proc/bootcfg/env");?></pre>
			</td>
		</tr>
		<tr>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr>
			<td>Running Processes</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("ps");?></pre>
			</td>
		</tr>
		<tr>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr>
			<td>Files in /mnt/jffs2/</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("ls -al /mnt/jffs2/");?></pre>
			</td>
		</tr>
		<tr>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr>
			<td>Calibration Information</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("cat /tmp/calibration");?></pre>
			</td>
		</tr>
		<tr>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr>
			<td>Regulatory Error Message</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("cat /tmp/api.log");?></pre>
			</td>
		</tr>
		<tr>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr>
			<td>Wireless Config</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("cat /mnt/jffs2/wireless_conf.txt");?></pre>
			</td>
		</tr>
		<tr>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr>
			<td>Boardparam</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("cat /proc/bootcfg/boardparam");?></pre>
			</td>
		</tr>
		<tr>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr <?php if($curr_mode == "Station") echo "style=\"display: none;\""; ?>>
			<td>hostapd.conf</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("cat /mnt/jffs2/hostapd.conf");?></pre>
			</td>
		</tr>
		<tr <?php if($curr_mode == "Station") echo "style=\"display: none;\""; ?>>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr <?php if($curr_mode != "Station") echo "style=\"display: none;\""; ?>>
			<td>wpa_supplicant</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("cat /mnt/jffs2/wpa_supplicant.conf");?></pre>
			</td>
		</tr>
		<tr <?php if($curr_mode != "Station") echo "style=\"display: none;\""; ?>>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr <?php if($curr_region != "us") echo "style=\"display: none;\""; ?>>
			<td>eirp_info_us.txt</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("cat /proc/bootcfg/eirp_info_us.txt");?></pre>
			</td>
		</tr>
		<tr <?php if($curr_region != "us") echo "style=\"display: none;\""; ?>>
			<td class="divline" colspan="2";></td>
		</tr>
		<tr <?php if($curr_mode != "jp") echo "style=\"display: none;\""; ?>>
			<td>eirp_info_jp.txt</td>
			<td>
					<pre style="width:900px; overflow: auto;"><?php passthru("cat /proc/bootcfg/eirp_info_jp.txt");?></pre>
			</td>
		</tr>
		<tr <?php if($curr_mode != "jp") echo "style=\"display: none;\""; ?>>
			<td class="divline" colspan="2";></td>
		</tr>
	</table>


<!--button table-->
	<table class="tablemain" >
		<tr>
			<td align="center">
				<p>
				<button type="button" style="height:50px; width:200px" onClick="window.location='download/configuration.tar'">Download System Information</button></p>
			</td>

		</tr>
	</table>
	</div>
</div>
</form>
<div class="bottom">Quantenna Communications</div>
</body>
</html>

