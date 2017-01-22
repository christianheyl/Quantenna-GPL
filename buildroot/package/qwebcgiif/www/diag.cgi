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
SSID_wifi0=`call_qcsapi get_SSID wifi0`
curr_channel=`call_qcsapi get_channel wifi0`
scs_status=`call_qcsapi get_scs_status wifi0`
vsp_status=`call_qcsapi vsp wifi0 show`
Linux_version="`uname -a`"
hw_version="`cat /proc/hw_revision`"
customer_version="`/scripts/get_rev_num`"
rval_func_read_uptime="`uptime | awk '{print $1}'`"

func_Get_info()
{
	res=`echo "calcmd 31 0 4 0" > /sys/devices/qdrv/control`
	res=`info;cat /var/log/messages |tail -n 11 | cut -d ']' -f 2 >/tmp/calibration`
}

func_Backup_jffs2_proc()
{
	res=`mkdir /var/www/download 2>/dev/null`
	res=`cp -rf /proc/bootcfg/ /tmp/configuration/`
	res=`cp -rf /mnt/jffs2/ /tmp/configuration/`
	res=`tar -cvf /var/www/download/info.tar /tmp/configuration/ 2>/dev/null`
}


func_Write_system_configurations()
{
	res=`echo "[$1]" >> /tmp/configuration/configuration.txt 2>/dev/null`
	res=`$2 >> /tmp/configuration/configuration.txt 2>/dev/null `
}

func_Collect_information()
{
	res=`mkdir /tmp/configuration 2>/dev/null`
	res=`rm -rf /tmp/configuration/configuration.txt 2>/dev/null`
	res=`echo "Running board information" > /tmp/configuration/configuration.txt`

	func_Write_system_configurations "Linux_version" "uname -a"
	func_Write_system_configurations "Chip_version" "cat /proc/hw_revision"
	func_Write_system_configurations "System_uptime" "uptime"
	func_Write_system_configurations "Software_version" "/scripts/get_rev_num"
	func_Write_system_configurations "U-boot_parameters" "cat /proc/bootcfg/env"
	func_Write_system_configurations "Interface_info" "ifconfig"
	func_Write_system_configurations "WLAN" "cat /tmp/iwconfig"
	func_Write_system_configurations "PS" "ps"
	func_Write_system_configurations "Calibration" "cat /tmp/calibration"

	res=`tar -cvf /var/www/download/configuration.tar /tmp/configuration 2>/dev/null`
}

func_Get_info
func_Backup_jffs2_proc
func_Collect_information
%>

<body class="body" >
	<div class="top">
		<script type="text/javascript">
			createTop('<% echo -n $curr_version %>','<% echo -n $curr_mode %>');
		</script>
	</div>
	<div style="border:6px solid #9FACB7; width:800px; height:auto; background-color:#fff;">
<form enctype="multipart/form-data" action="diag_d.cgi" id="mainform" name="mainform" method="post">
		<!--choose table-->
		<div class="righttop">STATIC INFORMATION</div>
		<table>
				<tr>
					<td>
						<input type="submit" style="width:auto; height:auto;"  name="Dynamic_page" id="Dynamic_page" value="GotoDynamic" class="button" />
					</td>
					<!--<td >
						<p><button type="button" style="height:auto; width:auto" class="button" onClick="window.location='download/configuration.tar'">Download All Configurations</button></p>
					</td>-->
					<td >
						<p><button type="button" style="height:auto; width:auto;" class="button" onClick="window.location='download/info.tar'">Download jffs2 proc</button></p>
					</td>
				</tr>
		</table>
		<div class="rightmain">

		<!--data table-->
		<div >
		<table style="color:#0B7682; border:1px solid #9FACB7;">
			<div align="center"  ><b style="color:#FF3F00; font-size:25px">Version Information</b></div>
			<tr>
				<td width="200px" ><b style="color:#2A00FF;">Current Mode</b></td>
				<td width="600px">
					<div class="tablemain"><% echo -n $curr_mode %></div>
				</td>
			</tr>
			<tr>
				<td width="200px" ><b style="color:#2A00FF;">Wifi0 SSID</b></td>
				<td width="600px">
					<div class="tablemain"><% echo -n $SSID_wifi0 %></div>
				</td>
			</tr>
			<tr>
				<td width="200px" ><b style="color:#2A00FF;">Current Channel</b></td>
				<td width="600px">
					<div class="tablemain"><% echo -n $curr_channel %></div>
				</td>
			</tr>
			<tr>
				<td width="200px" ><b style="color:#2A00FF;">SCS status</b></td>
				<td width="600px">
					<div class="tablemain"><% echo -n $scs_status %></div>
				</td>
			</tr>
			<tr>
				<td width="200px" ><b style="color:#2A00FF;">VSP status</b></td>
				<td width="600px">
					<div class="tablemain"><% if [ "$vsp_status" == "VSP is not enabled" ]; then echo -n "Disabled";else echo -n "Enabled"; fi %></div>
				</td>
			</tr>
			<tr>
				<td width="200px" ><b style="color:#2A00FF;">Software Version</b></td>
				<td width="600px">
					<div class="tablemain"><% echo -n $customer_version %></div>
				</td>
			</tr>
			<tr>
				<td><b style="color:#2A00FF;">Chip Version</b></td>
				<td>
					<div class="tablemain"><% echo -n $hw_version %></div>
				</td>
			</tr>
			<tr>
				<td><b style="color:#2A00FF;">Linux Version</b></td>
				<td>
					<div class="tablemain"><% echo -n $Linux_version %></div>
				</td>
			</tr>
			<tr>
				<td><b style="color:#2A00FF;">System Uptime</b></td>
				<td>
					<div class="tablemain"><% echo "$rval_func_read_uptime" %></div>
				</td>
			</tr>

		</table>
		</div>


		<!--ouput table-->
		<div style="color:#0B7682;">
			<div align="center" height="20px" ><b style="color:#FF3F00; font-size:25px">System Information</b></div>
			<div style="border:1px solid #9FACB7; height:auto;overflow: auto;">
			<tr ><b style="color:#2A00FF;">WLAN Configuration</b></tr>
			<tr >
				<td>
					<div>
						<pre class="tablemain"><% iwconfig %></pre>
					</div>
				</td>
			</tr>
			</div>
			<div style="height:5px;"></div>
			<div style="border:1px solid #9FACB7; height:auto;overflow: auto;">
			<tr><b style="color:#2A00FF;">Interface Information</b></tr>
			<tr>
				<td>
					<div>
						<pre class="tablemain"><% ifconfig %></pre>
					</div>
				</td>
			</tr>
			</div>
			<div style="height:5px;"></div>
			<div style="border:1px solid #9FACB7; height:auto;overflow: auto;">
			<tr><b style="color:#2A00FF;">U-boot Parameters</b></tr>

			<tr>
				<td>
					<div>
						<pre class="tablemain"><% cat /proc/bootcfg/env %></pre>
					</div>
				</td>
			</tr>
			</div>
			<div style="height:5px;"></div>
			<div style="border:1px solid #9FACB7; height:auto;overflow: auto;">
			<tr><b style="color:#2A00FF;">Running Processes</b></tr>
			<tr>
				<td>
					<div>
						<pre class="tablemain"><% ps %></pre>
					</div>
				</td>
			</tr>
			</div>
			<!--<tr>
					<td class="divline" colspan="2";></td>
			</tr>-->


			<!--<tr>
					<td class="divline" colspan="2";></td>
			</tr>-->
			<div style="height:5px;"></div>
			<div style="border:1px solid #9FACB7; height:auto;overflow: auto;">
			<tr><b style="color:#2A00FF;">Files in /mnt/jffs2/</b></tr>
			<tr>
				<td>
					<div>
						<pre class="tablemain"><% ls -al /mnt/jffs2/ %></pre>
					</div>
				</td>
			</tr>
			</div>
			<div style="height:5px;"></div>
			<div style="border:1px solid #9FACB7; height:auto;overflow: auto;">
			<tr><b style="color:#2A00FF;">Calibration Information</b></tr>
			<tr>
				<td>
					<div>
						<pre class="tablemain"><% cat /tmp/calibration %></pre>
					</div>
				</td>
			</tr>
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
