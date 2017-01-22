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


function popnew(url)
{
	newwindow=window.open(url,'name');
	if (window.focus) {newwindow.focus();}
}

function validate(action_name)
{
	popnew("assoc_table.php?id="+action_name);
}
</script>

<?php

$curr_mode=exec("call_qcsapi get_mode wifi0");
$sum_active_interface= "";
$ssid0="";
$ssid1="";
$ssid2="";
$ssid3="";
$ssid4="";
$ssid5="";
$ssid6="";
$ssid7="";
$passed_id=0;

function get_value()
{
	global $curr_mode,$ssid0,$mac0,$arr_ssid,$arr_index,$sum_active_interface,$ssid1,$mac1,$ssid2,$mac2,$ssid3,$mac3,$ssid4,$mac4,$ssid5,$mac5,$ssid6,$mac6,$ssid7,$mac7;

	$arr_ssid="";
	$arr_index="";
	$sum_active_interface= 1;

	//=============Get SSID=======================
	$ssid0=exec("call_qcsapi get_SSID wifi0");
	$mac0=exec("call_qcsapi get_macaddr wifi0");
	$arr_ssid[0]="wifi0"."("."$mac0".")";
	$arr_index=0;

	if($curr_mode=="Access point")
	{
		$ssid1=exec("call_qcsapi get_SSID wifi1");
		if (is_qcsapi_error($ssid1))
		{
			$ssid1="N/A";
		}
		else
		{
			$mac1=exec("call_qcsapi get_macaddr wifi1");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi1"."("."$mac1".")";
			$arr_index="$arr_index".","."1";
		}

		$ssid2=exec("call_qcsapi get_SSID wifi2");
		if (is_qcsapi_error($ssid2))
		{
			$ssid2="N/A";
		}
		else
		{
			$mac2=exec("call_qcsapi get_macaddr wifi2");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi2"."("."$mac2".")";
			$arr_index="$arr_index".","."2";
		}

		$ssid3=exec("call_qcsapi get_SSID wifi3");
		if (is_qcsapi_error($ssid3))
		{
			$ssid3="N/A";
		}
		else
		{
			$mac3=exec("call_qcsapi get_macaddr wifi3");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi3"."("."$mac3".")";
			$arr_index="$arr_index".","."3";
		}

		$ssid4=exec("call_qcsapi get_SSID wifi4");
		if (is_qcsapi_error($ssid4))
		{
			$ssid4="N/A";
		}
		else
		{
			$mac4=exec("call_qcsapi get_macaddr wifi4");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi4"."("."$mac4".")";
			$arr_index="$arr_index".","."4";
		}

		$ssid5=exec("call_qcsapi get_SSID wifi5");
		if (is_qcsapi_error($ssid5))
		{
			$ssid5="N/A";
		}
		else
		{
			$mac5=exec("call_qcsapi get_macaddr wifi5");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi5"."("."$mac5".")";
			$arr_index="$arr_index".","."5";
		}

		$ssid6=exec("call_qcsapi get_SSID wifi6");
		if (is_qcsapi_error($ssid6))
		{
			$ssid6="N/A";
		}
		else
		{
			$mac6=exec("call_qcsapi get_macaddr wifi6");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi6"."("."$mac6".")";
			$arr_index="$arr_index".","."6";
		}

		$ssid7=exec("call_qcsapi get_SSID wifi7");
		if (is_qcsapi_error($ssid7))
		{
			$ssid7="N/A";
		}
		else
		{
			$mac7=exec("call_qcsapi get_macaddr wifi7");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi7"."("."$mac7".")";
			$arr_index="$arr_index".","."7";
		}
	}
}




function read_mode()
{
	global $curr_mode;
	if($curr_mode=="Station")
	{$res = "Station (STA)";}
	else
	{$res = "Access Point (AP)";}
	return $res;
}

function read_band()
{
	//Get Current band
	$curr_band_temp = trim(shell_exec("call_qcsapi get_phy_mode wifi0"));

	if(strcmp($curr_band_temp, "11na") == 0)
		$curr_band = "11an";
	else if(strcmp($curr_band_temp, "11ng") == 0)
		$curr_band = "11gn";
	else if(strcmp($curr_band_temp, "11g") == 0)
		$curr_band = "11bg";
	else
		$curr_band = get_band_sub_str($curr_band_temp);

	return ("802." . "$curr_band");
}

function read_bssid($interface)
{
	$ssid = exec("call_qcsapi get_BSSID $interface");
	if($ssid=="00:00:00:00:00:00")
	{
		$res="Not Associated";
	}
	else
	{$res=$ssid;}
	return $res;
}

function read_channel($interface)
{
	$channel = exec("call_qcsapi get_channel $interface");
	if (is_qcsapi_error($channel))
	{
		$res="Not Associated";
	}
	else
	{$res=$channel;}
	return $res;
}

function read_assoc($interface)
{
	global $curr_mode;
	if ($curr_mode == "Access point")
	{
		$res=exec("call_qcsapi get_count_assoc $interface");
	}
	else
	{
		$bssid=exec("call_qcsapi get_bssid $interface");
		if($bssid == "00:00:00:00:00:00")
		{$res="Not Associated";}
		else
		{$res="Associated";}
	}
	return $res;
}

function read_rssi($interface)
{
	global $curr_mode;
	$res="";
	if ($curr_mode == "Station")
	{
		$res=exec("call_qcsapi get_rssi_dbm $interface 0");
		if (is_qcsapi_error($res))
		{
			$res="Not Associated";
		}
	}
	return $res;
}

get_value();

if (isset($_GET['id']))
{
	$passed_id=substr($_GET['id'],0,1);
	$interface_id="wifi".$passed_id;

}
else
{
	$interface_id="wifi0";
}
?>

<script language="javascript">
var mode = "<?php echo $curr_mode; ?>";

function reload()
{
	var tmp_index= "<?php echo $arr_index; ?>";
	var interface_index=tmp_index.split(",");

	var cmb_if = document.getElementById("cmb_interface");
	if (mode == "Station")
	{
		window.location.href="status_wireless.php";
	}
	else
	{
		window.location.href="status_wireless.php?id="+interface_index[cmb_if.selectedIndex];
	}
}

function populate_interface_list()
{
	var cmb_if = document.getElementById("cmb_interface");
	var passed_id="<?php echo $passed_id; ?>";

	cmb_if.options.length = "<?php echo $sum_active_interface; ?>";
	var tmp_index= "<?php echo $arr_index; ?>";
	var interface_index=tmp_index.split(",");
	var tmp_text=new Array();
	tmp_text[0]="<?php echo $arr_ssid[0]; ?>";
	tmp_text[1]="<?php echo $arr_ssid[1]; ?>";
	tmp_text[2]="<?php echo $arr_ssid[2]; ?>";
	tmp_text[3]="<?php echo $arr_ssid[3]; ?>";
	tmp_text[4]="<?php echo $arr_ssid[4]; ?>";
	tmp_text[5]="<?php echo $arr_ssid[5]; ?>";
	tmp_text[6]="<?php echo $arr_ssid[6]; ?>";
	tmp_text[7]="<?php echo $arr_ssid[7]; ?>";

	for (var i=0; i < cmb_if.options.length; i++)
	{
		cmb_if.options[i].text = tmp_text[i]; cmb_if.options[i].value = interface_index[i];
		if (passed_id == cmb_if.options[i].value)
		{
			cmb_if.options[i].selected=true;
		}
	}

}

function onload_event()
{

	init_menu();
	if (mode == "Access point")
	{
		populate_interface_list();
	}
	else
	{
		set_visible('tr_if', false);
		set_visible('tr_l', false);
	}

}

</script>


<body class="body" onload="onload_event();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php echo $curr_mode;?>',privilege);
		</script>
	</div>
	<div class="right">
		<div class="righttop">STATUS - WIRELESS</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr id="tr_if">
					<td width="40%">Wifi Interface:</td>
					<td width="60%">
						<select name="cmb_interface" class="combox" id="cmb_interface" onchange="reload()">
						</select>
					</td>
				</tr>
				<tr id="tr_l">
					<td class="divline" colspan="2";></td>
				</tr>
				<tr>
					<td>Device Mode:</td>
					<td><?php $tmp=read_mode(); echo $tmp;?></td>
				</tr>
				<tr>
					<td>Wireless Band:</td>
					<td><?php $tmp=read_band(); echo $tmp;?></td>
				</tr>
				<tr>
					<td>Bandwidth:</td>
					<td><?php $tmp=exec("call_qcsapi get_bw wifi0"); echo $tmp;?> MHz</td>
				</tr>
				<tr>
					<td>AP Mac Address (BSSID):</td>
					<td><?php $tmp=read_bssid($interface_id); echo $tmp;?></td>
				</tr>
				<tr>
					<td>Channel:</td>
					<td><?php $tmp=exec("call_qcsapi get_channel wifi0"); echo $tmp;?></td>
				</tr>
				<tr>
					<td>
					<?php
					if($curr_mode=="Station")
					{echo "Association Status:";}
					else
					{echo "Associated Devices Count:";}
					?>
					</td>
					<td valign="middle"><?php $tmp=read_assoc($interface_id);echo $tmp; ?>
<input type="button" name="btn_assoc_table" id="btn_assoc_table" value="Association Table" class="button" style="width:150px; height:23px;" onclick="validate(<?php echo $passed_id;?>);"/>
					</td>
				</tr>
				<tr id="tr_rssi" <?php if($curr_mode == "Access point") echo "style=\"display: none;\""; ?>>
					<td>RSSI:</td>
					<td><?php $tmp=read_rssi($interface_id); echo $tmp;?></td>
				</tr>
				<tr>
					<td>Packets Received Successfully:</td>
					<td><?php $tmp=exec("call_qcsapi get_counter64 $interface_id RX_packets"); echo $tmp;?></td>
				</tr>
				<tr>
					<td>Bytes Received:</td>
					<td><?php $tmp=exec("call_qcsapi get_counter64 $interface_id RX_bytes"); echo $tmp;?></td>
				</tr>
				<tr>
					<td>Packets Transmitted Successfully:</td>
					<td><?php $tmp=exec("call_qcsapi get_counter64 $interface_id TX_packets"); echo $tmp;?></td>
				</tr>
				<tr>
					<td>Bytes Transmitted:</td>
					<td><?php $tmp=exec("call_qcsapi get_counter64 $interface_id TX_bytes"); echo $tmp;?></td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_refresh" id="btn_refresh" type="button" onclick="reload();"  class="button">Refresh</button>
			</div>
		</div>
	</div>
</div>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> <br />
	<div><?php echo $str_copy ?></div>
</div>
</body>
</html>

