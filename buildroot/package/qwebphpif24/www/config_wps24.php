#!/usr/lib/cgi-bin/php-cgi

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
	<title>Quantenna Communications</title>
	<link rel="stylesheet" type="text/css" href="./themes/style.css" media="screen" />

	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<meta http-equiv="expires" content="0" />
	<meta http-equiv="CACHE-CONTROL" content="no-cache" />
</head>

<script language="javascript" type="text/javascript" src="./SpryAssets/SpryTabbedPanels.js"></script>
<script language="javascript" type="text/javascript" src="./js/cookiecontrol.js"></script>
<script language="javascript" type="text/javascript" src="./js/menu.js"></script>
<script language="javascript" type="text/javascript" src="./js/webif.js"></script>
<?php
include("common.php");
$privilege = get_privilege(2);
?>

<script type="text/javascript">
var privilege="<?php echo $privilege; ?>";
</script>

<?php
$curr_mode=exec("qweconfig get mode.wlan1");
//Need add check for WPS support
$curr_wpsstate=2;
$curr_wpspin="";
$curr_appin="";
$pre_check_bf_mf_for_wps=0;

function get_value()
{
	global $curr_mode,$curr_wpsstate,$curr_wpspin,$curr_appin,$pre_check_bf_mf_for_wps;

	if($curr_mode=="0")
	{
		$curr_wpsstate=trim(exec("qweconfig get wpsstate.wlan1"));
		$curr_appin=trim(exec("qweconfig get wpspin.wlan1"));

		$curr_broadcast=trim(exec("qweconfig get broadcastssid.wlan1"));
		$curr_mf=trim(exec("qweconfig  get aclmode"));
		if ($curr_broadcast== "1" && $curr_mf == "0")
		{
			$pre_check_bf_mf_for_wps=1;
		}
	}
}

function set_value()
{
	global $curr_mode,$curr_wpsstate,$curr_wpspin,$curr_appin;

	if ($_POST['action'] == 0)
	{
		if ($curr_mode == "0")
		{
			$change_flag=0;
			$new_wpsstate=$_POST['cmb_wpsstate'];
			$new_appin=$_POST['txt_appin'];
			if($new_wpsstate != $curr_wpsstate)
			{
				$change_flag=$change_flag+1;
				exec("qweconfig set wpsstate.wlan1 $new_wpsstate");
			}
			if($new_appin != $curr_appin)
			{
				$change_flag=$change_flag+1;
				exec("qweconfig set wpspin.wlan1 $curr_appin");
			}
			if ($change_flag>0)
			{
				exec("start-stop-daemon -S -b -x /bin/qweaction -- wlan1 commit");
			}
		}
	}
	else if ($_POST['action'] == 2)
	{
		exec("qweaction wlan1 wps pbc");
		echo "<script langauge=\"javascript\">self.open(\"config_wps_info24.php\",'WPS_STATUS','alwaysRaised,resizable,scrollbars,width=600,height=150').focus();</script>";
	}
	else if($_POST['action'] == 3)
	{
		$curr_wpspin = $_POST['txt_wpspin'];
		$tmp=exec("qweaction wlan1 wps $curr_wpspin");
		if (!(strpos($tmp, "Error") === FALSE))
		{
			echo "<script langauge=\"javascript\">self.open(\"config_wps_info24.php\",'WPS_STATUS','alwaysRaised,resizable,scrollbars,width=600,height=150').focus();</script>";
		}
		else
		{
			confirm("PIN validation failed.");
			return;
		}
	}
}

get_value();
if (isset($_POST['action']))
{
	set_value();
	get_value();
}
?>

<script type="text/javascript">
var mode = "<?php echo $curr_mode; ?>";

function populate_wpsstates_list()
{
	var curr_wpsstate = "<?php echo $curr_wpsstate; ?>";
	var cmb_ws = document.getElementById("cmb_wpsstate");

	cmb_ws.options.length = 3;

	cmb_ws.options[0].text = "Disabled"; cmb_ws.options[0].value ="0";
	cmb_ws.options[1].text = "Not Configured"; cmb_ws.options[1].value = "1";
	cmb_ws.options[2].text = "Configured"; cmb_ws.options[2].value = "2";

	if (curr_wpsstate== 0)
	{
		cmb_ws.options[0].selected = true;
	}
	else if (curr_wpsstate== 1)
	{
		cmb_ws.options[1].selected = true;
	}
	else if (curr_wpsstate== 2 )
	{
		cmb_ws.options[2].selected = true;
	}
}

function validate(action_name)
{
	var tmp = document.getElementById("action");
	var pre_check_bf_mf_for_wps = "<?php echo $pre_check_bf_mf_for_wps; ?>";
	tmp.value = action_name;

	if (action_name==0)//Save Button
	{
		var cmb_ws = document.getElementById("cmb_wpsstate");

		if (cmb_ws.selectedIndex != 0)
		{
			if (pre_check_bf_mf_for_wps != 1)
			{
				alert("Make sure Broadcast SSID has been enabled and Macfilter has been disabled.");
				return false;
			}
		}
		document.mainform.submit();
	}
	else if(action_name==1)//Cancel Button
	{
		window.location.href="config_wps24.php";
	}
	else if(action_name==2)//WPS Pbc Start Button
	{
		document.mainform.submit();
	}
	else if(action_name==3)//WPS Pin Start Button
	{
		var wpspin = document.getElementById("txt_wpspin");
		var reg = /\s/g;
		wpspin.value = wpspin.value.replace(reg, "");
		reg = /[-]/g;
		wpspin.value=wpspin.value.replace(reg,"");

		if (wpspin.value.length !=4 && wpspin.value.length !=8)
		{
			alert("WPS Pin needs to be 4 characters or 8 characters");
			return false;
		}
		if((wpspin.value != "") && (!(/^\d+$/.test(wpspin.value))))
		{
			alert("The WPS Pin should be natural numbers");
			return false;
		}
		document.mainform.submit();

	}
}

function modechange(obj)
{
	if(obj.name == "cmb_wpsstate")
	{
		var cmb_wpsstate=document.getElementById("cmb_wpsstate");

		if (cmb_wpsstate.selectedIndex== 0)
		{
			set_disabled('btn_wpspin', true);
			set_disabled('txt_wpspin', true);
			set_disabled('txt_appin', true);
		}
		else if (cmb_wpsstate.selectedIndex== 1)
		{
			//set_disabled('btn_wpspbc', true);
			set_disabled('btn_wpspin', true);
			set_disabled('txt_wpspin', true);
			set_disabled('txt_appin', true);
		}
		else if (cmb_wpsstate.selectedIndex== 2)
		{
			//set_disabled('btn_wpspbc', false);
			set_disabled('btn_wpspin', false);
			set_disabled('txt_wpspin', false);
			set_disabled('txt_appin', false);
		}
	}
}

function onload_event()
{
	var curr_wpsstate = "<?php echo $curr_wpsstate; ?>";
	init_menu();
	if (mode=="0")
	{
		populate_wpsstates_list();
		if (curr_wpsstate < 2)
		{
			set_disabled("btn_wpspbc",true);
			set_disabled("txt_wpspin",true);
			set_disabled("btn_wpspin",true);
		}
	}

	set_control_value('cmb_wpsstate','<?php echo $curr_wpsstate; ?>', 'combox');
	set_control_value('txt_wpspin','<?php echo $curr_wpspin; ?>', 'text');
	set_control_value('txt_appin','<?php echo $curr_appin; ?>', 'text');

	if (mode!="0")
	{
		set_visible('tr_ws', false);
		set_visible('tr_appin', false);
	}
}

</script>

<body class="body" onload="onload_event();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="config_wps24.php" id="mainform" name="mainform" method="post">
<input type="hidden" name="action" id="action" value="action" />
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php $tmp=exec("call_qcsapi get_mode wifi0"); echo $tmp;?>','<?php $tmp=exec("qweconfig get mode.wlan1"); echo $tmp;?>',privilege);
		</script>
	</div>
	<div class="right">
		<div class="righttop">2.4G WI-FI - WPS</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr id="tr_ws">
					<td width="35%">WPS State:</td>
					<td width="65%">
						<select name="cmb_wpsstate" id="cmb_wpsstate" class="combox" onchange="modechange(this)">
						</select>
					</td>
				</tr>
				<tr>
					<td>WPS PBC:</td>
					<td>
						<button name="btn_wpspbc" id="btn_wpspbc" type="button" onclick="validate(2);" class="button" style="width:120px;">WPS PBC</button>
					</td>
				</tr>
				<tr id="tr_wpspin">
					<td>WPS PIN:</td>
					<td>
						<input name="txt_wpspin" type="text" id="txt_wpspin" class="textbox" />
						<button name="btn_wpspin" id="btn_wpspin" type="button" onclick="validate(3);" class="button" style="width:120px;">WPS PIN</button>
					</td>
				</tr>
				<tr id="tr_appin">
					<td>WPS AP PIN:</td>
					<td>
						<input name="txt_appin" type="text" id="txt_appin" class="textbox" />
					</td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_save_adv" id="btn_save_basic" type="button" onclick="validate(0);"  class="button">Save</button>
				<button name="btn_cancel_adv" id="btn_cancel_basic" type="button" onclick="validate(1);"  class="button">Cancel</button>
			</div>
		</div>
	</div>
</div>
</form>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> | <a href="help/h_networking.php">Help</a><br />
	<div>&copy; 2013 Quantenna Communications, Inc. All Rights Reserved.</div>
</div>

</body>
</html>

