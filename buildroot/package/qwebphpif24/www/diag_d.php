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

function ToStatic() {
	window.location.href="diag_s.php";
}


function refresh_page() {
	window.name=(window.name=="1"?"0":"1");
	if(window.name == "1")ss();
}

function ss(){
	if(window.name == "1") {
		setTimeout("self.location.href='diag_d.php';",3000);
	}
}
ss();

function Log_status_submit() {
	var frm = document.forms[0];
	frm.submit();
}
</script>

<?php
$p_tmp_pid=exec("ps | grep get_phy_stats | grep -v grep | awk -F ' ' '{print $1}'");
if(""!=$p_tmp_pid)
	{$status_phy_stats=0;}
else
	{$status_phy_stats=1;}




function Get_log_info()
{
	exec("mkdir /var/www/download");
	exec("mkdir /tmp/messages_download/");
	exec("cp /var/log/messages /tmp/messages_download/Log_message");
	exec("tar -cvf /var/www/download/messages.tar /tmp/messages_download/");
}

Get_log_info();

if (isset($_POST['get_phy_stats']))
{
	$tmp_pid=exec("ps | grep get_phy_stats | grep -v grep | awk -F ' ' '{print $1}'");
	if(""!=$tmp_pid)
	{exec("kill $tmp_pid"); $status_phy_stats=1;}
	else
	{exec("/scripts/get_phy_stats > /dev/null &"); $status_phy_stats=0;}
}

?>

<body class="body" onload="focus();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="diag_d.php" id="status_change" name="status_change" method="post">
<div style="border:6px solid #9FACB7; width:1200px; height:auto; background-color:#fff;">
	<div class="righttop">Qdiagnostic Dynamic</div>
	<!--choose table-->
		<div class="rightmain">
			<input type="button"  name="Static_page" id="Static_page" value="Go To Static Page" style="width:180px;" onclick="ToStatic();" />
			<input type="button"  name="Dynamic_page" id="Dynamic_page" value="Go To Dynamic Page" style="width:180px" disabled="disabled"/>
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%"></td>
					<td width="70%"></td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
			</table>
			<textarea wrap=off cols="150" rows="31" style="width:100%; height:100%; font:14px Calibri, Candara, corbel, "Franklin Gothic Book";"><?php passthru("cat /var/log/messages| tail -n 30");?></textarea>

<!--button table-->
		<table class="tablemain" >
			<!--<div align="center" height="20px" style="background: #069"><b style="color:#FFF;font-size:150%; ">Change Log Status</b></div>-->
			<tr></tr>
			<tr>
				<td style="width:100px;">PHY_STATS</td>
				<td>
					<input type="button" style="width:80px; " value="<?php if($status_phy_stats==1){echo "Start";}else{echo "Stop";}?>"  onclick='javascript:Log_status_submit();'/>
					<input type="hidden" name="get_phy_stats" id="get_phy_stats"/>
					<!--<input type=button style="width:395px; height:60px;" value="Refresh" onclick="location.reload()">-->
				</td>
				<td style="width:60px;">DMSG</td>
				<td>
					<input type="button" style="width:80px;" value="Start" onclick="refresh_page();return false;" name="refresh_control" id="refresh_control">
					<script language="javascript">
						if(window.name==1)
						{
							document.getElementById('refresh_control').value='Stop';
						}
						else
						document.getElementById('refresh_control').value='Start';
					</script>
				</td>
				<td align="right">
					<button type="button" style="width:160px" onClick="window.location='download/messages.tar'">Download_log</button>
				</td>
			</tr>
		</table>
	</div>
</div>

<div class="bottom">Quantenna Communications</div>

</body>
</html>
