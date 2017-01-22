#!/usr/lib/cgi-bin/php-cgi

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
	<title>Quantenna Communications</title>
	<link rel="stylesheet" type="text/css" href="./themes/style.css" media="screen" />
	<link rel="stylesheet" type="text/css" href="./SpryAssets/SpryTabbedPanels.css" />
	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<meta http-equiv="expires" content="0" />
	<meta http-equiv="CACHE-CONTROL" content="no-cache" />
</head>

<script language="javascript" type="text/javascript" src="./SpryAssets/SpryTabbedPanels.js"></script>
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
$calstate = exec("get_bootval calstate");
if ($calstate != 3)
{
	echo "<head>";
	echo "Wireless settings are available only in calibration state 3. Redirecting to device status page";
	echo "<meta HTTP-EQUIV=\"REFRESH\" content=\"3; url=status_device.php\">";
	echo "</head>";
	echo "</html>";
	exit();
}

$pid = exec("pidof wpa_supplicant");
if ($pid == "") $pid = exec("pidof hostapd");
$val = exec("ifconfig -a | grep wifi0");
if ($val == "" || $pid == "")
{
	echo "<head>";
	echo "Wireless services are not started. Redirecting to network page";
	echo "<meta HTTP-EQUIV=\"REFRESH\" content=\"3; url=status_networking.php\">";
	echo "</head>";
	exit();
}

$curr_mode=exec("call_qcsapi get_mode wifi0");
$curr_hotspot="1";
$curr_oper_friendly="";
$oper_friendly_arr_len=0;
$hs20_conn_capab_list="";
$hs20_conn_arr_len = 0;
$curr_disable_dgaf="";
$curr_wan_metrics="";
$curr_wan_link_status="";
$curr_wan_symmetric_status="";
$curr_download_speed="";
$curr_uplink_speed="";
$get_proto_status="";
$curr_2_4_ghz_band="";
$curr_5_ghz_band="";

function get_value()
{
	global	$curr_hotspot,$curr_oper_friendly,$oper_friendly_arr_len;
	global $hs20_conn_capab_list,$hs20_conn_arr_len,$curr_disable_dgaf;
	global $curr_wan_metrics,$curr_wan_link_status,$curr_wan_symmetric_status,$curr_download_speed,$curr_uplink_speed;
	global $get_proto_status,$curr_2_4_ghz_band,$curr_5_ghz_band;

	//Get Current Encryption mode
        $get_proto_status = get_ap_proto();

	//Get Hotspot Enable
	$curr_hotspot=trim(shell_exec("call_qcsapi get_hs20_status wifi0"));
	if(is_qcsapi_error($curr_hotspot))
		$curr_hotspot="0";
	//Get Operator Name And Langauge Code
	$curr_oper_friendly=trim(shell_exec("call_qcsapi get_oper_friendly_name wifi0"));
        if(is_qcsapi_error($curr_oper_friendly))
        {
                $oper_friendly_arr_len = 0;
        }
        else
        {
                $curr_oper_friendly = explode("\n",$curr_oper_friendly);
                $oper_friendly_arr_len = count($curr_oper_friendly);
        }
	//Get IP Protocol, Port Number and Status
	$hs20_conn_capab_list=trim(shell_exec("call_qcsapi get_hs20_conn_capab wifi0"));
	if(is_qcsapi_error($hs20_conn_capab_list))
	{
		$hs20_conn_arr_len = 0;
	}
	else
	{
		$hs20_conn_capab_list=explode("\n",$hs20_conn_capab_list);
		$hs20_conn_arr_len = count($hs20_conn_capab_list);
	}
	//Get Disable dgaf
	$curr_disable_dgaf=trim(shell_exec("call_qcsapi get_hs20_params wifi0 disable_dgaf"));
	if(is_qcsapi_error($curr_disable_dgaf))
	{
		$curr_disable_dgaf="";
	}
	//Get WAN Metrics
	$curr_wan_metrics=trim(shell_exec("call_qcsapi get_hs20_params wifi0 hs20_wan_metrics"));
	if(is_qcsapi_error($curr_wan_metrics))
	{
		$curr_wan_link_status=0;
		$curr_wan_symmetric_status=0;
		$curr_download_speed="";
		$curr_uplink_speed="";
	}
	else
	{
		$curr_wan_metrics=explode(":",$curr_wan_metrics);
		$curr_wan_link_status = $curr_wan_metrics[0] & 03;
		$curr_wan_symmetric_status = $curr_wan_metrics[0] >> 2;
		$curr_download_speed = $curr_wan_metrics[1];
		$curr_uplink_speed = $curr_wan_metrics[2];
	}
	//Get Operating Class
	$curr_2_4_ghz_band=0;
	$curr_5_ghz_band=0;
	$curr_oper_band=trim(shell_exec("call_qcsapi get_hs20_params wifi0 hs20_operating_class"));
	if(is_qcsapi_error($curr_oper_band))
	{
		$curr_2_4_ghz_band=0;
		$curr_5_ghz_band=0;
	}
	else
	{
		if ($curr_oper_band == 51)
			$curr_2_4_ghz_band = 1;
		if ($curr_oper_band == 73)
			$curr_5_ghz_band = 1;
		if ($curr_oper_band != 51 && $curr_oper_band != 73)
		{
			$curr_2_4_ghz_band = 1;
			$curr_5_ghz_band = 1;
		}
	}
}

function set_value()
{
	global  $curr_hotspot,$curr_disable_dgaf,$curr_wan_link_status;
	global	$curr_2_4_ghz_band,$curr_5_ghz_band;

	//Save
	if ($_POST['action'] == 0)
	{
		$new_hotspot=$_POST['cmb_hotspot'];

		$count_oper_friendly = $_POST['numrows_oper_friendly'];
                $chkbox_oper_friendly_arr = $_POST['chk_box_oper_friendly'];
                $chkbox_oper_friendly_arr = explode(",", $chkbox_oper_friendly_arr);

                $add_oper_friendly = $_POST['add_oper_friendly'];
                $del_oper_friendly = $_POST['del_oper_friendly'];

		$conn_capab_count = $_POST['numrows_conn_capab'];
		$add_conn_capab = $_POST['add_conn_capab'];

		$conn_capab_chkbox_arr = $_POST['chk_box_conn_capab'];
		$conn_capab_chkbox_arr = explode(",", $conn_capab_chkbox_arr);
		$del_conn_capab = $_POST['del_conn_capab'];

		$new_disable_dgaf=$_POST['chk_disable_dgaf'];
		if ($new_disable_dgaf=="on")
		{$new_disable_dgaf=1;}
		else
		{$new_disable_dgaf=0;}

		$new_wan_link_status = $_POST['cmb_wan_link_status'];
		$new_wan_symmetric_status = $_POST['cmb_wan_symmetric_link'];
		$new_download_speed = $_POST['txt_download_speed'];
		$new_uplink_speed = $_POST['txt_uplink_speed'];

		$new_2_4_ghz_band=$_POST['chk_2_4_GHz'];
		if ($new_2_4_ghz_band =="on")
		{$new_2_4_ghz_band=1;}
		else
		{$new_2_4_ghz_band=0;}

		$new_5_ghz_band=$_POST['chk_5_GHz'];
		if ($new_5_ghz_band =="on")
		{$new_5_ghz_band=1;}
		else
		{$new_5_ghz_band=0;}

		//Set Hotspot
		if ($new_hotspot != $curr_hotspot)
		{
			exec("call_qcsapi set_hs20_status wifi0 $new_hotspot");
			//Remove disable_dgaf param from hostapd.conf when hs20 is disabled
			if ($new_hotspot == 0)
			{
				$disable_dgaf=trim(shell_exec("call_qcsapi get_hs20_params wifi0 disable_dgaf"));
				if ($disable_dgaf == 1 || $disable_dgaf == 0)
					exec("call_qcsapi remove_hs20_param wifi0 disable_dgaf");
			}
		}

		//Set Operator Name And Language Code
		if ($add_oper_friendly == 1) {
                        for($i = 0; $i < $count_oper_friendly; $i++) {
                                $add_friendly_name = $_POST['txtbox_oper_friendly'][$i];
                                list($oper_name,$lang_code)=explode(":", $add_friendly_name);
				$lang_code = '"'.$lang_code.'"';
                                exec("call_qcsapi add_oper_friendly_name wifi0 $oper_name $lang_code");
                        }
                }
		//Remove hs20_oper_friendly_name parameter from hostapd.conf file
		if ($del_oper_friendly == 1) {
                        for($i = 0; $i <= $count_oper_friendly; $i++) {
                                if ($chkbox_oper_friendly_arr[$i] == "true")
                                {
                                        $del_venue_name = $_POST['txtbox_oper_friendly'][$i];
                                        list($oper_name,$lang_code)=explode(":", $del_venue_name);
					$lang_code = '"'.$lang_code.'"';
                                        exec("call_qcsapi del_oper_friendly_name wifi0 $oper_name $lang_code");
                                }
                        }
                }
		//Set IP Protocol, Port No, Status
		if ($add_conn_capab == 1) {
                        for($i = 0; $i < $conn_capab_count; $i++) {
                                $add_conn_val = $_POST['txtbox_conn_capab'][$i];
                                list($ip_proto,$port_no,$status)=explode(":", $add_conn_val);
                                exec("call_qcsapi add_hs20_conn_capab wifi0 $ip_proto $port_no $status");
                        }
                }
		//Remove hs20_conn_capab parameter from hostapd.conf file
		if ($del_conn_capab == 1) {
                        for($i = 0; $i <= $conn_capab_count; $i++) {
                                if ($conn_capab_chkbox_arr[$i] == "true")
                                {
                                        $del_conn_val = $_POST['txtbox_conn_capab'][$i];
                                        list($ip_proto,$port_no,$status)=explode(":", $del_conn_val);
                                        exec("call_qcsapi del_hs20_conn_capab wifi0 $ip_proto $port_no $status");
                                }
                        }
                }
		//Set Disable dgaf
		if ($new_disable_dgaf != $curr_disable_dgaf)
		{
			exec("call_qcsapi set_hs20_params wifi0 disable_dgaf $new_disable_dgaf");
		}
		//Set WAN Metrics
		if ($new_wan_link_status != 0)
		{
			$wan_info = $new_wan_link_status | $new_wan_symmetric_status << 2;
			$wan_info = "0".$wan_info;
			exec("call_qcsapi set_hs20_params wifi0 hs20_wan_metrics $wan_info $new_download_speed $new_uplink_speed 0 0 0 ");
		}
		//Remove WAN Metrics
		if ($curr_wan_link_status != 0 && $new_wan_link_status == 0)
		{
			exec("call_qcsapi remove_hs20_param wifi0 hs20_wan_metrics");
		}
		//Set Operating Class
		if (($curr_2_4_ghz_band == 0 && $new_2_4_ghz_band == 1 && $new_5_ghz_band == 0) ||
			($curr_5_ghz_band == 1 && $new_5_ghz_band == 0 && $new_2_4_ghz_band == 1))
		{
			exec("call_qcsapi set_hs20_params wifi0 hs20_operating_class 81");
		}

		if (($curr_5_ghz_band == 0 && $new_5_ghz_band == 1 && $new_2_4_ghz_band == 0) ||
			($curr_2_4_ghz_band == 1 && $new_2_4_ghz_band == 0 && $new_5_ghz_band == 1))
		{
			exec("call_qcsapi set_hs20_params wifi0 hs20_operating_class 115");
		}

		if ((($curr_2_4_ghz_band == 0 && $new_2_4_ghz_band == 1) &&
			($curr_5_ghz_band == 0 && $new_5_ghz_band == 1))||
			(($curr_2_4_ghz_band == 1 && $new_2_4_ghz_band == 1) &&
			 ($curr_5_ghz_band == 0  && $new_5_ghz_band == 1))||
			(($curr_5_ghz_band == 1 && $new_5_ghz_band == 1) &&
			 ($curr_2_4_ghz_band == 0 &&$new_2_4_ghz_band == 1)))
		{
			exec("call_qcsapi set_hs20_params wifi0 hs20_operating_class 81 115");
		}
		//Remove Operating Class
		if ((($curr_2_4_ghz_band == 1 && $new_2_4_ghz_band == 0) &&
			($curr_5_ghz_band == 1 && $new_5_ghz_band == 0)) ||
			($curr_2_4_ghz_band == 1 && $new_2_4_ghz_band == 0 && $new_5_ghz_band == 0) ||
			($curr_5_ghz_band == 1 && $new_5_ghz_band == 0 && $new_2_4_ghz_band == 0))
		{
			exec("call_qcsapi remove_hs20_param wifi0 hs20_operating_class");
		}

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
var oper_friendly_arr = <?php echo '["' . implode('", "', $curr_oper_friendly) . '"]' ?>;
var hs20_conn_capab_arr = <?php echo '["' . implode('", "', $hs20_conn_capab_list) . '"]' ?>;

function modechange(obj)
{
	if (obj.name == "cmb_hotspot")
	{
		if (isset('cmb_hotspot', '0'))
		{
			set_disabled('txt_op_name', true);
			set_disabled('txt_langauge', true);
			set_disabled('cmb_ip_protocol', true);
			set_disabled('txt_port', true);
			set_disabled('cmb_status', true);
			set_disabled('btn_oper_friendly_add', true);
			set_disabled('btn_oper_friendly_del', true);
			set_disabled('btn_conn_capab_add', true);
			set_disabled('btn_conn_capab_del', true);
			set_disabled('chk_disable_dgaf', true);
			set_disabled('cmb_wan_link_status', true);
			set_disabled('cmb_wan_symmetric_link', true);
			set_disabled('txt_download_speed', true);
			set_disabled('txt_uplink_speed', true);
		}
		else
		{
			set_disabled('txt_op_name', false);
			set_disabled('txt_langauge', false);
			set_disabled('cmb_ip_protocol', false);
			set_disabled('txt_port', false);
			set_disabled('cmb_status', false);
			set_disabled('btn_oper_friendly_add', false);
			set_disabled('btn_oper_friendly_del', false);
			set_disabled('btn_conn_capab_add', false);
			set_disabled('btn_conn_capab_del', false);
			set_disabled('chk_disable_dgaf', false);
			set_disabled('cmb_wan_link_status', false);
			set_disabled('cmb_wan_symmetric_link', false);
			set_disabled('txt_download_speed', false);
			set_disabled('txt_uplink_speed', false);
		}
	}
	if (obj.name == "cmb_wan_link_status")
	{
		if (isset('cmb_wan_link_status', '0'))
		{
			set_disabled('cmb_wan_symmetric_link', true);
			set_disabled('txt_download_speed', true);
			set_disabled('txt_uplink_speed', true);
		}
	}
}

function delete_hs20_conn_capab_row(tableid)
{
	var chk_box_arr = new Array();
        var table = document.getElementById(tableid);
        var rowCount = table.rows.length;
	var checkCount = 0;
	document.getElementById("del_conn_capab").value = 1;

        for(var i=1; i<rowCount; i++) {
		var row = table.rows[i];
                var chkbox = row.cells[0].childNodes[0];
		chk_box_arr[i-1]=chkbox.checked;
	}

	document.getElementById("chk_box_conn_capab").value = chk_box_arr;
	document.getElementById("numrows_conn_capab").value = rowCount - 1;

	for(var i=1; i<rowCount; i++) {
		var row = table.rows[i];
		var chkbox = row.cells[0].childNodes[0];
		if(null != chkbox && true == chkbox.checked) {
			checkCount++;
			table.rows[i].style.display = 'none';
		}
	}
	if (checkCount == 0)
	{
		alert("select hs20 connection capability to delete");
	}
	else
	{
		set_visible('tr_warning', true);
	}
}

function validate_port_number(port_num)
{
	if (port_num.value.length == 0)
	{
		alert("port number can not be empty (Require the port number to be between 0 and 65535)");
		return false;
	}

	if (((port_num.value.length > 0) && isNaN(port_num.value)) ||
		((port_num.value < 0) || (port_num.value > 65535)))
	{
		alert("Invalid port number (Require the port number to be between 0 and 65535)");
		return false;
	}

	return true;
}

function add_hs20_conn_capab_row(tableID)
{
	set_visible('hs20_conn_capab_table', true);

	var ip_proto = document.getElementById("cmb_ip_protocol");
	var port_no = document.getElementById("txt_port");
	var stat = document.getElementById("cmb_status");

	if (!validate_port_number(port_no))
	{
		return false;
	}

	var hs20_conn_capab = ip_proto.value + ":" + port_no.value + ":" + stat.value;

	document.getElementById("add_conn_capab").value = 1;

	var table = document.getElementById(tableID);

        var rowCount = table.rows.length;
        var row = table.insertRow(rowCount);

        var cell1 = row.insertCell(0);
        var element1 = document.createElement("input");
        element1.type = "checkbox";
        element1.name="chkbox[]";
        cell1.appendChild(element1);

	var cell2 = row.insertCell(1);
	var element2 = document.createElement("input");
	element2.type="text";
	element2.name="txtbox_conn_capab[]";
	element2.value=hs20_conn_capab;
	element2.readOnly = true;
	cell2.appendChild(element2);

	document.getElementById("numrows_conn_capab").value = rowCount;
	document.getElementById("txt_port").value="";

	set_visible('tr_warning', true);
}

function delete_oper_friendly_namerow(tableID)
{
	var chk_box_arr = new Array();
        var table = document.getElementById(tableID);
        var rowCount = table.rows.length;
	var checkCount = 0;
	document.getElementById("del_oper_friendly").value = 1;

        for(var i=1; i<rowCount; i++) {
		var row = table.rows[i];
                var chkbox = row.cells[0].childNodes[0];
		chk_box_arr[i-1]=chkbox.checked
	}

	document.getElementById("chk_box_oper_friendly").value = chk_box_arr;
	document.getElementById("numrows_oper_friendly").value = rowCount - 1;

	for(var i=1; i<rowCount; i++) {
		var row = table.rows[i];
		var chkbox = row.cells[0].childNodes[0];
		if(null != chkbox && true == chkbox.checked) {
			checkCount++;
			table.rows[i].style.display = 'none';
		}
	}

	if (checkCount == 0)
	{
		alert("select operator friendly name to delete");
	}
	else
	{
		set_visible('tr_warning', true);
	}

}

function validate_lang_code(lang_code)
{
	var AlphaPattern = /^[a-zA-Z]+$/;
        if (lang_code.value.length == 0)
        {
                alert("LANGUAGE CODE can not be empty (Require alphabetical characters between 2 or 3)");
                return false;
        }

        if (lang_code.value.length > 0 && AlphaPattern.test(lang_code.value) == false)
        {
                alert("Invalid LANGUAGE CODE (valid values are alphabetical characters between 2 or 3)");
                return false;
        }

        if (lang_code.value.length < 2 || lang_code.value.length > 3)
        {
                alert("Invalid LANGUAGE CODE (valid values are alphabetical characters between 2 or 3)");
                return false;
        }

        return true;
}

function validate_oper_name(oper_name)
{
	oper_name.value = oper_name.value.replace(/([\x00-\x1f]|[\x7f-\xff])/g, '');
        if (oper_name.value.length == 0)
        {
                alert("Operator Name can not be empty (Require characters length between 1 to 252)");
                return false;
        }

        if (oper_name.value.length > 252)
        {
                alert("Invalid Operator Name (characters length should be between 1 to 252)");
                return false;
        }
        return true;
}

function add_oper_friendly_namerow(tableid)
{
	set_visible('oper_friendly_table', true);
	var lang_code = document.getElementById("txt_langauge");
	var oper_name = document.getElementById("txt_op_name");
	if (!validate_lang_code(lang_code) || !validate_oper_name(oper_name))
	{
		return false;
	}
	var oper_friendly_val = lang_code.value + ":" + oper_name.value;

	document.getElementById("add_oper_friendly").value = 1;

	var table = document.getElementById(tableid);

        var rowCount = table.rows.length;
        var row = table.insertRow(rowCount);

        var cell1 = row.insertCell(0);
        var element1 = document.createElement("input");
        element1.type = "checkbox";
        element1.name="chkbox[]";
        cell1.appendChild(element1);

	var cell2 = row.insertCell(1);
	var element2 = document.createElement("input");
	element2.type="text";
	element2.name="txtbox_oper_friendly[]";
	element2.value=oper_friendly_val;
	element2.readOnly = true;
	cell2.appendChild(element2);

	document.getElementById("numrows_oper_friendly").value = rowCount;
	document.getElementById("txt_op_name").value="";
	document.getElementById("txt_langauge").value="";

	set_visible('tr_warning', true);
}

function reload()
{
        window.location.href="config_hotspot.php";
}

function is_enterprise_enabled(proto_status)
{
        if (proto_status == "NONE" || proto_status == "11i" ||
                proto_status == "11i_pmf" ||  proto_status == "WPAand11i")
        {
                return false;
        }

        return true;
}

function validate()
{
        var proto_status = "<?php echo $get_proto_status; ?>";
        var hotspot_status = document.getElementById("cmb_hotspot");
        if (hotspot_status.value == 1)
        {
		if (!is_enterprise_enabled(proto_status))
                {
                        alert("To enable Hotspot functionality\n the encryption type for the wireless network must beset to one of\n the enterprise security settings (WPA2-AES Enterprise or WPA2 + WPA Enterprise)");
                        return false;
                }
        }

	//Validate WAN Metrics
	var wan_link_status = document.getElementById("cmb_wan_link_status");
	var wan_symmetric_link = document.getElementById("cmb_wan_symmetric_link");
	var download_speed = document.getElementById("txt_download_speed");
	var uplink_speed = document.getElementById("txt_uplink_speed");
	if (wan_link_status.value != 0)
	{
		if (wan_symmetric_link.value == 1)
		{
			if (download_speed.value != uplink_speed.value)
			{
				alert("Downlink and Uplink Speed must be same");
				return false;
			}
		}
		else
		{
			if (download_speed.value == uplink_speed.value)
			{
				alert("Downlink and Uplink Speed must be different");
				return false;
			}
		}
	}
	document.mainform.submit();
}

function populate_hs20_conn_capab(curr_conn_capab)
{
	var table = document.getElementById("hs20_conn_capab_table");

	var rowCount = table.rows.length;
        var row = table.insertRow(rowCount);

        var cell1 = row.insertCell(0);
        var element1 = document.createElement("input");
        element1.type = "checkbox";
        element1.name = "chkbox[]";
        cell1.appendChild(element1);

        var cell2 = row.insertCell(1);
        var element2 = document.createElement("input");
        element2.type="text";
        element2.name="txtbox_conn_capab[]";
        element2.value=curr_conn_capab;
        element2.readOnly = true;
        cell2.appendChild(element2);
}

function populate_oper_friendly(curr_oper_friendly)
{
        var table = document.getElementById("oper_friendly_table");

        var rowCount = table.rows.length;
        var row = table.insertRow(rowCount);

        var cell1 = row.insertCell(0);
        var element1 = document.createElement("input");
        element1.type = "checkbox";
        element1.name = "chkbox[]";
        cell1.appendChild(element1);

        var cell2 = row.insertCell(1);
        var element2 = document.createElement("input");
        element2.type="text";
        element2.name="txtbox_oper_friendly[]";
        element2.value=curr_oper_friendly;
        element2.readOnly = true;
        cell2.appendChild(element2);
}

function onload_event()
{
	var curr_hotspot = "<?php echo $curr_hotspot; ?>";
	var oper_friendly_arr_len = "<?php echo $oper_friendly_arr_len; ?>";
	var hs20_conn_capab_arr_len = "<?php echo $hs20_conn_arr_len; ?>";

	init_menu();

	if (curr_hotspot == "0")
	{
		set_disabled('txt_op_name', true);
                set_disabled('txt_langauge', true);
                set_disabled('cmb_ip_protocol', true);
                set_disabled('txt_port', true);
                set_disabled('cmb_status', true);
		set_disabled('btn_oper_friendly_add', true);
		set_disabled('btn_oper_friendly_del', true);
		set_disabled('btn_conn_capab_add', true);
		set_disabled('btn_conn_capab_del', true);
		set_disabled('chk_disable_dgaf', true);
		set_disabled('cmb_wan_link_status', true);
		set_disabled('cmb_wan_symmetric_link', true);
		set_disabled('txt_download_speed', true);
		set_disabled('txt_uplink_speed', true);
	}

	//populate operator friendly name
        if (oper_friendly_arr_len == "0")
        {
                set_visible('oper_friendly_table', false);
        }
        else
        {
                for( var i=0; i < oper_friendly_arr_len; i++) {
                        populate_oper_friendly(oper_friendly_arr[i]);
                }
        }

	//populate hs20_conn_capab
        if (hs20_conn_capab_arr_len == 0)
        {
                set_visible('hs20_conn_capab_table', false);
        }
        else
        {
                for( var i=0; i< hs20_conn_capab_arr_len; i++) {
                        populate_hs20_conn_capab(hs20_conn_capab_arr[i]);
                }
        }

	set_control_value('cmb_hotspot','<?php echo $curr_hotspot; ?>', 'combox');
	set_control_value('cmb_wan_link_status','<?php echo $curr_wan_link_status; ?>', 'combox');
	set_control_value('cmb_wan_symmetric_link','<?php echo $curr_wan_symmetric_status; ?>', 'combox');

	set_visible('tr_warning', false);
}
</script>

<body class="body" onload="onload_event();" >
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php echo $curr_mode;?>',privilege);
		</script>
	</div>
	<form enctype="multipart/form-data" action="config_hotspot.php" id="mainform" name="mainform" method="post">
	<input type="hidden" name="action" id="action" value="action" />
	<div class="right">
		<div class="righttop">CONFIG - HOTSPOT</div>
		<div id="TabbedPanels1" class="TabbedPanels">
			<ul class="TabbedPanelsTabGroup">
				<li class="TabbedPanelsTab" tabindex="0">Basic</li>
				<li class="TabbedPanelsTab" tabindex="0" id="tbc_advanced">Advanced</li>
			</ul>
			<div class="TabbedPanelsContentGroup">
				<div class="TabbedPanelsContent">
					<div class="rightmain">
						<table class="tablemain">
							<tr id="tr_hotspot">
								<td width="40%">HotSpot Enable:</td>
								<td width="60%">
									<select name="cmb_hotspot" class="combox" id="cmb_hotspot" onchange="modechange(this)">
										<option value="1">Enable</option>
										<option value="0">Disable</option>
									</select>
								</td>
							</tr>
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
						</table>
							<tr id="tr_op_name">
								<td>Langauge Code:</td>
								<td><input name="txt_langauge" type="text" id="txt_langauge" class="textbox" style="width:60px;" /></td>
								<td>Operator Name:</td>
									<td><input name="txt_op_name" type="text" id="txt_op_name" class="textbox" style="width:100px;" /></td>
								<td><button name="btn_oper_friendly_add" id="btn_oper_friendly_add" type="button" onclick="add_oper_friendly_namerow('oper_friendly_table');" class="button" style="width:60px;">ADD</button></td>
								<td><button name="btn_oper_friendly_del" id="btn_oper_friendly_del" type="button" onclick="delete_oper_friendly_namerow('oper_friendly_table');" class="button" style="width:80px;">REMOVE</button></td>
								<table id="oper_friendly_table" width="350px" border="1">
									<td width="20%">option</td>
									<td width="20%">value</td>
								</table>

								<input name="numrows_oper_friendly" id="numrows_oper_friendly" type="hidden"/>
								<input name="chk_box_oper_friendly" id="chk_box_oper_friendly" type="hidden"/>
								<input name="add_oper_friendly" id="add_oper_friendly" type="hidden"/>
								<input name="del_oper_friendly" id="del_oper_friendly" type="hidden"/>
							</tr>
						<table class="tablemain">
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
						</table>
							<tr id="tr_ip_protocol">
								<td>IP Proto:</td>
								<td>
									<select name="cmb_ip_protocol" id="cmb_ip_protocol" class="combox" style="width:80px;">
										<option value="1">ICMP</option>
										<option value="6">TCP</option>
										<option value="17">UDP</option>
									</select>
								</td>
								<td>Port:</td>
								<td>
									<input name="txt_port" type="text" id="txt_port" class="textbox" style="width:60px;"/>
								</td>
								<td>Status:</td>
								<td>
									<select name="cmb_status" id="cmb_status" class="combox" style="width:80px;" >
										<option value="0">Closed</option>
										<option value="1">Open</option>
										<option value="2">Unknown</option>
									</select>
								</td>
								<td><button name="btn_conn_capab_add" id="btn_conn_capab_add" type="button" onclick="add_hs20_conn_capab_row('hs20_conn_capab_table');" class="button" style="width:60px;">ADD</button></td>
								<td><button name="btn_conn_capab_del" id="btn_conn_capab_del" type="button" onclick="delete_hs20_conn_capab_row('hs20_conn_capab_table');" class="button" style="width:80px;">REMOVE</button></td>
								<table id="hs20_conn_capab_table" width="350px" border="1">
									<td width="20%">option</td>
									<td width="20%">value</td>
								</table>
								<input name="numrows_conn_capab" id="numrows_conn_capab" type="hidden"/>
								<input name="add_conn_capab" id="add_conn_capab" type="hidden"/>

								<input name="chk_box_conn_capab" id="chk_box_conn_capab" type="hidden"/>
								<input name="del_conn_capab" id="del_conn_capab" type="hidden"/>
							</tr>

						<table class="tablemain">
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
							<tr id="tr_warning">
								<td colspan="2"; style="color:red; font-size:smaller;">*To apply the changes, click save button*</td>
							</tr>
						</table>
						<div class="rightbottom">
							<button name="btn_save_basic" id="btn_save_basic" type="button" onclick="validate();"  class="button">Save</button>
							<button name="btn_cancel_basic" id="btn_cancel_basic" type="button" onclick="reload();" class="button">Cancel</button>
						</div>
					</div>
				</div>
				<div class="TabbedPanelsContent">
					<div class="rightmain">
						<table class="tablemain">
							<tr id="tr_disable_dgaf">
								<td colspan="4";> Disable dgaf:&nbsp;&nbsp;
									<input name="chk_disable_dgaf" id="chk_disable_dgaf" type="checkbox"  class="checkbox" <?php if($curr_disable_dgaf==1) echo "checked=\"checked\""?>/>
								</td>
							</tr>
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
							<tr id="tr_wan_link_metrics">
								<td>WAN Link Status:</td>
								<td>
									<select name="cmb_wan_link_status" id="cmb_wan_link_status" class="combox" style="width:180px;" onchange="modechange(this)">
										<option value="0">Not Configures</option>
										<option value="1">Link Up</option>
										<option value="2">Link Down</option>
										<option value="3">Link in test state</option>
									</select>
								</td>
							</tr>
							<tr id="tr_wan_symmetric_link">
								<td>WAN Symmetric Link Status:</td>
								<td>
									<select name="cmb_wan_symmetric_link" id="cmb_wan_symmetric_link" class="combox" style="width:180px;">
										<option value="0">Different</option>
										<option value="1">Same</option>
									</select>
								</td>
							</tr>
							<tr id="tr_wan_download_speed">
								<td>WAN Download Speed:</td>
								<td>
									<input name="txt_download_speed" type="text" id="txt_download_speed" class="textbox" value="<?php  echo htmlspecialchars($curr_download_speed,ENT_QUOTES); ?>" style="width:180px;"/>
								</td>
							</tr>
							<tr id="tr_wan_uplink_speed">
								<td>WAN Uplink Speed:</td>
								<td>
									<input name="txt_uplink_speed" type="text" id="txt_uplink_speed" class="textbox" style="width:180px;" value="<?php  echo htmlspecialchars($curr_uplink_speed,ENT_QUOTES); ?>"   />
								</td>
							</tr>
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
<!--GESL-->
							<tr id="tr_operating_class">
								<td width="40%">hs20 operating class:</td>
							</tr>
							<tr>
								<td colspan="4";> 2.4 GHz:&nbsp;&nbsp;
									<input name="chk_2_4_GHz" id="chk_2_4_GHz" type="checkbox"  class="checkbox" <?php if($curr_2_4_ghz_band==1) echo "checked=\"checked\""?>/>
								</td>
							</tr>
							<tr>
								<td colspan="4";> 5 GHz:&nbsp;&nbsp;
									<input name="chk_5_GHz" id="chk_5_GHz" type="checkbox"  class="checkbox" <?php if($curr_5_ghz_band==1) echo "checked=\"checked\""?>/>
							</tr>
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
						</table>
						<div class="rightbottom">
							<button name="btn_save_basic" id="btn_save_basic" type="button" onclick="validate();"  class="button">Save</button>
							<button name="btn_cancel_basic" id="btn_cancel_basic" type="button" onclick="reload();" class="button">Cancel</button>
						</div>
					</div>
				</div>
		</div>
	</div>
	<input type="hidden" name="csrf_token" value="<?php echo get_session_token(); ?>" />
	</form>
<script type="text/javascript">
var TabbedPanels1 = new Spry.Widget.TabbedPanels("TabbedPanels1");
function changetab()
{
        TabbedPanels1.showPanel(1);
}
</script>
</div>

<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> | <a href="help/h_wireless_conf.php">Help</a><br />
	<div><?php echo $str_copy ?></div>
</div>
</body>
</html>

