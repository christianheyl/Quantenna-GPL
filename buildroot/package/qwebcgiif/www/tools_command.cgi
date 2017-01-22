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
var privilege=get_privilege(0);
</script>
<%
curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`

get_value()
{
	if [ -n "$POST_header" ]
	then
		echo "<script language=\"javascript\" type=\"text/javascript\">set_control_value('cmb_header','$POST_header', 'combox');</script>"
	fi
}

set_value()
{
	if [ -n "$POST_header" -a -n "$POST_command"  ]
	then
		if [ "$POST_header" = "none" ]
		then
			res=`$POST_command`
		else
			res=`call_qcsapi $POST_command`
		fi
	fi
}

get_value

if [ -n "$POST_action" ]
then
	set_value
	get_value
fi
%>

<script language="javascript" type="text/javascript">
function validate()
{
	var txt_command = document.getElementById('txt_command');
	if(txt_command.value=='')
	{
		alert('The command should not be empty.');
		mainform.txt_command.focus();
		return false;
	}

	document.tools_command.header.value=document.mainform.cmb_header.value;
	document.tools_command.command.value=document.mainform.txt_command.value;
	document.tools_command.submit();
}

function load_value()
{
	init_menu();
	var txt_command = document.getElementById('txt_command');
	txt_command.focus();
	set_control_value('cmb_header','<% echo -n "$POST_header" %>', 'combox');
}

function keydownevent(event)
{
	if (event.keyCode==13) {validate();}
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
			<div class="righttop">TOOLS - COMMAND</div>
			<div class="rightmain">
				<form name="mainform">
				<table class="tablemain">
					<tr>
						<td width="90%">
							<select name="cmb_header" class="combox" id="cmb_header" style="width:100px;">
								<option value="none">None</option>
								<option value="call_qcsapi">call_qcsapi</option>
							</select>
						</td>
						<td width="10%"></td>
					</tr>
					<tr>
						<td>
							<input name="txt_command" type="text" id="txt_command" class="textbox" style="width:500px;" onkeydown="keydownevent(event);"/>
						</td>
						<td>
							<button name="btn_send" id="btn_send" type="button" onclick="validate();"  class="button" style="width:55px;">Send</button>
						</td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
					<tr>
						<td colspan="2">
							<textarea class="tablemain" name="txt_res" id="txt_res" wrap=off cols="150" rows="25" ><% echo -n "$res" %></textarea>
						</td>
					</tr>
				</table>
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

<form name="tools_command" method="POST" action="tools_command.cgi">
	<input type="hidden" name="action" value="action" />
	<input type="hidden" name="header" />
	<input type="hidden" name="command" />
</form>

</body>
</html>

