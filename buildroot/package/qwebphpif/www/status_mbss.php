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
	window.location.href="status_mbss.php";
}

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
if($curr_mode=="Station")
{
	echo "<script langauge=\"javascript\">alert(\"Don`t support in the Station mode.\");</script>";
	echo "<script language='javascript'>location.href='status_device.php'</script>";
	return;
}
//=============Get SSID=======================
$ssid1=exec("call_qcsapi get_SSID wifi1");
if(is_qcsapi_error($ssid1))
{$ssid1="N/A";}
$ssid2=exec("call_qcsapi get_SSID wifi2");
if(is_qcsapi_error($ssid2))
{$ssid2="N/A";}
$ssid3=exec("call_qcsapi get_SSID wifi3");
if(is_qcsapi_error($ssid3))
{$ssid3="N/A";}
$ssid4=exec("call_qcsapi get_SSID wifi4");
if(is_qcsapi_error($ssid4))
{$ssid4="N/A";}
$ssid5=exec("call_qcsapi get_SSID wifi5");
if(is_qcsapi_error($ssid5))
{$ssid5="N/A";}
$ssid6=exec("call_qcsapi get_SSID wifi6");
if(is_qcsapi_error($ssid6))
{$ssid6="N/A";}
$ssid7=exec("call_qcsapi get_SSID wifi7");
if(is_qcsapi_error($ssid7))
{$ssid7="N/A";}
$ssid8=exec("call_qcsapi get_SSID wifi8");
if(is_qcsapi_error($ssid8))
{$ssid8="N/A";}
//============================================
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
		<div class="righttop">STATUS - MBSS</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr>
					<td width="10%"></td>
					<td width="20%">SSID</td>
					<td width="10%" align="center">Broadcast</td>
					<td width="10%" align="center">Association</td>
					<td width="30%"></td>
				</tr>
				<tr <?php if($ssid1 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>MBSS 1: </td>
					<td><?php echo htmlspecialchars($ssid1,ENT_QUOTES);?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_option wifi1 broadcast_SSID"); echo $tmp; ?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_count_assoc wifi1"); echo $tmp; ?></td>
					<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(1);" class="button">Detail</button></td>
				</tr>
				<tr <?php if($ssid2 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>MBSS 2: </td>
					<td><?php echo htmlspecialchars($ssid2,ENT_QUOTES);?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_option wifi2 broadcast_SSID"); echo $tmp; ?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_count_assoc wifi2"); echo $tmp; ?></td>
					<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(2);" class="button">Detail</button></td>
				</tr>
				<tr <?php if($ssid3 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>MBSS 3: </td>
					<td><?php echo htmlspecialchars($ssid3,ENT_QUOTES);?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_option wifi3 broadcast_SSID"); echo $tmp; ?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_count_assoc wifi3"); echo $tmp; ?></td>
					<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(3);" class="button">Detail</button></td>
				</tr>
				<tr <?php if($ssid4 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>MBSS 4: </td>
					<td><?php echo htmlspecialchars($ssid4,ENT_QUOTES);?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_option wifi4 broadcast_SSID"); echo $tmp; ?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_count_assoc wifi4"); echo $tmp; ?></td>
					<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(4);" class="button">Detail</button></td>
				</tr>
				<tr <?php if($ssid5 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>MBSS 5: </td>
					<td><?php echo htmlspecialchars($ssid5,ENT_QUOTES);?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_option wifi5 broadcast_SSID"); echo $tmp; ?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_count_assoc wifi5"); echo $tmp; ?></td>
					<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(5);" class="button">Detail</button></td>
				</tr>
				<tr <?php if($ssid6 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>MBSS 6: </td>
					<td><?php echo htmlspecialchars($ssid6,ENT_QUOTES);?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_option wifi6 broadcast_SSID"); echo $tmp; ?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_count_assoc wifi6"); echo $tmp; ?></td>
					<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(6);" class="button">Detail</button></td>
				</tr>
				<tr <?php if($ssid7 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>MBSS 7: </td>
					<td><?php echo htmlspecialchars($ssid7,ENT_QUOTES);?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_option wifi7 broadcast_SSID"); echo $tmp; ?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_count_assoc wifi7"); echo $tmp; ?></td>
					<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(7);" class="button">Detail</button></td>
				</tr>
				<tr <?php if($ssid8 == "N/A") echo "style=\"display: none;\""; ?>>
					<td>MBSS 8: </td>
					<td><?php echo htmlspecialchars($ssid8,ENT_QUOTES);?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_option wifi8 broadcast_SSID"); echo $tmp; ?></td>
					<td align="center"><?php $tmp=exec("call_qcsapi get_count_assoc wifi8"); echo $tmp; ?></td>
					<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(8);" class="button">Detail</button></td>
				</tr>
				<tr>
					<td class="divline" colspan="5";></td>
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

