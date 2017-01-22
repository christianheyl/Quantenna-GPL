#!/usr/bin/haserl
Content-type: text/html

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
	<title>Quantenna Communications</title>
	<link rel="stylesheet" type="text/css" href="/themes/style.css" media="screen" />

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
curr_ssid=`call_qcsapi get_SSID wifi0`


if [ "$curr_mode" = "Access point" ]
then
	confirm "This page is only for the Station Mode."
	echo -n "<script language='javascript'>location.href='status_device.php'</script>"
fi

set_value()
{
	if [ "$POST_action" = "connect" ]
	then
		res=`call_qcsapi verify_ssid wifi0 "$POST_select_ssid"`
		if [ $? -ne 0 ]
		then
			res=`call_qcsapi create_ssid wifi0 "$POST_select_ssid"`
		fi

		if [ "$POST_select_security" = "No" ]
		then
			res=`call_qcsapi SSID_set_authentication_mode wifi0 "$POST_select_ssid" NONE`

		elif [ "$POST_select_security" = "Yes" ]
		then
			#set the protocol
			if [ "$POST_select_protocol" = "1" ]
			then
				tmp_protocol="WPA"
			elif [ "$POST_select_protocol" = "2" ]
			then
				tmp_protocol="11i"
			elif [ "$POST_select_protocol" = "3" ]
			then
				tmp_protocol="WPAand11i"
			fi
			res=`call_qcsapi SSID_set_proto wifi0 "$POST_select_ssid" $tmp_protocol`

			#set the encryption
			if [ "$POST_select_encryption" = "1" ]
			then
				tmp_encryption="TKIPEncryption"
			elif [ "$POST_select_encryption" = "2" ]
			then
				tmp_encryption="AESEncryption"
			elif [ "$POST_select_encryption" = "3" ]
			then
				tmp_encryption="TKIPandAESEncryption"
			fi
			res=`call_qcsapi SSID_set_encryption_modes wifi0 "$POST_select_ssid" $tmp_encryption`

			#set the authentication
			if [ "$POST_select_authentication" = "1" ]
			then
				res=`call_qcsapi SSID_set_authentication_mode wifi0 "$POST_select_ssid" PSKAuthentication`
			fi
			res=`call_qcsapi SSID_set_key_passphrase wifi0 "$POST_select_ssid" 0 "$POST_select_pwd"`
			if [ $? -ne 0 ]
			then
				res=`call_qcsapi SSID_set_pre_shared_key wifi0 0 "$POST_select_pwd"`
			fi
		fi
		res=`call_qcsapi associate wifi0 "$POST_select_ssid"`

	elif [ "$POST_action" = "rescan" ]
	then
		res=`call_qcsapi start_scan wifi0`
		sleep 3
	fi
}

if [ -n "$POST_action" ]
then
	set_value
fi

%>
<script type="text/javascript">
function show_item(name)
{
	var item = document.getElementById(name);
	if (item)
		item.style.display = '';
}

function hide_item(name)
{
	var item = document.getElementById(name);
	if (item)
		item.style.display = 'none';
}

function click_ap(ssid,security,protocol,authentication,encryption)
{
	show_item("select_ap_ssid");
	show_item("select_ap_btn");
	show_item("select_ap_line");
	var apssid=document.getElementById("select_ssid");
	apssid.innerHTML="AP:"+ssid;
	if(security=="Yes")
	{
		show_item("select_ap_pwd");
	}
	else
	{
		hide_item("select_ap_pwd");
	}
	var form2=document.forms[1];
	document.forms[1].select_ssid.value=ssid;
	document.forms[1].select_security.value=security;
	document.forms[1].select_protocol.value=protocol;
	document.forms[1].select_authentication.value=authentication;
	document.forms[1].select_encryption.value=encryption;
}

function connect()
{
	document.forms[1].action.value="connect";
	if(document.forms[1].select_security.value=="Yes")
	{
		var txtpsk=document.getElementById("select_ap_passphrase");
		nonhex = /[^A-Fa-f0-9]/g;
		nonascii = /[^\x20-\x7E]/;
		backslash = /(\\)/g;
		if (txtpsk.value.length < 8 || txtpsk.value.length > 64)
		{
			txtpsk.focus();
			alert("Passphrase must contain between 8 and 64 ASCII characters");
			return false;
		}
		if ((nonascii.test(txtpsk.value)))
		{
			txtpsk.focus();
			alert("Only ASCII characters allowed");
			return false;
		}
		if ((backslash.test(txtpsk.value)))
		{
			txtpsk.focus();
			alert("Backslash is not allowed in password");
			return false;
		}
		if(txtpsk.value.length == 64 && (nonhex.test(txtpsk.value)))
		{
			txtpsk.focus();
			alert("Passkey must contain 64 hexadecimal (0-9, A-F) characters");
			return false;
		}
	}
	document.forms[1].select_pwd.value=document.forms[0].select_ap_passphrase.value;
	document.wireless_aplist.submit();
}

function rescan()
{
	document.forms[1].action.value="rescan";
	document.wireless_aplist.submit();
}
</script>

<body class="body" onload="load_value();">
	<div class="top">
		<script type="text/javascript">
			createTop('<% echo -n $curr_version %>','<% echo -n $curr_mode %>');
		</script>
	</div>
	<div style="border:6px solid #9FACB7; width:800px; background-color:#fff;">
		<div class="righttop">ACCESS POINT LIST</div>
		<form name="mainform">
		<div class="rightmain" style="color:#0B7682; font:16px Calibri, Arial;">
			Current SSID: <% echo -n "$curr_ssid"%>
			<table class="tablemain">
				<tr>
					<td width="10%" align="center" bgcolor="#96E0E2" ></td>
					<td width="30%" align="center" bgcolor="#96E0E2">SSID</td>
					<td width="30%" align="center" bgcolor="#96E0E2">Mac Address</td>
					<td width="10%" align="center" bgcolor="#96E0E2">Channel</td>
					<td width="10%" align="center" bgcolor="#96E0E2">RSSI</td>
					<td width="10%" align="center" bgcolor="#96E0E2">Security</td>
				</tr>
				<%
				count=`call_qcsapi get_results_AP_scan wifi0`
				if [ $? -ne 0 ]
				then
					count=0
				fi
				if [ "$count" -gt 0 ]
				then
					i=0
					while [ $i -lt $count ]
					do
						res=`call_qcsapi get_properties_AP wifi0 $i`
						let index=$i+1
						ssid=`echo $res | awk -F\" '{print $2}'`
						mac=`echo $res | awk '{print $2}'`
						channel=`echo $res | awk '{print $3}'`
						rssi=`echo $res | awk '{print $4}'`
						security=`echo $res | awk '{print $5}'`
						protocol=`echo $res | awk '{print $6}'`
						authentication=`echo $res | awk '{print $7}'`
						encryption=`echo $res | awk '{print $8}'`

						if [ "$security" = "0" ]
						then
							security="No"
						elif [ "$security" = "1" ]
						then
							security="Yes"
						fi
						echo "<tr onclick=\"click_ap('$ssid','$security','$protocol','$authentication','$encryption')\">"
						echo "<td align=\"center\">$index</td>"
						echo "<td>$ssid</td>"
						echo "<td align=\"center\">$mac</td>"
						echo "<td align=\"center\">$channel</td>"
						echo "<td align=\"center\">$rssi</td>"
						echo "<td align=\"center\">$security</td>"
						echo "</tr>"

						let i=$i+1
					done
				fi
				%>
				<tr>
					<td class="divline" colspan="6";></td>
				</tr>
				<tr id="select_ap_ssid" style="display:none;">
					<td colspan="6" id="select_ssid">
					</td>
				</tr>
				<tr id="select_ap_pwd" style="display:none;">
					<td colspan="6">
						Passphrase: <input type="text" name="select_ap_passphrase" id="select_ap_passphrase" class="textbox" width="142px"/>
					</td>
				</tr>
				<tr id="select_ap_btn" style="display:none;">
					<td colspan="6">
						<button name="btn_assoc" id="btn_assoc" type="button" onclick="connect();"  class="button">Connect</button>
					</td>
				</tr>
				<tr id="select_ap_line" style="display:none;">
					<td class="divline" colspan="6";></td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_rescan" id="btn_rescan" type="button" onclick="rescan();"  class="button">Rescan</button>
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

<form name="wireless_aplist" method="POST" action="wireless_aplist.cgi">
	<input type="hidden" name="action"/>
	<input type="hidden" name="select_ssid"/>
	<input type="hidden" name="select_security" />
	<input type="hidden" name="select_protocol" />
	<input type="hidden" name="select_authentication" />
	<input type="hidden" name="select_encryption" />
	<input type="hidden" name="select_pwd" />
</form>

</body>
</html>

