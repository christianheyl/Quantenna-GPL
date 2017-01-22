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
$privilege = get_privilege(1);

?>


<script type="text/javascript">
var privilege="<?php echo $privilege; ?>";
</script>

<body class="body" onload="focus();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="tools_logs.php" id="mainform" name="mainform" method="post">
<?php
if(isset($_POST['btn_stop']))
{
	$process_id = exec("ps | grep get_phy_stats | grep -v \"grep\" | awk '{print $1}'");
	if($process_id != "")
	{
		exec("kill -9 $process_id");
	}
}
?>
<div style="border:6px solid #9FACB7; width:1200px; height:700px; background-color:#fff;">
	<div class="righttop">TOOLS - LOG</div>
		<div class="rightmain">
			<input type="submit" name="btn_start" id="btn_start" value="Start" class="button"/>
			<input type="submit" name="btn_f_stop" id="btn_f_stop" value="Stop" class="button" disabled="disabled"/>
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%"></td>
					<td width="70%"></td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
			</table>
			<textarea name="txt_log" id="txt_log" wrap=off cols="150" rows="31" style="width:100%; height:100%; font:14px Calibri, Candara, corbel, "Franklin Gothic Book";"></textarea>
		</div>
	</div>
</div>
</form>
</div>
<div class="bottom">Quantenna Communications</div>

</body>
</html>

