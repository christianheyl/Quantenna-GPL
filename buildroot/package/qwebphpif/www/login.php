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
<?php
if (isset($_POST['user']) && isset($_POST['pwd']))
{
	$user=$_POST['user'];
	$pwd=md5(trim($_POST['pwd']));
	$flag=0;
	$file_path = "/mnt/jffs2/admin.conf";
	$fp = fopen($file_path, 'r');
	while(!feof($fp))
	{
		$buffer = stream_get_line($fp, 100, "\n");
		$arraylist=split(',',$buffer);
		if($arraylist[0]==$user && $arraylist[1]==$pwd)
		{
			$flag=1;
			break;
		}
	}
	fclose($fp);
	if ($flag==1)
	{
		session_start();
		$_SESSION['token']=md5(uniqid(rand(), TRUE));
		$_SESSION['qtn_privilege']=md5($user);
		$_SESSION['qtn_last_activity']=time();
		echo "<script language='javascript'>location.href='status_device.php'</script>";
	}
	else
	{echo "<script language='javascript'>alert(\"Login Failed\")</script>";}
}
?>

<script type="text/javascript">

function validate()
{
	if (checkbox()==true)
	{
		document.mainform.submit();
	}
}
function checkbox()
{
	var txt_user = document.getElementById('user');
	var txt_pwd = document.getElementById('pwd');
	if(txt_user.value=='')
	{
		alert('The user name should not be empty.');
		mainform.user.focus();
		return false;
	}
	if(txt_pwd.value=='')
	{
		alert('The password should not be empty.');
		mainform.pwd.focus();
		return false;
	}
	return true;
}

function keydownevent(event)
{
	if (event.keyCode==13) {validate();}
}

window.onload = function() {
var txt_user = document.getElementById('user');
txt_user.focus();
}
</script>
<form enctype="multipart/form-data" action="login.php" id="mainform" name="mainform" method="post">
<body class="body">
	<div style="border:0px;	margin:0px auto; padding:0px; width:800px; height:75px;">
		<a href="./login.php">
			<img src="./images/logo.png" border="0"/>
		</a>
	</div>
	<div style="border:0px;margin:0px auto; padding:0px; width:800px;">
		<table border=0 cellspacing="0">
			<tr>
				<th width="100px" height="20px" scope="col" style="border:0px;margin:0px auto; padding:0px;"></th>
				<th width="61px" scope="col" style="border:0px;margin:0px auto; padding:0px;"></th>
				<th width="379px" scope="col" style="border:0px;margin:0px auto; padding:0px;"></th>
				<th width="38px" scope="col" style="border:0px;margin:0px auto; padding:0px;"></th>
				<th width="100px" scope="col" style="border:0px;margin:0px auto; padding:0px;"></th>
			</tr>
			<tr>
				<td height="124px"></td>
				<td colspan="3" style="background:url(images/lg_01.gif) no-repeat top;padding-bottom:0px; padding-left:0px; padding-right:0px; margin:0px; padding-top:0px;"></td>
				<td></td>
			</tr>
			<tr>
				<td height="23px"></td>
				<td rowspan="8" style="background:url(images/lg_02_1.gif) no-repeat left bottom;"></td>
				<td style="background:url(images/lg_03_2.gif);"><img src="./images/lg_03_1.gif"></td>
				<td rowspan="8" style="background:url(images/lg_02_2.gif) no-repeat right bottom;"></td>
				<td></td>
			</tr>
			<tr>
				<td height="48px"></td>
				<td style="background:url(images/lg_04_2.gif);"><input name="user" type="text" id="user" value="" class="textbox" style="margin-left:10px; width:200px;" onkeydown="keydownevent(event);"/></td>
				<td></td>
			</tr>
			<tr>
				<td height="25px"></td>
				<td style="background:url(images/lg_05_2.gif);"><img src="./images/lg_05_1.gif"></td>
				<td></td>
			</tr>
			<tr>
				<td height="90px"></td>
				<td style="background:url(images/lg_06_2.gif);" align="left" valign="top"><input name="pwd" type="password" class="textbox" id="pwd" value="" style="width:200px; margin-top:10px; margin-left:10px;" onkeydown="keydownevent(event);"/></td>
				<td></td>
			</tr>
			<tr>
				<td height="42px"></td>
				<td style="background:url(images/lg_07_2.gif);" align="center" valign="middle"><img src="./images/lg_07_1.gif" align="middle" onclick="validate();"></td>
				<td></td>
			</tr>
			<tr>
				<td height="69px"></td>
				<td style="background:url(images/lg_08.gif) bottom;"></td>
				<td></td>
			</tr>
		</table>
	</div>
</body>
</form>
</html>

