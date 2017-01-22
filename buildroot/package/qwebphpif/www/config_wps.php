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
<script language="javascript" type="text/javascript" src="./js/menu.js"></script>
<script language="javascript" type="text/javascript" src="./js/webif.js"></script>
<?php
include("common.php");
$privilege = get_privilege(1);
?>


<script type="text/javascript">
var privilege="<?php echo $privilege; ?>";
</script>

<?php
$curr_mode=exec("call_qcsapi get_mode wifi0");
$sum_active_interface= "";
$ssid0="";
$ssid1="";
$ssid2="";
$ssid3="";
$ssid4="";
$ssid5="";
$ssid6="";
$ssid7="";
$curr_wpsstate=2;
$curr_wpspin="";
$curr_appin="";
$pre_check_bf_mf_for_wps=0;
$get_hs20_status="";

if (isset($_GET['id']))
{
	$page_id=substr($_GET['id'],0,1);
	$interface_id="wifi".$page_id;
}
else
{
	if($_POST['cmb_interface']!="")
	{
		$page_id= $_POST['cmb_interface'];
		$interface_id="wifi".escapeshellarg($page_id);
	}
	else
	{
		$page_id="0";
		$interface_id="wifi0";
	}
}

function get_value()
{
	global $curr_mode,$ssid0,$mac0,$arr_ssid,$arr_index,$sum_active_interface,$ssid1,$mac1,$ssid2,$mac2,$ssid3,$mac3,$ssid4,$mac4,$ssid5,$mac5,$ssid6,$mac6,$ssid7,$mac7,$curr_wpsstate0,$curr_wpsstate,$curr_wpspin,$curr_appin,$pre_check_bf_mf_for_wps,$interface_id;
	global $get_hs20_status;

	$arr_ssid="";
	$arr_index="";
	$sum_active_interface= 1;

	//=============Get SSID=======================
	$ssid0=exec("call_qcsapi get_SSID wifi0");
	$mac0=exec("call_qcsapi get_macaddr wifi0");
	$arr_ssid[0]="wifi0"."("."$mac0".")";
	$arr_index=0;

	//Get the status of HS2.0, 1-enable 0-disable
	$get_hs20_status = trim(shell_exec("call_qcsapi get_hs20_status wifi0"));
	if (is_qcsapi_error($get_hs20_status))
		$get_hs20_status = 0;
	if($curr_mode=="Access point")
	{
		$ssid1=exec("call_qcsapi get_SSID wifi1");
		if(is_qcsapi_error($ssid1))
		{
			$ssid1="N/A";
		}
		else
		{
			$mac1=exec("call_qcsapi get_macaddr wifi1");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi1"."("."$mac1".")";
			$arr_index="$arr_index".","."1";
		}

		$ssid2=exec("call_qcsapi get_SSID wifi2");
		if(is_qcsapi_error($ssid2))
		{
			$ssid2="N/A";
		}
		else
		{
			$mac2=exec("call_qcsapi get_macaddr wifi2");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi2"."("."$mac2".")";
			$arr_index="$arr_index".","."2";
		}

		$ssid3=exec("call_qcsapi get_SSID wifi3");
		if(is_qcsapi_error($ssid3))
		{
			$ssid3="N/A";
		}
		else
		{
			$mac3=exec("call_qcsapi get_macaddr wifi3");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi3"."("."$mac3".")";
			$arr_index="$arr_index".","."3";
		}

		$ssid4=exec("call_qcsapi get_SSID wifi4");
		if(is_qcsapi_error($ssid4))
		{
			$ssid4="N/A";
		}
		else
		{
			$mac4=exec("call_qcsapi get_macaddr wifi4");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi4"."("."$mac4".")";
			$arr_index="$arr_index".","."4";
		}

		$ssid5=exec("call_qcsapi get_SSID wifi5");
		if(is_qcsapi_error($ssid5))
		{
			$ssid5="N/A";
		}
		else
		{
			$mac5=exec("call_qcsapi get_macaddr wifi5");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi5"."("."$mac5".")";
			$arr_index="$arr_index".","."5";
		}

		$ssid6=exec("call_qcsapi get_SSID wifi6");
		if(is_qcsapi_error($ssid6))
		{
			$ssid6="N/A";
		}
		else
		{
			$mac6=exec("call_qcsapi get_macaddr wifi6");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi6"."("."$mac6".")";
			$arr_index="$arr_index".","."6";
		}

		$ssid7=exec("call_qcsapi get_SSID wifi7");
		if(is_qcsapi_error($ssid7))
		{
			$ssid7="N/A";
		}
		else
		{
			$mac7=exec("call_qcsapi get_macaddr wifi7");

			$sum_active_interface++;
			$arr_ssid[$sum_active_interface-1]="wifi7"."("."$mac7".")";
			$arr_index="$arr_index".","."7";
		}

		$tmp=trim(exec("call_qcsapi get_wps_runtime_state $interface_id"));
		switch ($tmp){
		case "disabled":
			$curr_wpsstate=0;
			break;
		case "not configured":
			$curr_wpsstate=1;
			break;
		case "configured":
			$curr_wpsstate=2;
			break;
		}
		if($curr_wpsstate == "2")
		{
			$curr_appin=exec("call_qcsapi get_wps_ap_pin $interface_id");
		}

		$curr_broadcast=trim(exec("call_qcsapi get_option $interface_id SSID_broadcast"));
		$curr_mf=trim(exec("call_qcsapi get_macaddr_filter $interface_id"));
		if ($curr_broadcast== "TRUE" && $curr_mf == "0")
			$pre_check_bf_mf_for_wps=1;
	}
	else if ($curr_mode == "Station")
	{
		//Get WPS Pin
		$curr_wpspin = trim(shell_exec("call_qcsapi get_wps_sta_pin wifi0"));
	}
}

function set_value()
{
	global $curr_mode,$ssid0,$mac0,$arr_ssid,$arr_index,$sum_active_interface,$ssid1,$mac1,$ssid2,$mac2,$ssid3,$mac3,$ssid4,$mac4,$ssid5,$mac5,$ssid6,$mac6,$ssid7,$mac7,$curr_wpsstate,$curr_wpspin,$curr_appin,$interface_id,$page_id;
	$new_wpsstate=$_POST['cmb_wpsstate'];
	session_start();
	$_SESSION['wps_state']=$new_wpsstate;
	$new_wpsstate_esc=escapeshellarg($new_wpsstate);

	if ($_POST['action'] == 0)
	{
		exec("call_qcsapi set_wps_configured_state $interface_id $new_wpsstate_esc");
		if($new_wpsstate == "2")
		{
			$curr_appin=exec("call_qcsapi get_wps_ap_pin wifi0");
		}
	}
	else if ($_POST['action'] == 2)
	{
		if($curr_mode == "Access point")
		{
			exec("call_qcsapi registrar_report_button_press $interface_id");
		}
		else if ($curr_mode == "Station")
		{
			exec("call_qcsapi enrollee_report_button_press wifi0");
		}
	}
	else if($_POST['action'] == 3)
	{
		$curr_wpspin = $_POST['txt_wpspin'];
		if($curr_mode == "Access point")
		{
			$tmp=exec("hostapd_cli wps_check_pin $curr_wpspin");
			if($tmp==$curr_wpspin)
			{
				exec("call_qcsapi registrar_report_pin $interface_id $curr_wpspin");
				echo "<script langauge=\"javascript\">self.open(\"config_wps_info.php?id=$page_id\",'WPS_STATUS','alwaysRaised,resizable,scrollbars,width=600,height=150').focus();</script>";
				//$wps_session=1;
			}
			else
			{
				confirm("PIN validation failed.");
				return;
			}
		}
		else if ($curr_mode == "Station")
		{
			$tmp=exec("wpa_cli wps_check_pin $curr_wpspin");
			if($tmp==$curr_wpspin)
			{
				exec("call_qcsapi enrollee_report_pin wifi0 $curr_wpspin");
				echo "<script langauge=\"javascript\">self.open(\"config_wps_info.php?id=$page_id\",'WPS_STATUS','alwaysRaised,resizable,scrollbars,width=600,height=150').focus();</script>";
				//$wps_session=1;
			}
			else
			{
				confirm("PIN validation failed.");
				return;
			}
		}
	}
	//WPS AP Pin Generate
	else if($_POST['action'] == 4)
	{
		$curr_appin=exec("call_qcsapi get_wps_ap_pin $interface_id 1");
		//$wps_session=2;
	}
}

get_value();
if (isset($_POST['action']))
{
	if (!(isset($_POST['csrf_token']) && $_POST['csrf_token'] === get_session_token())) {
		header('Location: login.php');
		exit();
	}
	set_value();
	get_value();
}
?>

<script type="text/javascript">
var mode = "<?php echo $curr_mode; ?>";
var page_id = "<?php echo $page_id; ?>";


function populate_interface_list()
{
	var cmb_ws = document.getElementById("cmb_wpsstate");
	var cmb_if = document.getElementById("cmb_interface");


	cmb_if.options.length = "<?php echo $sum_active_interface; ?>";
	var tmp_index= "<?php echo $arr_index; ?>";
	var interface_index=tmp_index.split(",");
	var tmp_text=new Array();
	tmp_text[0]="<?php echo $arr_ssid[0]; ?>";
	tmp_text[1]="<?php echo $arr_ssid[1]; ?>";
	tmp_text[2]="<?php echo $arr_ssid[2]; ?>";
	tmp_text[3]="<?php echo $arr_ssid[3]; ?>";
	tmp_text[4]="<?php echo $arr_ssid[4]; ?>";
	tmp_text[5]="<?php echo $arr_ssid[5]; ?>";
	tmp_text[6]="<?php echo $arr_ssid[6]; ?>";
	tmp_text[7]="<?php echo $arr_ssid[7]; ?>";

	for (var i=0; i < cmb_if.options.length; i++)
	{
		cmb_if.options[i].text = tmp_text[i]; cmb_if.options[i].value = interface_index[i];
		if (page_id == cmb_if.options[i].value)
		{
			cmb_if.options[i].selected=true;
		}
	}

}

function populate_wpsstates_list()
{
	var curr_wpsstate = "<?php echo $curr_wpsstate; ?>";
	var cmb_ws = document.getElementById("cmb_wpsstate");
	var cmb_if = document.getElementById("cmb_interface");

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
	tmp.value = action_name;

	if (action_name==0)//Save Button
	{
		var cmb_ws = document.getElementById("cmb_wpsstate");
		var cmb_if = document.getElementById("cmb_interface");


		var pre_check_bf_mf_for_wps = "<?php echo $pre_check_bf_mf_for_wps; ?>";


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
		window.location.href="config_wps.php";
	}
	else if(action_name==2)//WPS Pbc Start Button
	{
		document.mainform.submit();
		var status_page="config_wps_info.php?id="+page_id;
		self.open(status_page,'WPS_STATUS','alwaysRaised,resizable,scrollbars,width=600,height=150').focus();
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
	else if(action_name==4)//Generate Device ID Button
	{
		document.mainform.submit();
	}
}


function reload()
{
	var tmp_index= "<?php echo $arr_index; ?>";
	var interface_index=tmp_index.split(",");

	var cmb_if = document.getElementById("cmb_interface");
	if (mode == "Station")
	{
		window.location.href="config_wps.php";
	}
	else
	{
		window.location.href="config_wps.php?id="+interface_index[cmb_if.selectedIndex];
	}
}


function modechange(obj)
{
	if(obj.name == "cmb_interface")
	{

		reload();

	}
	if(obj.name == "cmb_wpsstate")
	{
		var cmb_wpsstate=document.getElementById("cmb_wpsstate");

		if (cmb_wpsstate.selectedIndex== 0)
		{
			//set_disabled('btn_wpspbc', true);
			set_disabled('btn_wpspin', true);
			set_disabled('txt_wpspin', true);
			set_disabled('btn_regenerate', true);
		}
		else if (cmb_wpsstate.selectedIndex== 1)
		{
			//set_disabled('btn_wpspbc', true);
			set_disabled('btn_wpspin', true);
			set_disabled('txt_wpspin', true);
			set_disabled('btn_regenerate', false);
		}
		else if (cmb_wpsstate.selectedIndex== 2)
		{
			//set_disabled('btn_wpspbc', false);
			set_disabled('btn_wpspin', false);
			set_disabled('txt_wpspin', false);
			set_disabled('btn_regenerate', false);
		}
	}
}

function wps_enable_disable(hs20_status)
{
	// HS2.0 enabled then disable the WPS configuration in UI.
	if (hs20_status == "1")
	{
		alert("WPS can not be configured when HS2.0 enabled");
		set_disabled('cmb_interface', true);
		set_disabled('cmb_wpsstate', true);
		set_disabled('btn_wpspbc', true);
		set_disabled('txt_wpspin', true);
		set_disabled('btn_wpspin', true);
		set_disabled('txt_appin', true);
		set_disabled('btn_regenerate', true);
		set_disabled('btn_save_basic', true);
		set_disabled('btn_cancel_basic', true);
	}
	else
	{
		set_disabled('cmb_interface', false);
		set_disabled('cmb_wpsstate', false);
		set_disabled('btn_wpspbc', false);
		set_disabled('txt_wpspin', false);
		set_disabled('btn_wpspin', false);
		set_disabled('txt_appin', false);
		set_disabled('btn_regenerate', false);
		set_disabled('btn_save_basic', false);
		set_disabled('btn_cancel_basic', false);
	}
}

function onload_event()
{

	var curr_wpsstate = "<?php echo $curr_wpsstate; ?>";
	var hs20_status = "<?php echo $get_hs20_status; ?>";
	if (page_id=="")
	{
		page_id="0";
	}

	init_menu();

	wps_enable_disable(hs20_status);

	if (mode=="Access point")
	{
		populate_interface_list();
	}
	populate_wpsstates_list();
/*
	if (wps_session == "0")
		set_visible("tr_startingwps", false);
	else if (wps_session == "1")
		changetab();
	else if (wps_session == "2")
	{
		set_visible("tr_startingwps", false);
		changetab();
	}
*/




	if (curr_wpsstate < 2)
	{
		set_disabled("btn_wpspbc",true);
		set_disabled("txt_wpspin",true);
		set_disabled("btn_wpspin",true);
		if (curr_wpsstate < 1)
		{
			set_disabled("btn_regenerate",true);
		}
	}

	set_control_value('cmb_wpsstate','<?php echo $curr_wpsstate; ?>', 'combox');
	set_control_value('txt_wpspin','<?php echo $curr_wpspin; ?>', 'text');
	set_control_value('txt_appin','<?php echo $curr_appin; ?>', 'text');

	if (mode!="Access point")
	{
		set_visible('tr_ws', false);
		set_visible('tr_appin', false);
		set_visible('tr_if', false);
		set_visible('tr_l', false);
	}

}

</script>

<body class="body" onload="onload_event();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="config_wps.php" id="mainform" name="mainform" method="post">
<input type="hidden" name="action" id="action" value="action" />
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php echo $curr_mode;?>',privilege);
		</script>
	</div>
	<div class="right">
		<div class="righttop">CONFIG - WPS</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr id="tr_if">
					<td width="35%">Wifi Interface:</td>
					<td width="65%">
						<select name="cmb_interface" id="cmb_interface" class="combox" onchange="modechange(this)">
						</select>
					</td>
				</tr>
				<tr id="tr_l">
					<td class="divline" colspan="2";></td>
				</tr>
				<tr id="tr_ws">
					<td>WPS State:</td>
					<td>
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
						<input name="txt_appin" type="text" id="txt_appin" class="textbox" readonly="readonly"/>
						<button name="btn_regenerate" id="btn_regenerate" type="button" onclick="validate(4);" class="button" style="width:120px;">Regenerate</button>
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
<input type="hidden" name="csrf_token" value="<?php echo get_session_token(); ?>" />
</form>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> | <a href="help/h_networking.php">Help</a><br />
	<div><?php echo $str_copy ?></div>
</div>

</body>
</html>

