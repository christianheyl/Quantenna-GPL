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
curr_wpsstate=2
curr_wpspin=""
curr_appin=""
wps_started="0"
curr_running_state=""

get_value()
{
	if [ "$curr_mode" = "Access point" ]
	then
		tmp=`call_qcsapi get_wps_configured_state wifi0`
		if [ "$tmp" = "disabled" ]
		then
			curr_wpsstate=0
		elif [ "$tmp" = "not configured" ]
		then
			curr_wpsstate=1
		fi
		curr_appin=`call_qcsapi get_wps_ap_pin wifi0 0`
	else
		curr_wpspin=`call_qcsapi get_wps_sta_pin wifi0`
	fi
	curr_running_state=`call_qcsapi get_wps_state wifi0`
}

set_value()
{
	if [ "$POST_action" = "save" ]
	then
		if [ -n "$POST_wpsstate" -a "$POST_wpsstate" != "$curr_wpsstate" ]
		then
			res=`call_qcsapi set_wps_configured_state wifi0 $POST_wpsstate`
		fi

	elif [ "$POST_action" = "wpspbc" ]
	then
		if [ "$curr_mode" = "Access point" ]
		then
			res=`call_qcsapi registrar_report_button_press wifi0`
		else
			res=`call_qcsapi enrollee_report_button_press wifi0`
		fi
		confirm "WPS Started..."

	elif [ "$POST_action" = "wpspin" ]
	then
		if [ -n "$POST_wpspin" ]
		then
			if [ "$curr_mode" = "Access point" ]
			then
				tmp=`hostapd_cli wps_check_pin $POST_wpspin`
				if [ "$tmp" = "$POST_wpspin" ]
				then
					res=`call_qcsapi registrar_report_pin wifi0 $POST_wpspin`
					confirm "WPS Started..."
				else
					confirm "PIN validation failed."
					return
				fi
			else
				tmp=`wpa_cli wps_check_pin $POST_wpspin | grep FAIL`
				if [ "$tmp" = "" ]
				then
					res=`call_qcsapi enrollee_report_pin wifi0 $POST_wpspin`
					confirm "WPS Started..."
				else
					confirm "PIN validation failed."
					return
				fi
			fi
		fi

	elif [ "$POST_action" = "regenerate" ]
	then
		res=`call_qcsapi get_wps_ap_pin wifi0 1`
	fi
	wps_started=$POST_wps_started
}

get_value

if [ -n "$POST_action" ]
then
	set_value
	get_value
fi
%>

<script language="javascript" type="text/javascript">
var wps_start='<% echo -n "$wps_started" %>';
var get_start='<% echo -n "$GET_start" %>';

function modechange()
{
	var wpsstate='<% echo -n "$curr_wpsstate" %>';
	var v=true;
	if (document.mainform.cmb_wpsstate.value==wpsstate && wpsstate==2)
	{
		v=false;
	}
	set_disabled('btn_wpspbc', v);
	set_disabled('btn_wpspin', v);
	set_disabled('txt_wpspin', v);
	set_disabled('btn_regenerate', v);
}

function validate(act)
{
	if (act==0)//Save Click
	{
		document.wireless_wps.wps_started.value="0";
		document.wireless_wps.action.value="save";
		document.wireless_wps.wpsstate.value=document.mainform.cmb_wpsstate.value;
	}
	else if (act==1)//WPS PBC
	{
		document.wireless_wps.action.value="wpspbc";
		document.wireless_wps.wps_started.value="1";
	}
	else if (act==2)//WPS PIN
	{
		document.wireless_wps.action.value="wpspin";
		var wpspin_control = document.getElementById("txt_wpspin");
		var reg = /\s/g;
		wpspin_control.value = wpspin_control.value.replace(reg, "");
		reg = /[-]/g;
		wpspin_control.value=wpspin_control.value.replace(reg,"");

		if (wpspin_control.value.length !=4 && wpspin_control.value.length !=8)
		{
			alert("WPS Pin needs to be 4 characters or 8 characters");
			return false;
		}
		if((wpspin_control.value != "") && (!(/^\d+$/.test(wpspin_control.value))))
		{
			alert("The WPS Pin should be natural numbers");
			return false;
		}
		document.wireless_wps.wpspin.value=document.mainform.txt_wpspin.value;
		document.wireless_wps.wps_started.value="1";
	}
	else if (act==3)//WPS AP Pin regenerate
	{
		document.wireless_wps.wps_started.value="0";
		document.wireless_wps.action.value="regenerate";
	}
	document.wireless_wps.submit();
}

function load_value()
{
	init_menu();
	set_control_value('cmb_wpsstate','<% echo -n "$curr_wpsstate" %>', 'combox');
	set_control_value('txt_appin','<% echo -n "$curr_appin" %>', 'text');
	set_control_value('txt_wpspin','<% echo -n "$curr_wpspin" %>', 'text');
	var wpsstate='<% echo -n "$curr_wpsstate" %>';
	var v = (wpsstate < 2)? true : false;
	set_disabled('btn_wpspbc', v);
	set_disabled('btn_wpspin', v);
	set_disabled('txt_wpspin', v);
	set_disabled('btn_regenerate', v);

	if(wps_start == "0" && get_start != "1")
	{
		hide('tr_wps_state');
		hide('tr_wps_line');
	}
	else
	{
		show('tr_wps_state');
		show('tr_wps_line');
	}
}

function refresh_page()
{
	if(wps_start == "1" || get_start == "1" )
	{
		var tmp_state='<% echo -n $curr_running_state %>';
		if( tmp_state=='2 (WPS_SUCCESS)' || tmp_state=='3 (WPS_ERROR)' || tmp_state=='4 (WPS_TIMEOUT)' )
		{
			wps_start="2";
		}
		else
		{
			setTimeout("self.location.href='wireless_wps.cgi?start=1';",3000);
		}
	}
}

refresh_page();
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
			<div class="righttop">WIRELESS - WPS</div>
			<div class="rightmain">
				<form name="mainform">
					<table class="tablemain">
					<tr<% if [ "$curr_mode" = "Station" ]; then echo " style=\"display: none;\""; fi %>>
						<td width="40%">WPS State:</td>
						<td width="60%">
							<select name="cmb_wpsstate" class="combox" id="cmb_wpsstate" onchange="modechange(this)">
								<option value="0">Disabled</option>
								<option value="1">Not Configured</option>
								<option value="2">Configured</option>
							</select>
						</td>
					</tr>
					<tr>
						<td>WPS PBC:</td>
						<td>
							<button name="btn_wpspbc" id="btn_wpspbc" type="button" onclick="validate(1);" class="button" style="width:120px;">WPS PBC</button>
						</td>
					</tr>
					<tr>
						<td>WPS PIN:</td>
						<td>
							<input name="txt_wpspin" type="text" id="txt_wpspin" value="" class="textbox"/>
							<button name="btn_wpspin" id="btn_wpspin" type="button" onclick="validate(2);" class="button" style="width:120px;">WPS PIN</button>
						</td>
					</tr>
					<tr<% if [ "$curr_mode" = "Station" ]; then echo " style=\"display: none;\""; fi %>>
						<td>WPS AP PIN:</td>
						<td>
							<input name="txt_appin" type="text" id="txt_appin" class="textbox" readonly="readonly"/>
							<button name="btn_regenerate" id="btn_regenerate" type="button" onclick="validate(3);" class="button" style="width:120px;">Regenerate</button>
						</td>
					</tr>
					<tr id="tr_wps_line">
						<td class="divline" colspan="2"></td>
					</tr>
					<tr id="tr_wps_state">
						<td colspan="2">WPS Current State: <% echo -n $curr_running_state %></td>
					</tr>
					<tr<% if [ "$curr_mode" = "Station" ]; then echo " style=\"display: none;\""; fi %>>
						<td class="divline" colspan="2"></td>
					</tr>
				</table>
				<div class="rightbottom"<% if [ "$curr_mode" = "Station" ]; then echo " style=\"display: none;\""; fi %>>
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

<form name="wireless_wps" method="POST" action="wireless_wps.cgi">
	<input type="hidden" name="action" />
	<input type="hidden" name="wpsstate" />
	<input type="hidden" name="wpspin" />
	<input type="hidden" name="wps_started" />
</form>

</body>
</html>

