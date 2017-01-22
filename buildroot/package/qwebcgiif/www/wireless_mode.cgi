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
short_mode=""

get_value()
{
	if [ "$curr_mode" = "Access point" ]
	then
		short_mode="ap"
	elif [ "$curr_mode" = "Station" ]
	then
		short_mode="sta"
	fi
}

set_value()
{
	if [ -n "$POST_mode" -a "$POST_mode" != "$short_mode"  ]
	then
		func_wr_wireless_conf mode $POST_mode
		echo "<script language='javascript'>location.href='system_rebooted.cgi'</script>"
	fi
}

get_value

if [ -n "$POST_action" ]
then
	set_value
fi
%>

<script language="javascript" type="text/javascript">
function validate(act)
{
	if (act==0)//Save Click
	{
		var tag = confirm('Change the device mode need to reboot the device. Continue?');
		if( tag != true )
		{
			return false;
		}
		document.wireless_mode.mode.value=document.mainform.cmb_mode.value;
		document.wireless_mode.submit();
	}
}

function load_value()
{
	init_menu();
	set_control_value('cmb_mode','<% echo -n "$short_mode" %>', 'combox');
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
		<div class="righttop">WIRELESS - MODE</div>
		<div class="rightmain">
			<form name="mainform">
			<table class="tablemain">
				<tr>
					<td width="40%">Device Mode:</td>
					<td width="60%">
						<select name="cmb_mode" class="combox" id="cmb_mode">
							<option value="ap"> Access Point </option>
							<option value="sta"> Station </option>
						</select>
					</td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_save_basic" id="btn_save_basic" type="button" onclick="validate(0);"  class="button">Save</button>
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

<form name="wireless_mode" method="POST" action="wireless_mode.cgi">
	<input type="hidden" name="action" value="action" />
	<input type="hidden" name="mode" />
</form>

</body>
</html>

