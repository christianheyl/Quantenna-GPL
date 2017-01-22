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
var privilege=get_privilege(1);
</script>

<%
source ./common_sh.sh
curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`

if [ -n "$POST_phy_status_control" ]
then
	tmp_pid="`ps | grep get_phy_stats | grep -v grep | awk -F ' ' '{print $1}'`"
	if [ -n "$tmp_pid" ]
	then
		kill $tmp_pid
	else
		/scripts/get_phy_stats > /dev/null &
	fi
fi

%>

<!--refresh botton scripts-->
<script language="javascript">
function refresh_page() {
	window.name=(window.name=="1"?"0":"1");
	document.mainform.submit();
}

function ss(){
	if(window.name == "1") {
		setTimeout("self.location.href='tools_log.cgi';",5000);
	}
}
ss();


</script>


<!--script part-->

<body class="body" onload="focus();">
	<div class="top">
		<script type="text/javascript">
			createTop('<% echo -n $curr_version %>','<% echo -n $curr_mode %>');
		</script>
	</div>
	<form enctype="multipart/form-data" action="tools_log.cgi" id="mainform" name="mainform" method="post">
	<div style="border:6px solid #9FACB7; width:1200px; height:auto; background-color:#fff;">
		<div class="righttop">TOOLS - LOG</div>
			<div class="rightmain">
				<table class="tablemain" style=" height:auto">
					<tr>
						<td width="30%">
						<input type="button" style="width:80px; height:60px;" class="button" value="Start" onclick="refresh_page();return false;" name="refresh_control" id="refresh_control">
						<input type="hidden" name="phy_status_control" id="phy_status_control" value="action">
						<script language="javascript">
							if(window.name==1)
							{
								document.getElementById('refresh_control').value='Stop';
							}
							else
							document.getElementById('refresh_control').value='Start';

						</script>
						</td>
						<td width="70%"></td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
				</table>
				<textarea class="tablemain" name="txt_log" id="txt_log" wrap=off cols="150" rows="31" ><% cat /var/log/messages | tail -30 %></textarea>
			</div>
		</div>
	</div>
	</form>
	</div>
<div class="bottom">
<tr>
	<script type="text/javascript">
	createBot();
	</script>
</tr>
</div>

</body>
</html>

