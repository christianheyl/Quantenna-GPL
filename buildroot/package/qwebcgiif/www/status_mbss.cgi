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

if [ -n "$curr_mode" -a "$curr_mode" = "Station" ]
then
	echo "<script langauge=\"javascript\">alert(\"Don\`t support in the Station mode.\");</script>"
	echo "<script language='javascript'>location.href='status_device.cgi'</script>"
	return
fi

%>

<%
#=============Get SSID=======================
ssid1=`call_qcsapi get_SSID wifi1`
if [ $? -ne 0 ]
then
	ssid1="N/A"
fi

ssid2=`call_qcsapi get_SSID wifi2`
if [ $? -ne 0 ]
then
	ssid2="N/A"
fi

ssid3=`call_qcsapi get_SSID wifi3`
if [ $? -ne 0 ]
then
	ssid3="N/A"
fi

ssid4=`call_qcsapi get_SSID wifi4`
if [ $? -ne 0 ]
then
	ssid4="N/A"
fi

ssid5=`call_qcsapi get_SSID wifi5`
if [ $? -ne 0 ]
then
	ssid5="N/A"
fi

ssid6=`call_qcsapi get_SSID wifi6`
if [ $? -ne 0 ]
then
	ssid6="N/A"
fi

ssid7=`call_qcsapi get_SSID wifi7`
if [ $? -ne 0 ]
then
	ssid7="N/A"
fi

ssid8=`call_qcsapi get_SSID wifi8`
if [ $? -ne 0 ]
then
	ssid8="N/A"
fi
#============================================
%>

<script type="text/javascript">

function reload()
{
	window.location.href="status_mbss.cgi";
}

function popnew(url)
{
	newwindow=window.open(url,'name');
	if (window.focus) {newwindow.focus();}
}

function validate(action_name)
{
	popnew("assoc_table.cgi?id="+action_name);
}
</script>
<!--script part-->
<script language="javascript" type="text/javascript" src="./js/menu.js">
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
		<input id="action" name="action" type="hidden" >
		<div class="right">
			<div class="righttop">STATUS - MBSS</div>
			<div class="rightmain">
			<form name="mainform" >
				<table class="tablemain">
					<tr>
						<td width="10%"></td>
						<td width="20%">SSID</td>
						<td width="10%" align="center">Broadcast</td>
						<td width="10%" align="center">Association</td>
						<td width="30%"></td>
					</tr>
					<tr <% if [ "$ssid1" = "N/A" ]; then echo "style=\"display: none;\""; fi %>>
						<td>MBSS 1: </td>
						<td><input type="text" class="textbox" readOnly="true" value="<%  echo -n "$ssid1" | sed -e 's/\"/\&\#34;/g' %>"/></td>
						<td align="center"><% tmp=`call_qcsapi get_option wifi1 broadcast_SSID`; echo -n $tmp; %></td>
						<td align="center"><% tmp=`call_qcsapi get_count_assoc wifi1`; echo -n $tmp; %></td>
						<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(1);"  class="button">Detail</button></td>
					</tr>
					<tr <% if [ "$ssid2" = "N/A" ]; then echo "style=\"display: none;\""; fi %>>
						<td>MBSS 2: </td>
						<td><input type="text" class="textbox" readOnly="true" value="<%  echo -n "$ssid2" | sed -e 's/\"/\&\#34;/g' %>"/></td>
						<td align="center"><% tmp=`call_qcsapi get_option wifi2 broadcast_SSID`; echo -n $tmp; %></td>
						<td align="center"><% tmp=`call_qcsapi get_count_assoc wifi2`; echo -n $tmp; %></td>
						<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(2);"  class="button">Detail</button></td>
					</tr>
					<tr <% if [ "$ssid3" = "N/A" ]; then echo "style=\"display: none;\""; fi %>>
						<td>MBSS 3: </td>
						<td><input type="text" class="textbox" readOnly="true" value="<%  echo -n "$ssid3" | sed -e 's/\"/\&\#34;/g' %>"/></td>
						<td align="center"><% tmp=`call_qcsapi get_option wifi3 broadcast_SSID`; echo -n $tmp; %></td>
						<td align="center"><% tmp=`call_qcsapi get_count_assoc wifi3`; echo -n $tmp; %></td>
						<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(3);"  class="button">Detail</button></td>
					</tr>
					<tr <% if [ "$ssid4" = "N/A" ]; then echo "style=\"display: none;\""; fi %>>
						<td>MBSS 4: </td>
						<td><input type="text" class="textbox" readOnly="true" value="<%  echo -n "$ssid4" | sed -e 's/\"/\&\#34;/g' %>"/></td>
						<td align="center"><% tmp=`call_qcsapi get_option wifi4 broadcast_SSID`; echo -n $tmp; %></td>
						<td align="center"><% tmp=`call_qcsapi get_count_assoc wifi4`; echo -n $tmp; %></td>
						<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(4);"  class="button">Detail</button></td>
					</tr>
					<tr <% if [ "$ssid5" = "N/A" ]; then echo "style=\"display: none;\""; fi %>>
						<td>MBSS 5: </td>
						<td><input type="text" class="textbox" readOnly="true" value="<%  echo -n "$ssid5" | sed -e 's/\"/\&\#34;/g' %>"/></td>
						<td align="center"><% tmp=`call_qcsapi get_option wifi5 broadcast_SSID`; echo -n $tmp; %></td>
						<td align="center"><% tmp=`call_qcsapi get_count_assoc wifi5`; echo -n $tmp; %></td>
						<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(5);"  class="button">Detail</button></td>
					</tr>
					<tr <% if [ "$ssid6" = "N/A" ]; then echo "style=\"display: none;\""; fi %>>
						<td>MBSS 6: </td>
						<td><input type="text" class="textbox" readOnly="true" value="<%  echo -n "$ssid6" | sed -e 's/\"/\&\#34;/g' %>"/></td>
						<td align="center"><% tmp=`call_qcsapi get_option wifi6 broadcast_SSID`; echo -n $tmp; %></td>
						<td align="center"><% tmp=`call_qcsapi get_count_assoc wifi6`; echo -n $tmp; %></td>
						<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(6);"  class="button">Detail</button></td>
					</tr>
					<tr <% if [ "$ssid7" = "N/A" ]; then echo "style=\"display: none;\""; fi %>>
						<td>MBSS 7: </td>
						<td><input type="text" class="textbox" readOnly="true" value="<%  echo -n "$ssid7" | sed -e 's/\"/\&\#34;/g' %>"/></td>
						<td align="center"><% tmp=`call_qcsapi get_option wifi7 broadcast_SSID`; echo -n $tmp; %></td>
						<td align="center"><% tmp=`call_qcsapi get_count_assoc wifi7`; echo -n $tmp; %></td>
						<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(7);"  class="button">Detail</button></td>
					</tr>
					<tr <% if [ "$ssid8" = "N/A" ]; then echo "style=\"display: none;\""; fi %>>
						<td>MBSS 8: </td>
						<td><input type="text" class="textbox" readOnly="true" value="<%  echo -n "$ssid8" | sed -e 's/\"/\&\#34;/g' %>"/></td>
						<td align="center"><% tmp=`call_qcsapi get_option wifi8 broadcast_SSID`; echo -n $tmp; %></td>
						<td align="center"><% tmp=`call_qcsapi get_count_assoc wifi8`; echo -n $tmp; %></td>
						<td><button name="btn_detail" id="btn_detail" type="button" onclick="validate(8);"  class="button">Detail</button></td>
					</tr>
					<tr>
						<td class="divline" colspan="5";></td>
					</tr>
				</table>
				<div id="result" style="visibility:hidden; text-align:left; margin-left:20px; margin-top:20px; font:16px Calibri, Arial, Candara, corbel, "Franklin Gothic Book";">
				</div>
				<div class="rightbottom">
					<button name="btn_refresh" id="btn_refresh" type="button" onclick="reload();"  class="button">Refresh</button>
				</div>
			</div>
		</div>
	</div>
	</form>
<div class="bottom">
<tr>
	<script type="text/javascript">
	createBot();
	</script>
</tr>
</div>
</body>
</html>

