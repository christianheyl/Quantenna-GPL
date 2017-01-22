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

function popnew(url)
{
	newwindow=window.open(url,'name');
	if (window.focus) {newwindow.focus();}
}

function validate(action_name)
{
	popnew("assoc_table24.php?id="+action_name);
}
</script>

<?php

$curr_mode=exec("qweconfig get mode.wlan1");
$sum_active_interface= "";
$ssid0="";
$ssid1="";
$ssid2="";
$ssid3="";
$ssid4="";
$passed_id=0;

function get_value()
{
	global $curr_mode,$ssid0,$mac0,$arr_ssid,$arr_index,$sum_active_interface,$ssid1,$mac1,$ssid2,$mac2,$ssid3,$mac3,$ssid4,$mac4;

	$arr_ssid="";
	$arr_index="";
	$sum_active_interface= 1;

	//=============Get SSID=======================
	if($curr_mode=="0")
	{
		$ssid0=exec("qweconfig get ssid.wlan1");
		$mac0=exec("qweaction wlan1 showhwaddr ap");
		$arr_ssid[0]="ap"."("."$mac0".")";
		$arr_index=0;

		$ssid1=exec("qweconfig get enable.vap0.wlan1");
		if($ssid1 == "0")
		{
			$ssid1="N/A";
		}
		else
		{
			$ssid1=exec("qweconfig get ssid.vap0.wlan1");
			$mac1=exec("qweaction wlan1 showhwaddr vap0");
			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="vap0"."("."$mac1".")";
			$arr_index="$arr_index".","."1";
		}

		$ssid2=exec("qweconfig get enable.vap1.wlan1");
		if($ssid2 == "0")
		{
			$ssid2="N/A";
		}
		else
		{
			$ssid2=exec("qweconfig get ssid.vap1.wlan1");
			$mac2=exec("qweaction wlan1 showhwaddr vap1");
			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="vap1"."("."$mac2".")";
			$arr_index="$arr_index".","."2";
		}

		$ssid3=exec("qweconfig get enable.vap2.wlan1");
		if($ssid3 == "0")
		{
			$ssid3="N/A";
		}
		else
		{
			$ssid3=exec("qweconfig get ssid.vap2.wlan1");
			$mac3=exec("qweaction wlan1 showhwaddr vap2");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="vap2"."("."$mac3".")";
			$arr_index="$arr_index".","."3";
		}

		$ssid4=exec("qweconfig get enable.vap3.wlan1");
		if($ssid4 == "0")
		{
			$ssid4="N/A";
		}
		else
		{
			$ssid4=exec("qweconfig get ssid.vap3.wlan1");
			$mac4=exec("qweaction wlan1 showhwaddr vap3");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="vap3"."("."$mac4".")";
			$arr_index="$arr_index".","."4";
		}
	}
}

function read_mode()
{
	global $curr_mode;
	if($curr_mode=="0")
	{$res = "Access Point (AP)";}
	else if($curr_mode=="1")
	{$res = "Bridge";}
	else if($curr_mode=="2")
	{$res = "Repeator";}
	return $res;
}

function read_bssid($interface)
{
	$bssid = exec("qweaction wlan1 showhwaddr $interface");
	if($bssid=="00:00:00:00:00:00")
	{
		$res="Not Associated";
	}
	else
	{$res=$bssid;}
	return $res;
}

function read_ssid()
{
	global $curr_mode;
	if ($curr_mode == "0")
	{
		$ssid=exec("qweconfig get ssid.wlan1");
	}
	else
	{
		$ssid=exec("qweconfig get ssid.sta.wlan1");
	}
	return $ssid;
}

function read_assoc($interface)
{
	global $curr_mode;
	if ($curr_mode == "0")
	{
		$res=exec("qweaction wlan1 stainfo $interface | grep mac | wc -l");
	}
	else
	{
		$res=trim(exec("qweaction wlan1 status rootap | grep mac | awk '{print$2}'"));
		if($res == "not-associated")
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
	if ($curr_mode != "0")
	{
		$res=exec("qweaction wlan1 status rootap | grep rssi | awk '{print$2}'");
		if($res == "not-associated")
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
	switch ($passed_id)
	{
		case "0":
			$interface_id="ap";
			break;
		case "1":
			$interface_id="vap0";
			break;
		case "2":
			$interface_id="vap1";
			break;
		case "3":
			$interface_id="vap2";
			break;
		case "4":
			$interface_id="vap3";
			break;
		default:
			$interface_id="ap";
			break;
	}
}
else
{
	$interface_id="ap";
}

?>

<script language="javascript">
var mode = "<?php echo $curr_mode; ?>";

function reload()
{
	var tmp_index= "<?php echo $arr_index; ?>";
	var interface_index=tmp_index.split(",");

	var cmb_if = document.getElementById("cmb_interface");
	if (mode == "0")
	{
		window.location.href="status_wireless24.php?id="+interface_index[cmb_if.selectedIndex];
	}
	else
	{
		window.location.href="status_wireless24.php";
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

	for (var i=0; i < cmb_if.options.length; i++)
	{
		cmb_if.options[i].text = tmp_text[i];
		cmb_if.options[i].value = interface_index[i];
		if (passed_id == cmb_if.options[i].value)
		{
			cmb_if.options[i].selected=true;
		}
	}

}

function onload_event()
{
	init_menu();
	if (mode == "0")
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
			createMenu('<?php $tmp=exec("call_qcsapi get_mode wifi0"); echo $tmp;?>','<?php $tmp=exec("qweconfig get mode.wlan1"); echo $tmp;?>',privilege);
		</script>
	</div>
	<div class="right">
		<div class="righttop">STATUS - 2.4G WI-FI</div>
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
					<td>Bandwidth:</td>
					<td><?php $tmp=exec("grep currBW /proc/wlan0/mib_11n | awk '{print $2}'"); echo $tmp;?></td>
				</tr>
				<tr>
					<td>AP Mac Address (BSSID):</td>
					<td><?php $tmp=read_bssid($interface_id); echo $tmp;?></td>
				</tr>
				<tr>
					<td>SSID:</td>
					<td><?php $tmp=read_ssid(); echo $tmp;?></td>
				</tr>
				<tr>
					<td>Channel:</td>
					<td><?php $tmp=exec("grep dot11channel /proc/wlan0/mib_rf | awk '{print $2}'"); echo $tmp;?></td>
				</tr>
				<tr>
					<td>
					<?php
					if($curr_mode=="0")
					{echo "Associated Devices Count:";}
					else
					{echo "Association Status:";}
					?>
					</td>
					<td valign="middle"><?php $tmp=read_assoc($interface_id);echo $tmp; ?> &nbsp;
<input type="button" name="btn_assoc_table" id="btn_assoc_table" value="Association Table" class="button" style="width:150px; height:23px;" onclick="validate(<?php echo $passed_id;?>);"/>
					</td>
				</tr>
				<tr id="tr_rssi" <?php if($curr_mode == "0") echo "style=\"display: none;\""; ?>>
					<td>RSSI:</td>
					<td><?php $tmp=read_rssi($interface_id); echo $tmp;?></td>
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
	<div>&copy; 2013 Quantenna Communications, Inc. All Rights Reserved.</div>
</div>
</body>
</html>

