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
</script>

<?php
$curr_mode=exec("call_qcsapi get_mode wifi0");
$curr_ip=read_br_ip();
exec("reboot -d 6 > /dev/null &");
echo "<meta http-equiv=\"refresh\" content=\"60; URL=http://$curr_ip\" />";
if (!isset($_SESSION['qtn_can_reboot']))
{
	header('Location: login.php');
	exit();
}

exec("reboot -d 6 > /dev/null &");
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
		<div class="righttop">SYSTEM - REBOOT</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr>
					<td colspan="2";>
					<?php
						echo "Rebooting.... <br><br>Click <a href=\"http://$curr_ip\" style=\"font-weight:bold\"> here </a> if you are not redirected automatically after 60s";
					?></td>
				</tr>
			</table>
			<div class="rightbottom" style="text-align:left; margin-left:20px; margin-top:70px;">
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

