#!/usr/bin/haserl
Content-type: text/html

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
<script language="javascript" type="text/javascript" src="./js/common_js.js"></script>
<script language="javascript" type="text/javascript">
var privilege=get_privilege(2);
</script>
<%
source ./common_sh.sh

curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`
curr_psk=""

if [ -n "$POST_action" ]
then
	curr_psk=`cat /mnt/jffs2/admin.conf | grep $POST_user | awk -F\, '{print $2}'`
	input_psk=`echo -n "$POST_oldpsk" | md5sum | awk '{print $1}'`
	if [ "$curr_psk" != "$input_psk" ]
	then
		confirm "Error: Incorrect Password"
	else
		new_psk=`echo -n "$POST_newpsk" | md5sum | awk '{print $1}'`
		line=`cat /mnt/jffs2/admin.conf | grep -n $POST_user | awk -F\: '{print $1}'`
		res=`sed -i "$line s/$curr_psk/$new_psk/" /mnt/jffs2/admin.conf`
		confirm "The change has been saved"
	fi
fi
%>

<script language="javascript" type="text/javascript">
function validate()
{
	var oldpsk = document.getElementById('txt_oldpsk');
	var newpsk = document.getElementById('txt_newpsk');
	var newpskagain = document.getElementById('txt_newpskagain');
	if(oldpsk.value=='')
	{
		alert('The old passphrase should not be empty.');
		oldpsk.focus();
		return false;
	}
	if(newpsk.value=='')
	{
		alert('The new password should not be empty.');
		newpsk.focus();
		return false;
	}
	newpsk.value=newpsk.value.replace(/(\")/g, "");
	newpskagain.value=newpskagain.value.replace(/(\")/g, "");
	if (newpsk.value != newpskagain.value)
	{
		alert('The passwords do not match.');
		newpskagain.focus();
		return false;
	}
	document.tools_admin.user.value=document.mainform.txt_user.value;
	document.tools_admin.oldpsk.value=document.mainform.txt_oldpsk.value;
	document.tools_admin.newpsk.value=document.mainform.txt_newpsk.value;
	document.tools_admin.submit();
}

function load_value()
{
	init_menu();
	if (privilege==0) {
		set_control_value('txt_user','super', 'text');
	}
	else if (privilege==1) {
		set_control_value('txt_user','admin', 'text');
	}
	else if (privilege==2) {
		set_control_value('txt_user','user', 'text');
	}
	set_control_value('txt_oldpsk','', 'text');
	set_control_value('txt_newpsk','', 'text');
	set_control_value('txt_newpskagain','', 'text');
}
</script>

<body class="body" onload="load_value();">
	<div class="top">
		<script type="text/javascript">
			createTop('<% echo -n $curr_version %>','<% echo -n $curr_mode %>');
		</script>
	</div>
	<div class="container">
		<div class="left">
			<script type="text/javascript">
				createMenu('<% echo -n $curr_mode %>',privilege);
			</script>
		</div>
		<div class="right">
			<div class="righttop">TOOLS - ADMIN</div>
			<div class="rightmain">
				<form name="mainform">
				<table class="tablemain">
					<tr>
						<td width="40%">User Name:</td>
						<td width="60%">
							<input name="txt_user" type="text" id="txt_user" class="textbox"/ disabled="disabled">
						</td>
					</tr>
					<tr>
						<td>Old Passphrase:</td>
						<td>
							<input name="txt_oldpsk" type="password" id="txt_oldpsk" value="" class="textbox"/>
						</td>
					</tr>
					<tr>
						<td>New Passphrase:</td>
						<td>
							<input name="txt_newpsk" type="password" id="txt_newpsk" value="" class="textbox">
						</td>
					</tr>
					<tr>
						<td>New Passphrase Again:</td>
						<td>
							<input name="txt_newpskagain" type="password" id="txt_newpskagain" value="" class="textbox"/>
						</td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
				</table>
				<div class="rightbottom">
					<button name="btn_save_basic" id="btn_save_basic" type="button" onclick="validate();"  class="button">Save</button>
					<button name="btn_cancel_basic" id="btn_cancel_basic" type="button" class="button" onclick="load_value();">Cancel</button>
				</div>
				</form>
			</div>
		</div>
	</div>
<div class="bottom">
<tr>
	<script type="text/javascript">
	createBot();
	</script>
</tr>
</div>

<form name="tools_admin" method="POST" action="tools_admin.cgi">
	<input type="hidden" name="action" value="action" />
	<input type="hidden" name="user" />
	<input type="hidden" name="oldpsk" />
	<input type="hidden" name="newpsk" />
</form>

</body>
</html>

