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
	window.location.href="status_networking.php";
}
</script>

<?php
$curr_mode=exec("call_qcsapi get_mode wifi0");
$is_pcie=exec("ifconfig | grep pcie") == ""? 0:1;
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
		<div class="righttop">STATUS - NETWORKING</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr>
					<td width="40%">IP Address:</td>
					<td width="60%"><?php $tmp = read_ipaddr();echo $tmp; ?></td>
				</tr>
				<tr>
					<td>Netmask:</td>
					<td><?php $tmp = read_netmask();echo $tmp; ?></td>
				</tr>
				<tr <?php if($is_pcie == 1) echo "style=\"display: none;\""; ?>>
					<td>Ethernet0 MAC Address:</td>
					<td><?php $tmp = read_emac_addr();echo $tmp; ?></td>
				</tr>
				<tr <?php if($is_pcie == 1) echo "style=\"display: none;\""; ?>>
					<td>Ethernet1 MAC Address:</td>
					<td><?php
					$tmp = exec("call_qcsapi get_mac_addr eth1_1");
					if((strpos($tmp, "API error") === FALSE))
					{
						echo $tmp;
					} else {
						echo "";
					}
					?></td>
				</tr>
				<tr <?php if($is_pcie == 0) echo "style=\"display: none;\""; ?>>
					<td>PCIE MAC Address:</td>
					<td><?php $tmp = read_pciemac_addr();echo $tmp; ?></td>
				</tr>
				<tr>
					<td>Wireless MAC Address:</td>
					<td><?php $tmp = read_wmac_addr();echo $tmp; ?></td>
				</tr>
				<tr>
					<td>BSSID:</td>
					<td><?php $tmp=exec("call_qcsapi get_bssid wifi0");echo $tmp;?></td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_refresh" id="btn_refresh" type="button" onclick="reload();" class="button">Refresh</button>
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

