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
	window.location.href="status_wds.php";
}
</script>

<?php
$curr_mode=exec("call_qcsapi get_mode wifi0");
if($curr_mode=="Station")
{
	echo "<script langauge=\"javascript\">alert(\"Don`t support in the Station mode.\");</script>";
	echo "<script language='javascript'>location.href='status_device.php'</script>";
	return;
}
//==============Get Signal Strength================
$signal_0=0;
$signal_1=0;
$signal_2=0;
$signal_3=0;
$signal_4=0;
$signal_5=0;
$signal_6=0;
$signal_7=0;
//=================================================
//==============Get WDS Peer=======================
$wds0=exec("call_qcsapi wds_get_peer_address wifi0 0");
if(is_qcsapi_error($wds0))
{$wds0="N/A";}
else
{$signal_0=exec("call_qcsapi get_rssi_dbm wds0 0");}

$wds1=exec("call_qcsapi wds_get_peer_address wifi0 1");
if(is_qcsapi_error($wds1))
{$wds1="N/A";}
else
{$signal_1=exec("call_qcsapi get_rssi_dbm wds1 0");}

$wds2=exec("call_qcsapi wds_get_peer_address wifi0 2");
if(is_qcsapi_error($wds2))
{$wds2="N/A";}
else
{$signal_2=exec("call_qcsapi get_rssi_dbm wds2 0");}

$wds3=exec("call_qcsapi wds_get_peer_address wifi0 3");
if(is_qcsapi_error($wds3))
{$wds3="N/A";}
else
{$signal_3=exec("call_qcsapi get_rssi_dbm wds3 0");}

$wds4=exec("call_qcsapi wds_get_peer_address wifi0 4");
if(is_qcsapi_error($wds4))
{$wds4="N/A";}
else
{$signal_4=exec("call_qcsapi get_rssi_dbm wds4 0");}

$wds5=exec("call_qcsapi wds_get_peer_address wifi0 5");
if(is_qcsapi_error($wds5))
{$wds5="N/A";}
else
{$signal_5=exec("call_qcsapi get_rssi_dbm wds5 0");}

$wds6=exec("call_qcsapi wds_get_peer_address wifi0 6");
if(is_qcsapi_error($wds6))
{$wds6="N/A";}
else
{$signal_6=exec("call_qcsapi get_rssi_dbm wds6 0");}

$wds7=exec("call_qcsapi wds_get_peer_address wifi0 7");
if(is_qcsapi_error($wds7))
{$wds7="N/A";}
else
{$signal_7=exec("call_qcsapi get_rssi_dbm wds7 0");}
//=================================================
?>
<body class="body" onload="init_menu();">
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
		<div class="righttop">STATUS - WDS</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr>
					<td width="10%">WDS</td>
					<td width="30%">MAC Address</td>
					<td width="60%">RSSI(dBm)</td>
				</tr>
				<tr <?php if($wds0 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>WDS0: </td>
					<td><?php echo $wds0;?></td>
					<td><?php echo $signal_0;?></td>
				</tr>
				<tr <?php if($wds1 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>WDS1: </td>
					<td><?php echo $wds1;?></td>
					<td><?php echo $signal_1;?></td>
				</tr>
				<tr <?php if($wds2 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>WDS2: </td>
					<td><?php echo $wds2;?></td>
					<td><?php echo $signal_2;?></td>
				</tr>
				<tr <?php if($wds3 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>WDS3: </td>
					<td><?php echo $wds3;?></td>
					<td><?php echo $signal_3;?></td>
				</tr>
				<tr <?php if($wds4 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>WDS4: </td>
					<td><?php echo $wds4;?></td>
					<td><?php echo $signal_4;?></td>
				</tr>
				<tr <?php if($wds5 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>WDS5: </td>
					<td><?php echo $wds5;?></td>
					<td><?php echo $signal_5;?></td>
				</tr>
				<tr <?php if($wds6 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>WDS6: </td>
					<td><?php echo $wds6;?></td>
					<td><?php echo $signal_6;?></td>
				</tr>
				<tr <?php if($wds7 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>WDS7: </td>
					<td><?php echo $wds7;?></td>
					<td><?php echo $signal_7;?></td>
				</tr>
				<tr>
					<td class="divline" colspan="4";></td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_cancel" id="btn_cancel" type="button" onclick="reload();" class="button">Refresh</button>
			</div>
		</div>
	</div>
</div>
</form>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> <br />
	<div><?php echo $str_copy ?></div>
</div>
</body>
</html>

