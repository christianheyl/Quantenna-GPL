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

wpa_file="/mnt/jffs2/wpa_supplicant.conf"
curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`
curr_ssid=""
curr_brdcst_ssid=""
curr_encryption="NONE"
curr_pwd=""

func_get_sta_proto()
{
	authentication_mode=`call_qcsapi SSID_get_proto wifi0 $1`
	if [ "$authentication_mode" = "QCS API error 1005: Configuration error" ]
	then
		curr_encryption="NONE"
	elif [ "$authentication_mode" = "11i" ]
	then
		curr_encryption="AES"
	elif [ "$authentication_mode" = "WPAand11i" ]
	then
		curr_encryption="TKIPandAES"
	fi
}

#Input 1 is the ssid, 2 is the encryption mode NONE, 11i or WPAand 11i
func_set_sta_proto()
{
	if [ "$2" = "NONE" ]
	then
		res=`call_qcsapi SSID_set_authentication_mode wifi0 $1 NONE`
	else
		res=`call_qcsapi SSID_set_authentication_mode wifi0 $1 PSKAuthentication`
		res=`call_qcsapi SSID_set_proto wifi0 $1 $2`
	fi
}

func_get_ap_proto()
{
	beacon=`call_qcsapi get_beacon wifi0`
	encryption=`call_qcsapi get_WPA_encryption_modes wifi0`
	if [ "$beacon" = "Basic" ]
	then
		curr_encryption="NONE"
	elif [ "$beacon" = "11i" -a "$encryption" = "AESEncryption" ]
	then
		curr_encryption="AES"
	elif [ "$beacon" = "WPAand11i" -a "$encryption" = "TKIPandAESEncryption" ]
	then
		curr_encryption="TKIPandAES"
	fi
}

func_set_ap_proto()
{
	if [ "$1" = "NONE" ]
	then
		res=`call_qcsapi set_beacon wifi0 Basic`
		res=`call_qcsapi set_WPA_encryption_modes wifi0 AESEncryption`
	elif [ "$1" = "AES" ]
	then
		res=`call_qcsapi set_beacon wifi0 11i`
		res=`call_qcsapi set_WPA_encryption_modes wifi0 AESEncryption`
	elif [ "$1" = "TKIPandAES" ]
	then
		res=`call_qcsapi set_beacon wifi0 WPAand11i`
		res=`call_qcsapi set_WPA_encryption_modes wifi0 TKIPandAESEncryption`
	fi
}

get_value()
{
	func_wr_wireless_conf channel
	curr_channel=$rval_func_wr_wireless_conf

	curr_channel_from_api=`call_qcsapi get_channel wifi0`
	channel_scan_status=`iwpriv wifi0 get_scanstatus | cut -d ':' -f2`

	if [ "$channel_scan_status" -eq "0" ]
	then
		channel_status="Current Ch $curr_channel_from_api"
	else
		channel_status="Ch Scaning $curr_channel_from_api"
	fi

	func_wr_wireless_conf bw
	curr_bandwidth=$rval_func_wr_wireless_conf

	curr_region=`cat /etc/region`

	if [ "$curr_mode" = "Station" ]
	then
		curr_ssid=`call_qcsapi get_ssid wifi0`
		if [ "$curr_ssid" != "" ]
		then
			curr_encryption="NONE"
			curr_pwd=""
		else
			func_get_sta_proto $curr_ssid
			curr_pwd=`call_qcsapi SSID_get_key_passphrase wifi0 $curr_ssid 0`
			if [ $? -ne 0 ]
			then
				curr_pwd=`call_qcsapi SSID_get_pre_shared_key wifi0 $curr_ssid 0`
				if [ $? -ne 0 ]
				then
					curr_pwd=""
				fi
			fi
		fi
	else
		curr_ssid=`call_qcsapi get_ssid wifi0`
		curr_brdcst_ssid=`call_qcsapi get_option wifi0 broadcast_SSID`
		func_get_ap_proto
		curr_pwd=`call_qcsapi get_passphrase wifi0 0`
		if [ $? -ne 0 ]
		then
			curr_pwd=`call_qcsapi get_pre_shared_key wifi0 0`
			if [ $? -ne 0 ]
			then
				curr_pwd=""
			fi
		fi
	fi
}

set_value()
{
	if [ -n "$POST_ssid" -a "$POST_ssid" != "$curr_ssid" ]
	then
		if [ "$curr_mode" = "Station" ]
		then
			res=`call_qcsapi verify_ssid wifi0 "$POST_ssid"`
			if [ $? -ne 0 ]
			then
				res=`call_qcsapi create_ssid wifi0 "$POST_ssid"`
			fi
			func_set_sta_proto $POST_ssid $POST_encryption
			res=`call_qcsapi SSID_set_key_passphrase wifi0 "$POST_ssid" 0 "$POST_pwd"`
			if [ $? -ne 0 ]
			then
				res=`call_qcsapi SSID_set_pre_shared_key wifi0 "$POST_ssid" 0 "$POST_pwd"`
			fi
			res=`call_qcsapi associate wifi0 "$POST_ssid"`
		else
			res=`call_qcsapi set_ssid wifi0 "$POST_ssid"`
		fi
	fi

	if [ -n "$POST_broadcast_ssid" -a "$POST_broadcast_ssid" != "$curr_brdcst_ssid" ]
	then
		if [ "$curr_mode" = "Access point" ]
		then
			res=`call_qcsapi set_option wifi0 broadcast_SSID $POST_broadcast_ssid`
		fi
	fi

	if [ -n "$POST_encryption" -a "$POST_encryption" != "$curr_encryption" ]
	then
		if [ "$curr_mode" = "Station" ]
		then
			func_set_sta_proto $curr_ssid $POST_encryption
		else
			func_set_ap_proto $POST_encryption
		fi
	fi

	if [ "$POST_encryption" != "NONE" -a -n "$POST_pwd" -a "$POST_pwd" != "$curr_pwd" ]
	then
		if [ "$curr_mode" = "Station" ]
		then
			res=`call_qcsapi SSID_set_key_passphrase wifi0 "$curr_ssid" 0 "$POST_pwd"`
			if [ $? -ne 0 ]
			then
				res=`call_qcsapi SSID_set_pre_shared_key wifi0 "$curr_ssid" 0 "$POST_pwd"`
			fi
		else
			res=`call_qcsapi set_passphrase wifi0 0 "$POST_pwd"`
			if [ $? -ne 0 ]
			then
				res=`call_qcsapi set_pre_shared_key wifi0 0 "$POST_pwd"`
			fi
		fi
	fi

	if [ "$curr_mode" != "Station" -a -n "$POST_p_channel_selection" -a "$curr_channel" != "$POST_p_channel_selection" ]
	then
		res=`call_qcsapi set_channel wifi0 $POST_p_channel_selection`
		func_wr_wireless_conf channel $POST_p_channel_selection
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
function populate_channellist()
{
<%
res=`call_qcsapi get_list_regulatory_channels $curr_region $curr_bandwidth`
if [ $? -eq 0 ]
then
	res=`call_qcsapi get_list_regulatory_channels $curr_region $curr_bandwidth | sed 's/,/\n/g' > /tmp/channel_list`
else
	if [ $curr_bandwidth -eq 20 ]
	then
		echo "36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 149, 153, 157, 161, 165" | sed 's/,/\n/g' > /tmp/channel_list
	else
		echo "36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 149, 153, 157, 161" | sed 's/,/\n/g' > /tmp/channel_list
	fi

fi

option_length=`cat /tmp/channel_list | wc -l`
let option_length=$option_length+1
%>
	powerlist = document.getElementById("channel_selection");
	if(!powerlist) return;
	powerlist.options.length = <% echo -n $option_length %>;
	powerlist.options[0].text = "Auto"; powerlist.options[0].value = "0";
<%
channel_value=`cat /tmp/channel_list | sed -n '1p;'`
count_for_channel_list=1
count_for_channel_value=1
while [ -n "$channel_value" ]
do
	echo "powerlist.options[$count_for_channel_list].text=$channel_value; powerlist.options[$count_for_channel_list].value = $channel_value;"
	let count_for_channel_list=$count_for_channel_list+1;
	let count_for_channel_value=$count_for_channel_value+1;
	channel_value=`cat /tmp/channel_list | sed -n "$count_for_channel_value p;"`
done
%>
}

function modechange()
{
	var value;
	var control;
	control=document.getElementById("cmb_encryption");
	value = (control.selectedIndex > 0)? true : false;
	set_visible('tr_passphrase', value);
}

function display_control()
{
	var mode = '<% echo -n "$curr_mode" %>';
	var encryption = '<% echo -n "$curr_encryption" %>';

	if (mode == "Station")
	{
		set_visible('tr_broadcast_ssid', false);
		set_visible('channel_selection', false);
	}
	else
	{
		set_visible('btn_aplist', false);
	}

	if (encryption == "NONE") {
		set_visible('tr_passphrase', false);
	}

	set_disabled("txt_current_channel",true);
}

function validate(act)
{
	if (act==0)//Save Click
	{
		var c_encryption='<% echo -n "$curr_encryption" %>'
		var mode = '<% echo -n "$curr_mode" %>';
		var mf = document.mainform;
		nonhex = /[^A-Fa-f0-9]/g;
		nonascii = /[^\x20-\x7E]/;
		var ssid = document.getElementById("txt_ssid");
		var broadcast = document.getElementById("chk_broadcast");
		ssid.value = ssid.value.replace(/(\")/g, '\"');

		if (ssid.value.length < 1 || ssid.value.length > 32)
		{
			alert("SSID must contain between 1 and 32 ASCII characters");
			return false;
		}
		if ((nonascii.test(ssid.value)))
		{
			alert("Only ASCII characters allowed in SSID");
			return false;
		}

		if(document.mainform.cmb_encryption.selectedIndex > 0)
		{
			pw = document.getElementById("txt_pwd");
			pw.value=pw.value.replace(/(\")/g, '\"');
			if (pw.value.length < 8 || pw.value.length > 64)
			{
				alert("Allowed Passphrase is 8 to 63 ASCII characters or 64 Hexadecimal digits");
				return false;
			}
			if ((nonascii.test(pw.value)))
			{
				alert("Allowed Passphrase is 8 to 63 ASCII characters or 64 Hexadecimal digits");
				return false;
			}

			if(pw.value.length == 64 && (nonhex.test(pw.value)))
			{
				alert("Allowed Passphrase is 8 to 63 ASCII characters or 64 Hexadecimal digits");
				return false;
			}

		}
		else if(document.mainform.cmb_encryption.selectedIndex == "0" && c_encryption!="NONE")
		{
			if (mode != "Station")
			{
				var tag = confirm('Disable the security?');
				if( tag != true )
				{
					return false;
				}
			}
		}

		if (broadcast.checked==false)
		{
			if (mode != "Station")
			{
				var tag1=confirm('Disable the broadcast SSID? WPS will also be disabled!');
				if( tag1 != true )
				{
					return false;
				}
			}
		}

		document.wireless_basic.ssid.value=document.mainform.txt_ssid.value;
		if (document.mainform.chk_broadcast.checked==true)
		{
			document.wireless_basic.broadcast_ssid.value="TRUE";
		}
		else
		{
			document.wireless_basic.broadcast_ssid.value="FALSE";
		}
		document.wireless_basic.encryption.value=document.mainform.cmb_encryption.value;
		document.wireless_basic.pwd.value=document.mainform.txt_pwd.value;
		document.wireless_basic.p_channel_selection.value=document.mainform.channel_selection.value;

		document.wireless_basic.submit();
	}
	else if (act==1)//AP List Click
	{
		popnew("wireless_aplist.cgi");
	}
}

function load_value()
{
	populate_channellist();
	display_control();
	init_menu();
	set_control_value('chk_broadcast','<% echo -n "$curr_brdcst_ssid" %>', 'checkbox');
	set_control_value('cmb_encryption','<% echo -n "$curr_encryption" %>', 'combox');
	set_control_value('channel_selection','<% echo -n "$curr_channel" %>', 'combox');
	set_control_value('txt_current_channel','<% echo -n "$channel_status" %>', 'text');
}

function reload()
{
	window.location.href="wireless_basic.cgi";
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
			<div class="righttop">WIRELESS - BASIC</div>
			<div class="rightmain">
				<form name="mainform">
				<table class="tablemain">
					<tr>
						<td width="35%">Wireless Band:</td>
						<td width="66%">
							<select name="cmb_wirelessmode" class="combox" id="cmb_wirelessmode" disabled="disabled">
								<option selected value="11a">802.11N_5GHZ</option>
							</select>
						</td>
					</tr>
					<tr>
						<td>SSID:</td>
						<td>
							<input name="txt_ssid" type="text" id="txt_ssid" class="textbox" value="<%  echo -n "$curr_ssid" | sed -e 's/\"/\&\#34;/g' %>" />
							<button name="btn_aplist" id="btn_aplist" type="button" onclick="validate(1);" class="button">Scan AP</button>
						</td>
					</tr>
					<tr id="tr_broadcast_ssid">
						<td>Broadcast SSID:</td>
						<td>
							<input name="chk_broadcast" id="chk_broadcast" type="checkbox"  class="checkbox"/>
						</td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
					<tr>
						<td>Encryption:</td>
						<td>
							<select name="cmb_encryption" class="combox" id="cmb_encryption" onchange="modechange();">
								<option value="NONE"> NONE-OPEN </option>
								<option value="AES"> WPA2-PSK AES </option>
								<option value="TKIPandAES"> WPA2 + WPA (mixed mode) </option>
							</select>
						</td>
					</tr>
					<tr id="tr_passphrase">
						<td>Passphrase:</td>
						<td><input name="txt_pwd" type="text" id="txt_pwd" class="textbox" value="<% if [ \"$curr_encryption\" != \"NONE\" ]; then echo -n "$curr_pwd" | sed -e 's/\"/\&\#34;/g' ; fi ; %>" /></td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
					<tr id="tr_channel">
						<td>Channel:</td>
						<td>
							<select id="channel_selection" name="channel_selection" class="combox" onchange="modechange(this)" >
							</select>
							<input name="txt_current_channel" type="text" id="txt_current_channel" class="textbox"/>
						</td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
				</table>
				<div class="rightbottom">
					<button name="btn_save_basic" id="btn_save_basic" type="button" onclick="validate(0);"  class="button">Save</button>
					<button name="btn_cancel_basic" id="btn_cancel_basic" type="button" onclick="reload();" class="button">Cancel</button>
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

<form name="wireless_basic" method="POST" action="wireless_basic.cgi">
	<input type="hidden" name="action" value="action" />
	<input type="hidden" name="ssid" />
	<input type="hidden" name="broadcast_ssid" />
	<input type="hidden" name="encryption" />
	<input type="hidden" name="pwd" />
	<input type="hidden" name="p_channel_selection" />
</form>

</body>
</html>

