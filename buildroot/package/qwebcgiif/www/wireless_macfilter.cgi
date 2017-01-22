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

if [ -n "$curr_mode" -a "$curr_mode" = "Station" ]
then
	echo "<script langauge=\"javascript\">alert(\"Don\`t support in the Station mode.\");</script>"
	echo "<script language='javascript'>location.href='status_device.php'</script>"
	return
fi

curr_filter=""
curr_maclist=""

get_value()
{
	curr_filter=`call_qcsapi get_macaddr_filter wifi0`
	if [ "$curr_filter" = "0" ] #none
	then
		curr_maclist=""
	elif [ "$curr_filter" = "1" ] #black
	then
		curr_maclist=`call_qcsapi get_denied_macaddr wifi0 350`
	elif [ "$curr_filter" = "2" ] #white
	then
		curr_maclist=`call_qcsapi get_authorized_macaddr wifi0 350`
	fi
}

set_value()
{
	if [ "$POST_action" = "save" ]
	then
		if [ -n "$POST_filter" -a "$POST_filter" != "$curr_filter" ]
		then
			res=`call_qcsapi set_macaddr_filter wifi0 $POST_filter`
		fi

	elif [ "$POST_action" = "add" ]
	then
		if [ -n "$POST_mac" ]
		then
			if [ "$curr_filter" = "1" ] #black
			then
				res=`call_qcsapi deny_macaddr wifi0 $POST_mac`
			elif [ "$curr_filter" = "2" ] #white
			then
				res=`call_qcsapi authorize_macaddr wifi0 $POST_mac`
			fi
		fi

	elif [ "$POST_action" = "remove" ]
	then
		if [ -n "$POST_mac" ]
		then
			if [ "$curr_filter" = "1" ] #black
			then
				res=`call_qcsapi authorize_macaddr wifi0 $POST_mac`
			elif [ "$curr_filter" = "2" ] #white
			then
				res=`call_qcsapi deny_macaddr wifi0 $POST_mac`
			fi
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

function modechange()
{
	var filter='<% echo -n "$curr_filter" %>';
	var v=true;
	if (document.mainform.cmb_filter.value==filter)
	{
		v=false;
	}
	set_disabled('btn_add', v);
	set_disabled('btn_remove', v);
	set_disabled('txt_macaddr', v);
}

function CheckMac(address)
{
	var reg_test=/^([0-9a-fA-F]{2})(([//\s:-][0-9a-fA-F]{2}){5})$/;
	var tmp_test_mac=address.substring(0,2);
	var tmp_output=parseInt(tmp_test_mac,16).toString(2);
	String.prototype.Right = function(i) {return this.slice(this.length - i,this.length);};
	tmp_output = tmp_output.Right(1);
	if(!reg_test.test(address))
	{
		return false;
	}
	else
	{
		if(1 == tmp_output)
			return false;
		if("00:00:00:00:00:00"==address)
			return false;
		return true;
	}
}

function validate(act)
{
	var txt=document.getElementById("txt_macaddr");
	if (act==0)//Save Click
	{
		document.wireless_macfilter.action.value="save";
		document.wireless_macfilter.filter.value=document.mainform.cmb_filter.value;
	}
	else if (act==1)//Add
	{
		if (CheckMac(txt.value)==false)
		{
			alert("Invalid MAC address");
			txt.focus();
			return false;
		}
		document.wireless_macfilter.action.value="add";
		document.wireless_macfilter.mac.value=document.mainform.txt_macaddr.value;
	}
	else if (act==2)//rmove
	{
		if (CheckMac(txt.value)==false)
		{
			alert("Invalid MAC address");
			txt.focus();
			return false;
		}
		document.wireless_macfilter.action.value="remove";
		document.wireless_macfilter.mac.value=document.mainform.txt_macaddr.value;
	}
	document.wireless_macfilter.submit();
}

function load_value()
{
	init_menu();
	set_control_value('cmb_filter','<% echo -n "$curr_filter" %>', 'combox');
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
			<div class="righttop">WIRELESS - MAC FILTER</div>
			<div class="rightmain">
				<form name="mainform">
				<table class="tablemain">
					<tr>
						<td width="30%">MAC Address Filtering:</td>
						<td width="70%">
							<select name="cmb_filter" class="combox" id="cmb_filter" onchange="modechange();">
								<option value="0"> NONE </option>
								<option value="1"> Black List </option>
								<option value="2"> White List </option>
							</select>
							<button name="btn_save" id="btn_save" type="button" onclick="validate(0);"  class="button">Save</button>
						</td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
					<tr>
						<td>MAC Address:</td>
						<td>
							<input name="txt_macaddr" type="text" id="txt_macaddr" class="textbox" style="width:167px;" maxlength=17/>
							<button name="btn_add" id="btn_add" type="button" onclick="validate(1);" class="button">Add</button>
							<button name="btn_remove" id="btn_remove" type="button" onclick="validate(2);" class="button">Remove</button>
						</td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
					<tr>
						<td colspan="2" style="font-weight:bold"><% if [ "$curr_filter" = "1" ]; then echo "Black "; elif [ "$curr_filter" = "2" ]; then echo "White "; fi %>MAC Address List:</td>
					</tr>
					<tr>
						<td colspan="2"></td>
					</tr>
					<tr>
						<td colspan="2";>
						<div style="overflow-x:auto; overflow-y:auto; height:465px; border:1px solid #9FACB7;">
							<table class="tablemain">
							<%
								if [ "$curr_maclist" != "" ]
								then
									var=`echo $curr_maclist | awk -F/, '{print $0}' | sed "s/,/ /g"`
									for list in $var
									do
										echo "<tr><td>$list</td></tr>"
									done
								fi
							%>
							</table>
						</div>
						</td>
					</tr>
				</table>
				<div class="rightbottom"></div>
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

<form name="wireless_macfilter" method="POST" action="wireless_macfilter.cgi">
	<input type="hidden" name="action" />
	<input type="hidden" name="filter" />
	<input type="hidden" name="mac" />
</form>

</body>
</html>

