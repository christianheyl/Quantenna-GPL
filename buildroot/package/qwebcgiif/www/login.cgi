#!/usr/bin/haserl
Content-type: text/html

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
	<title>Quantenna Communications</title>
	<link rel="stylesheet" type="text/css" href="/themes/style.css" media="screen" />

	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<meta http-equiv="expires" content="0" />
	<meta http-equiv="CACHE-CONTROL" content="no-cache" />
</head>
<script language="javascript" type="text/javascript" src="./js/cookiecontrol.js">
</script>

<%
confirm()
{
	echo "<script langauge=\"javascript\">alert(\"$1\");</script>";
}

filepath="/mnt/jffs2/admin.conf"

setvalue()
{
	if [ -n "$POST_user" -a -n "$POST_pwd" ]
	then
		if [ ! -e $filepath ]
		then
			confirm "Cannot find configura file"
			return
		fi

		correct_pwd=`cat $filepath | grep $POST_user | awk -F\, '{print $2}'`

		if [ -z "$correct_pwd" ]
		then
			confirm "Cannot find '$POST_user' account"
			return
		fi

		input_pwd=`echo -n "$POST_pwd" | md5sum | awk '{print $1}'`
		input_user=`echo -n "$POST_user" | md5sum | awk '{print $1}'`
		if [ "$input_pwd" = "$correct_pwd" ]
		then
			echo "<script language='javascript'>createCookie(\"p\", \"$input_user\", 2);</script>"
			echo "<script language='javascript'>location.href='status_device.cgi'</script>"
		else
			confirm "Wrong password"
		fi
	fi
}

setvalue
%>

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
<form enctype="multipart/form-data" action="login.cgi" id="mainform" name="mainform" method="post">
<body class="body">
	<div style="border:0px;	margin:0px auto; padding:0px; width:800px; height:150px;">
		<table height="100%" width="100%" cellpadding="0" cellspacing="0">
			<!--<tr>
				<td width="30%" style="background:url(images/logo.png) no-repeat"></td>
				<td width="70%" style="background:url(images/logo_02.png) no-repeat"></td>
			</tr>-->
		</table>
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
		<td style="background:url(images/lg_04_2.gif);"><input name="user" type="text" id="user" value="" class="textbox" style="margin-left:20px; width:200px;" onkeydown="keydownevent(event);"/></td>
		<td></td>
	</tr>
	<tr>
		<td height="25px"></td>
		<td style="background:url(images/lg_05_2.gif);"><img src="./images/lg_05_1.gif"></td>
		<td></td>
	</tr>
	<tr>
		<td height="90px"></td>
		<td style="background:url(images/lg_06_2.gif);" align="left" valign="top"><input name="pwd" type="password" class="textbox" id="pwd" value="" style="width:200px; margin-top:10px; margin-left:20px;" onkeydown="keydownevent(event);"/></td>
		<td></td>
	</tr>
	<tr>
		<td height="42px"></td>
		<td style="background:url(images/lg_07_2.gif);" align="center" valign="middle">
		<input type="image" name="start" src="./images/lg_07_1.gif" align="middle" onclick="validate();">
		</td>
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

