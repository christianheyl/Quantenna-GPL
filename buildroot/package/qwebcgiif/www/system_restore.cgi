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

set_value()
{
	if [ "$POST_action" = "restore" ]
	then
		res=`/scripts/restore_default_config -nr -ip`
	elif [ "$POST_action" = "keepip" ]
	then
		res=`/scripts/restore_default_config -nr`
	fi
	if [ "$POST_prio" = "0" ]
	then
		res=`sed -i "1 s/.*/super,1b3231655cebb7a1f783eddf27d254ca,0/" /mnt/jffs2/admin.conf`
		res=`sed -i "2 s/.*/admin,21232f297a57a5a743894a0e4a801fc3,1/" /mnt/jffs2/admin.conf`
		res=`sed -i "3 s/.*/user,ee11cbb19052e40b07aac0ca060c23ee,2/" /mnt/jffs2/admin.conf`
	elif [ "$POST_prio" = "1" ]
	then
		res=`sed -i "2 s/.*/admin,21232f297a57a5a743894a0e4a801fc3,1/" /mnt/jffs2/admin.conf`
		res=`sed -i "3 s/.*/user,ee11cbb19052e40b07aac0ca060c23ee,2/" /mnt/jffs2/admin.conf`
	elif [ "$POST_prio" = "2" ]
	then
		res=`sed -i "3 s/.*/user,ee11cbb19052e40b07aac0ca060c23ee,2/" /mnt/jffs2/admin.conf`
	fi
	echo "<script language='javascript'>location.href='system_rebooted.cgi'</script>"
}

if [ -n "$POST_action" ]
then
	set_value
fi
%>

<script language="javascript" type="text/javascript">
function validate(act)
{
	if (act==0)//Restore All
	{
		document.system_restore.action.value="restore";
	}
	else if (act==1) //Restore but keep IP
	{
		document.system_restore.action.value="keepip";
	}
	document.system_restore.prio.value=privilege;
	document.system_restore.submit();
}
</script>

<body class="body" onload="init_menu();">
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
			<div class="righttop">SYSTEM - RESTORE</div>
			<div class="rightmain">
				<form name="mainform">
				<table class="tablemain">
					<tr>
						<td width="100%">
							<p>Restore all configuration files to factory defaults and reboot?&nbsp;&nbsp;
							<input type="button" name="btn_restore" id="btn_restore" value="YES" class="button" onclick="validate(0);"/></p>
							<p style="margin-top: 3em;">
							Restore configuration files to default and reboot, but retain IP settings?&nbsp;&nbsp;
							<input type="button" name="btn_restore_keep_ip" id="btn_restore_keep_ip" value="YES" class="button" onclick="validate(1);"/>
							</p>
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

<form name="system_restore" method="POST" action="system_restore.cgi">
	<input type="hidden" name="action" />
	<input type="hidden" name="prio" />
</form>

</body>
</html>

