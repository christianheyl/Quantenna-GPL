#!/usr/bin/haserl --upload-limit=4096 --upload-dir=/tmp
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
hw_config_id=`call_qcsapi get_bootcfg_param hw_config_id`
boot_cmd=`call_qcsapi get_bootcfg_param bootcmd`
if [ "$boot_cmd" == "bootselect" ]
then
	boot_sel=`call_qcsapi get_bootcfg_param bootselect`
fi
%>
<script type="text/javascript">
function validate()
{
	var res = document.getElementById("result");
	var res1 = document.getElementById("result1");
	res.style.visibility="visible";
	res1.style.visibility="hidden";
	set_disabled('btn_upgrade', true);
	document.mainform.submit();
}

function load_value()
{
	init_menu();
	var res = document.getElementById("result");
	var res1 = document.getElementById("result1");
	res1.style.visibility="visible";
	res.style.visibility="hidden";
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
			<div class="righttop">SYSTEM - UPGRADE</div>
			<div class="rightmain">
			<form enctype="multipart/form-data" action="system_upgrade.cgi" id="mainform" name="mainform" method="post">
				<table class="tablemain">
					<tr>
						<td width="20%">Upload Image:</td>
						<td width="80%">
							<input name="uploaded" type="file" class="button" style="width:400px;"/>
						</td>
					</tr>
				</table>
				<div id="result" name="result" style="visibility:hidden; text-align:left;  color:#0B7682; margin-left:20px; margin-top:10px; font:16px Calibri, Arial, Candara, corbel, "Franklin Gothic Book";">
				Loading the image file......Please wait.
				</div>
				<div id="result1" name="result1" style="text-align:left; color:#0B7682; margin-left:20px; margin-top:10px; font:16px Calibri, Arial, Candara, corbel, "Franklin Gothic Book";">
				<%
				if [ -n "$HASERL_uploaded_path" ]
				then
					if [ "$boot_cmd" != "bootselect" ]
					then
						res=`call_qcsapi flash_image_update $HASERL_uploaded_path live`
						if [ $? -ne 0 ]
						then
							res=`echo $res| cut -d ':' -f2`
							echo "Upgrade failed:$res <br>"
						else
							echo "Firmware upgraded successfully.<br>";
							echo "<br>Click <a href=\"system_rebooted.cgi\" style=\"font-weight:bold\"> here </a>to reboot the platform.";
						fi
					else
						if [ "$boot_sel" -eq 1 ]
						then
							res=`call_qcsapi flash_image_update $HASERL_uploaded_path live`
							if [ $? -ne 0 ]
							then
								res=`echo $res| cut -d ':' -f2`
								echo "Upgrade failed:$res <br>"
							else
								res=`call_qcsapi update_bootcfg_param bootselect 0`
								echo "Firmware upgraded successfully.<br>";
								echo "<br>Click <a href=\"system_rebooted.cgi\" style=\"font-weight:bold\"> here </a>to reboot the platform.";
							fi
						elif [ "$boot_sel" -eq 0 ]
						then
							res=`call_qcsapi flash_image_update $HASERL_uploaded_path safety`
							if [ $? -ne 0 ]
							then
								res=`echo $res| cut -d ':' -f2`
								echo "Upgrade failed:$res <br>"
							else
								res=`call_qcsapi update_bootcfg_param bootselect 1`
								echo "Firmware upgraded successfully.<br>";
								echo "<br>Click <a href=\"system_rebooted.cgi\" style=\"font-weight:bold\"> here </a>to reboot the platform.";
							fi
						else
							echo "bootselect has invalid value"
						fi
					fi
				fi
				%>
				</div>
				<div class="rightbottom" style="text-align:left; margin-left:20px; margin-top:20px;">
				<button name="btn_upgrade" id="btn_upgrade" type="button" onclick="validate();" class="button">Upgrade</button>
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

