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

<!--refresh botton scripts-->
<script language="javascript">
function refresh_page() {
	window.name=(window.name=="1"?"0":"1");
	if(window.name == "1")ss();
}

function ss(){
	if(window.name == "1") {
		setTimeout("self.location.href='diag_d.cgi';",5000);
	}
}
ss();

function Log_status_submit() {
	document.status_change.submit();
}

</script>



<%
source ./common_sh.sh
curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`

cat /var/log/messages| tail -n 30 > /tmp/tmp_log

p_tmp_pid="`ps | grep get_phy_stats | grep -v grep | awk -F ' ' '{print $1}'`"
if [ -n "$p_tmp_pid" ]
then
	status_phy_stats=0
else
	status_phy_stats=1
fi

func_Get_log_info()
{
	res=`mkdir /var/www/download 2>/dev/null`
	res=`mkdir /tmp/messages_download/ 2>/dev/null`
	res=`cp /var/log/messages /tmp/messages_download/Log_message 2>/dev/null`
	res=`tar -cvf /var/www/download/messages.tar /tmp/messages_download/ 2>/dev/null`
}

func_Get_log_info
if [ -n "$POST_get_phy_stats" ]
then
	tmp_pid="`ps | grep get_phy_stats | grep -v grep | awk -F ' ' '{print $1}'`"
	if [ -n "$tmp_pid" ]
	then
		kill $tmp_pid
		status_phy_stats=1
	else
		/scripts/get_phy_stats > /dev/null &
		status_phy_stats=0
	fi
fi
%>

<body class="body" >
	<div class="top">
		<script type="text/javascript">
			createTop('<% echo -n $curr_version %>','<% echo -n $curr_mode %>');
		</script>
	</div>
	<div style="border:6px solid #9FACB7; width:1200px; height:auto; background-color:#fff;">
	<form enctype="multipart/form-data" action="diag.cgi" id="mainform" name="mainform" method="post">
		<div class="righttop">QHARVEST_DYNAMIC</div>
		<!--choose table-->
		<div align="right">
			<tr>
				<td>
					<input type="submit" style="width:200px; height:auto;"  name="Static_page" id="Static_page" value="GotoStatic" class="button" />
				</td>
				<td >
					<button type="button" style="height:auto; width:200px" class="button" onClick="window.location='download/messages.tar'">Download Log</button>
				</td>
			</tr>
		</div>
	</form>
	<form enctype="multipart/form-data" action="diag_d.cgi" id="status_change" name="status_change" method="post">
		<div>
			<div align="left" class="tablemain">
				<tr>
					<td style="width:60px;"><b style="color:#FF3F00;">PHY_STATS</b></td>
					<td style="width:100px;">
						<input type="button" style="width:100px; height:auto;" class="button" value="<% if [ "$status_phy_stats" -eq 1 ]; then echo -n "Start"; else echo -n "Stop"; fi %>"  onclick="Log_status_submit();"/>
						<input type="hidden" name="get_phy_stats" id="get_phy_stats" value="1"/>
						<!--<input type=button style="width:395px; height:60px;" value="Refresh" onclick="location.reload()">-->
					</td>
					<td style="width:110px;"><b style="color:#FF3F00; ">Refreshing MSG</b></td>
					<td style="width:100px;">
						<input type="button" style="width:100px; height:auto;" class="button" value="Start" onclick="refresh_page();return false;" name="refresh_control" id="refresh_control">
						<script language="javascript">
							if(window.name==1)
							{
								document.getElementById('refresh_control').value='Stop';
							}
							else
							document.getElementById('refresh_control').value='Start';
						</script>
					</td>
				</tr>
			</div>
		 </div>
	</form>

	<!--data table-->
	<div style="border:1px solid #9FACB7;">
	<div  align="center" ><b style="color:#FF3F00; font-size:25px">Log Information</b></div>
	<table  class="tablemain">
		<tr >
			<td>
				<div style="width:1190px; overflow: auto;">
					<pre class="tablemain"><% cat /tmp/tmp_log %></pre>
				</div>
			</td>
		</tr>
	</table>
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

</body>
</html>
