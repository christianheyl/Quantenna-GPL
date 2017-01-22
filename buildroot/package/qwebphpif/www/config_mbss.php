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
$curr_mode=exec("call_qcsapi get_mode wifi0");
if($curr_mode=="Station")
{
	echo "<script langauge=\"javascript\">alert(\"Don`t support in the Station mode.\");</script>";
	echo "<script language='javascript'>location.href='status_device.php'</script>";
	return;
}

$file="/mnt/jffs2/per_ssid_config.txt";
function generate_vlan_config_file($file_path)
{
	if(!file_exists($file_path))
	{
		$config_file_content="wifi0:priority=0\nwifi1:\nwifi2:\nwifi3:\nwifi4:\nwifi5:\nwifi6:\nwifi7:\nwds0:\nwds1:\nwds2:\nwds3:\nwds4:\nwds5:\nwds6:\nwds7:\nwds8:\n";
		file_put_contents($file_path, $config_file_content);
		exec("chmod 755 $file");
	}
}
generate_vlan_config_file($file);
$content=file_get_contents($file);
$chk1=0;
$chk2=0;
$chk3=0;
$chk4=0;
$chk5=0;
$chk6=0;
$chk7=0;
$ssid1="";
$ssid2="";
$ssid3="";
$ssid4="";
$ssid5="";
$ssid6="";
$ssid7="";
$pmf1="0";
$pmf2="0";
$pmf3="0";
$pmf4="0";
$pmf5="0";
$pmf6="0";
$pmf7="0";
$proto1="NONE";
$proto2="NONE";
$proto3="NONE";
$proto4="NONE";
$proto5="NONE";
$proto6="NONE";
$proto7="NONE";
$psk1="";
$psk2="";
$psk3="";
$psk4="";
$psk5="";
$psk6="";
$psk7="";
$br1=0;
$br2=0;
$br3=0;
$br4=0;
$br5=0;
$br6=0;
$br7=0;
$vlan0="";
$vlan1="";
$vlan2="";
$vlan3="";
$vlan4="";
$vlan5="";
$vlan6="";
$vlan7="";
$priority1="0";
$priority2="0";
$priority3="0";
$priority4="0";
$priority5="0";
$priority6="0";
$priority7="0";
$curr_radius1="";
$radius_arr_len1="";
$curr_radius2="";
$radius_arr_len2="";
$curr_radius3="";
$radius_arr_len3="";
$curr_radius4="";
$radius_arr_len4="";
$curr_radius5="";
$radius_arr_len5="";
$curr_radius6="";
$radius_arr_len6="";
$curr_radius7="";
$radius_arr_len7="";

function get_proto($device)
{
	$beacon=exec("call_qcsapi get_beacon $device");
	$encryption=exec("call_qcsapi get_WPA_encryption_modes $device");
	$authentication=exec("call_qcsapi get_WPA_authentication_mode $device");
	if ($beacon=="Basic")
		return "NONE";
	else if ($beacon=="11i" && $encryption=="AESEncryption" && $authentication=="PSKAuthentication")
		return "11i";
	else if ($beacon=="11i" && $encryption=="AESEncryption" && $authentication=="SHA256PSKAuthentication")
		return "11i_pmf";
	else if ($beacon=="WPAand11i" && $encryption=="TKIPandAESEncryption")
		return "WPAand11i";
	else if ($authentication=="EAPAuthentication" && $encryption=="AESEncryption")
		return "WPA2-EAP";
	else if ($authentication=="EAPAuthentication" && $encryption=="TKIPandAESEncryption")
		return "WPAand11i-EAP";
}

function set_proto($device,$proto)
{
	if ($proto=="NONE")
	{
		exec("call_qcsapi set_beacon $device Basic");
		exec("call_qcsapi set_WPA_encryption_modes $device AESEncryption");
	}
	else if ($proto=="11i")
	{
		exec("call_qcsapi set_beacon $device 11i");
		exec("call_qcsapi set_WPA_encryption_modes $device AESEncryption");
		exec("call_qcsapi set_WPA_authentication_mode $device PSKAuthentication");
	}
	else if ($proto=="11i_pmf")
	{
		exec("call_qcsapi set_beacon $device 11i");
		exec("call_qcsapi set_WPA_encryption_modes $device AESEncryption");
		exec("call_qcsapi set_WPA_authentication_mode $device SHA256PSKAuthentication");
	}
	else if ($proto=="WPAand11i")
	{
		exec("call_qcsapi set_beacon $device WPAand11i");
		exec("call_qcsapi set_WPA_encryption_modes $device TKIPandAESEncryption");
		exec("call_qcsapi set_WPA_authentication_mode $device PSKAuthentication");
	}
	else if ($proto=="WPA2-EAP")
	{
		$ipaddr=read_ipaddr();
		exec("call_qcsapi set_beacon $device 11i");
		exec("call_qcsapi set_WPA_authentication_mode $device EAPAuthentication");
		exec("call_qcsapi set_WPA_encryption_modes $device AESEncryption");
		exec("call_qcsapi set_own_ip_addr wifi0 \"$ipaddr\"");
	}
	else if ($proto=="WPAand11i-EAP")
	{
		$ipaddr=read_ipaddr();
		exec("call_qcsapi set_beacon $device WPAand11i");
		exec("call_qcsapi set_WPA_authentication_mode $device EAPAuthentication");
		exec("call_qcsapi set_WPA_encryption_modes $device TKIPandAESEncryption");
		exec("call_qcsapi set_own_ip_addr wifi0 \"$ipaddr\"");
	}
}

function getPsk($device)
{
	$tmp_psk=exec("call_qcsapi get_passphrase $device 0");
	if($tmp_psk=="QCS API error 1001: Parameter not found")
	{$tmp_psk=exec("call_qcsapi get_pre_shared_key $device 0");}
	return $tmp_psk;
}

function getBroad($device)
{
	$tmp_br=exec("call_qcsapi get_option $device SSID_broadcast");
	if ($tmp_br=="TRUE")
	{return 1;}
	else
	{return 0;}
}

function getPmf($device)
{
	$tmp_pmf=exec("call_qcsapi get_pmf $device");
	return $tmp_pmf;
}

function getPriority($device)
{
	$tmp=exec("call_qcsapi get_priority $device");
	if(is_qcsapi_error($tmp))
	{return "0";}
	else
	{return $tmp;}
}
//=================Load Value======================
function getValue()
{
	global $file,$chk1,$chk2,$chk3,$chk4,$chk5,$chk6,$chk7;
	global $ssid1,$ssid2,$ssid3,$ssid4,$ssid5,$ssid6,$ssid7;
	global $vlan1,$vlan2,$vlan3,$vlan4,$vlan5,$vlan6,$vlan7;
	global $br1,$br2,$br3,$br4,$br5,$br6,$br7;
	global $priority1,$priority2,$priority3,$priority4,$priority5,$priority6,$priority7;

	global $pmf1,$pmf2,$pmf3,$pmf4,$pmf5,$pmf6,$pmf7;
	global $proto1,$proto2,$proto3,$proto4,$proto5,$proto6,$proto7;
	global $psk1,$psk2,$psk3,$psk4,$psk5,$psk6,$psk7;
	global $curr_radius1,$radius_arr_len1,$curr_radius2,$radius_arr_len2,$curr_radius3,$radius_arr_len3,$curr_radius4,$radius_arr_len4, $curr_radius5,$radius_arr_len5, $curr_radius6,$radius_arr_len6,$curr_radius7,$radius_arr_len7;

	$ssid1=exec("call_qcsapi get_ssid wifi1");
	if(is_qcsapi_error($ssid1))
	{$ssid1="";$chk1=0;$br1=0;$priority1=0;$pmf1=0; $proto1="NONE";$vlan1="";}
	else
	{
		$chk1=1;
		$vlan1=exec("cat $file | grep wifi1 | awk -F 'vlan=' '{print $2}' | awk -F '&' '{print $1}'");
		$br1=getBroad("wifi1");
		$priority1=getPriority("wifi1");

		$pmf1=getPmf("wifi1");
		$proto1=get_proto("wifi1");
		$psk1=getPsk("wifi1");
		if ($proto1 == "WPA2-EAP" || $proto1 == "WPAand11i-EAP")
		{
			$curr_radius1 = trim(shell_exec("call_qcsapi get_radius_auth_server_cfg wifi1"));
			if(is_qcsapi_error($curr_radius1))
			{
				$radius_arr_len1=0;
			}
			else
			{
				$curr_radius1=explode("\n",$curr_radius1);
				$radius_arr_len1 = count($curr_radius1);
			}
		}
	}

	$ssid2=exec("call_qcsapi get_ssid wifi2");
	if(is_qcsapi_error($ssid2))
	{$ssid2="";$chk2=0;$br2=0;$priority2=0;$pmf2=0; $proto2="NONE";$vlan2="";}
	else
	{
		$chk2=1;
		$vlan2=exec("cat $file | grep wifi2 | awk -F 'vlan=' '{print $2}' | awk -F '&' '{print $1}'");
		$br2=getBroad("wifi2");
		$priority2=getPriority("wifi2");

		$pmf2=getPmf("wifi2");
		$proto2=get_proto("wifi2");
		$psk2=getPsk("wifi2");
		if ($proto2 == "WPA2-EAP" || $proto2 == "WPAand11i-EAP")
		{
			$curr_radius2 = trim(shell_exec("call_qcsapi get_radius_auth_server_cfg wifi2"));
			if(is_qcsapi_error($curr_radius2))
			{
				$radius_arr_len2=0;
			}
			else
			{
				$curr_radius2=explode("\n",$curr_radius2);
				$radius_arr_len2 = count($curr_radius2);
			}
		}
	}

	$ssid3=exec("call_qcsapi get_ssid wifi3");
	if(is_qcsapi_error($ssid3))
	{$ssid3="";$chk3=0;$br3=0;$priority3=0;$pmf3=0; $proto3="NONE";$vlan3="";}
	else
	{
		$chk3=1;
		$vlan3=exec("cat $file | grep wifi3 | awk -F 'vlan=' '{print $2}' | awk -F '&' '{print $1}'");
		$br3=getBroad("wifi3");
		$priority3=getPriority("wifi3");

		$pmf3=getPmf("wifi3");
		$proto3=get_proto("wifi3");
		$psk3=getPsk("wifi3");
		if ($proto3 == "WPA2-EAP" || $proto3 == "WPAand11i-EAP")
		{
			$curr_radius3 = trim(shell_exec("call_qcsapi get_radius_auth_server_cfg wifi3"));
			if(is_qcsapi_error($curr_radius3))
			{
				$radius_arr_len3=0;
			}
			else
			{
				$curr_radius3=explode("\n",$curr_radius3);
				$radius_arr_len3 = count($curr_radius3);
			}
		}
	}

	$ssid4=exec("call_qcsapi get_ssid wifi4");
	if(is_qcsapi_error($ssid4))
	{$ssid4="";$chk4=0;$br4=0;$priority4=0;$pmf4=0; $proto4="NONE";$vlan4="";}
	else
	{
		$chk4=1;
		$vlan4=exec("cat $file | grep wifi4 | awk -F 'vlan=' '{print $2}' | awk -F '&' '{print $1}'");
		$br4=getBroad("wifi4");
		$priority4=getPriority("wifi4");

		$pmf4=getPmf("wifi4");
		$proto4=get_proto("wifi4");
		$psk4=getPsk("wifi4");
		if ($proto4 == "WPA2-EAP" || $proto4 == "WPAand11i-EAP")
		{
			$curr_radius4 = trim(shell_exec("call_qcsapi get_radius_auth_server_cfg wifi4"));
			if(is_qcsapi_error($curr_radius4))
			{
				$radius_arr_len4=0;
			}
			else
			{
				$curr_radius4=explode("\n",$curr_radius4);
				$radius_arr_len4 = count($curr_radius4);
			}
		}
	}

	$ssid5=exec("call_qcsapi get_ssid wifi5");
	if(is_qcsapi_error($ssid5))
	{$ssid5="";$chk5=0;$br5=0;$priority5=0;$pmf5=0; $proto5="NONE";$vlan5="";}
	else
	{
		$chk5=1;
		$vlan5=exec("cat $file | grep wifi2 | awk -F 'vlan=' '{print $2}' | awk -F '&' '{print $1}'");
		$br5=getBroad("wifi5");
		$priority5=getPriority("wifi5");

		$pmf5=getPmf("wifi5");
		$proto5=get_proto("wifi5");
		$psk5=getPsk("wifi5");
		if ($proto5 == "WPA2-EAP" || $proto5 == "WPAand11i-EAP")
		{
			$curr_radius5 = trim(shell_exec("call_qcsapi get_radius_auth_server_cfg wifi5"));
			if(is_qcsapi_error($curr_radius5))
			{
				$radius_arr_len5=0;
			}
			else
			{
				$curr_radius5=explode("\n",$curr_radius5);
				$radius_arr_len5 = count($curr_radius5);
			}
		}
	}

	$ssid6=exec("call_qcsapi get_ssid wifi6");
	if(is_qcsapi_error($ssid6))
	{$ssid6="";$chk6=0;$br6=0;$priority6=0;$pmf6=0; $proto6="NONE";$vlan6="";}
	else
	{
		$chk6=1;
		$vlan6=exec("cat $file | grep wifi2 | awk -F 'vlan=' '{print $2}' | awk -F '&' '{print $1}'");
		$br6=getBroad("wifi6");
		$priority6=getPriority("wifi6");

		$pmf6=getPmf("wifi6");
		$proto6=get_proto("wifi6");
		$psk6=getPsk("wifi6");
		if ($proto6 == "WPA2-EAP" || $proto6 == "WPAand11i-EAP")
		{
			$curr_radius6 = trim(shell_exec("call_qcsapi get_radius_auth_server_cfg wifi6"));
			if(is_qcsapi_error($curr_radius6))
			{
				$radius_arr_len6=0;
			}
			else
			{
				$curr_radius6=explode("\n",$curr_radius6);
				$radius_arr_len6 = count($curr_radius6);
			}
		}
	}

	$ssid7=exec("call_qcsapi get_ssid wifi7");
	if(is_qcsapi_error($ssid7))
	{$ssid7="";$chk7=0;$br7=0;$priority7=0;$pmf7=0; $proto7="NONE";$vlan7="";}
	else
	{
		$chk7=1;
		$vlan7=exec("cat $file | grep wifi2 | awk -F 'vlan=' '{print $2}' | awk -F '&' '{print $1}'");
		$br7=getBroad("wifi7");
		$priority7=getPriority("wifi7");
		$pmf7=getPmf("wifi7");
		$proto7=get_proto("wifi7");
		$psk7=getPsk("wifi7");
		if ($proto7 == "WPA2-EAP" || $proto7 == "WPAand11i-EAP")
		{
			$curr_radius7 = trim(shell_exec("call_qcsapi get_radius_auth_server_cfg wifi7"));
			if(is_qcsapi_error($curr_radius7))
			{
				$radius_arr_len7=0;
			}
			else
			{
				$curr_radius7=explode("\n",$curr_radius7);
				$radius_arr_len7 = count($curr_radius7);
			}
		}
	}
}
//=====================================================
//========================Save Value===================
function setValue()
{
	global $file,$content;
	if ($_POST['action'] == 1)
	{
		$device = $_POST['cmb_interface'];
		$new_chk = $_POST['chk_bss'];
		if ($new_chk=="on")
		{$new_chk=1;}
		else
		{$new_chk=0;}

		$new_ssid = $_POST['txt_ssid'];
		$new_vlan = $_POST['txt_vlan'];
		$old_vlan=exec("cat $file | grep $device | awk -F 'vlan=' '{print $2}' | awk -F '&' '{print $1}'");
		$new_br = $_POST['chk_br'];
		if ($new_br=="on")
		{$new_br=1;}
		else
		{$new_br=0;}

		$new_priority = $_POST['cmb_priority'];

		$new_pmf=$_POST['cmb_pmf'];
		$new_proto = $_POST['cmb_proto'];
		$new_psk = $_POST['txt_psk'];

		$radius_count = $_POST['NumRowsRadius'];
		$chkbox_arr = $_POST['chk_box_radius'];
		$chkbox_arr = explode(",", $chkbox_arr);

		$add_radius = $_POST['add_radius'];
		$del_radius = $_POST['del_radius'];

		$new_ssid_esc=escapeshellarg(escape_any_characters($new_ssid));
		$new_psk_esc=escapeshellarg(escape_any_characters($new_psk));
		$new_pmf_esc=escapeshellarg($new_pmf);
		$new_br_esc=escapeshellarg($new_br);
		$new_priority_esc=escapeshellarg($new_priority);
		$new_vlan_esc=escapeshellarg($new_vlan);

		if ($new_chk == 1)
		{
			exec("call_qcsapi wifi_create_bss $device");
			//Set SSID
			$escaped_new_ssid=escape_any_characters($new_ssid);
			exec("call_qcsapi set_SSID $device $new_ssid_esc");
			//Set Broadcast
			exec("call_qcsapi set_option $device SSID_broadcast $new_br_esc");
			//Set Priority
			exec("call_qcsapi set_priority $device $new_priority_esc");
			$old_line=exec("cat $file | grep $device");
			$new_line="$device:priority=$new_priority";
			//Set VLAN
			if($new_vlan=="")
			{
				$tmp=trim(exec("call_qcsapi show_vlan_config $device"));
				if ($tmp != "VLAN disabled")
				{
					exec("call_qcsapi vlan_config $device disable 0");
				}
			}
			else
			{
				$tmp=trim(exec("call_qcsapi show_vlan_config $device"));
				if ($tmp == "VLAN disabled")
				{
					exec("call_qcsapi vlan_config $device enable 0");
				}
				exec("call_qcsapi vlan_config $device bind $new_vlan_esc");
				$new_line=$new_line."&vlan=$new_vlan";
			}
			$content=str_replace($old_line,$new_line,$content);
			$tmp=file_put_contents($file, $content);
			//Set PMF
			exec("call_qcsapi set_pmf $device $new_pmf_esc");
			//Set Encryption
			set_proto($device,$new_proto);
			//Set PSK
			$tmp_return=exec("call_qcsapi set_passphrase $device 0 $new_psk_esc");
			if($tmp_return=="QCS API error 22: Invalid argument")
			exec("call_qcsapi set_pre_shared_key $device 0 $new_psk_esc");
			//Set Radius server
			if ($add_radius == 1) {
				for($i = 0; $i < $radius_count; $i++) {
					$add_val = $_POST['txtbox_radius'][$i];
					$add_val = explode(",", $add_val);
					exec("call_qcsapi add_radius_auth_server_cfg $device $add_val[0] $add_val[1] $add_val[2]");
				}
			}
			//Delete Radius Server
			if ($del_radius == 1) {
				for($i = 0; $i <= $radius_count; $i++) {
					if ($chkbox_arr[$i] == "true")
                                        {
						$del_val = $_POST['txtbox_radius'][$i];
                                                $del_val = explode(" ", $del_val);
                                                exec("call_qcsapi del_radius_auth_server_cfg $device $del_val[0] $del_val[1]");
                                        }
                                }
                        }
		}
		else if ($new_chk == 0)
		{
			if($new_vlan!="")
				exec("call_qcsapi vlan_config $device unbind $old_vlan");
			$content=str_replace($old_line,"$device:",$content);
			$tmp=file_put_contents($file, $content);
			exec("call_qcsapi wifi_remove_bss $device");
		}
	}
}

//=====================================================
getValue();

if(isset($_POST['action']))
{
	if (!(isset($_POST['csrf_token']) && $_POST['csrf_token'] === get_session_token())) {
		header('Location: login.php');
		exit();
	}
	setValue();
	getValue();
}
?>

<script type="text/javascript">
var radius_arr1 = <?php echo '["' . implode('", "', $curr_radius1) . '"]' ?>;
var radius_arr2 = <?php echo '["' . implode('", "', $curr_radius2) . '"]' ?>;
var radius_arr3 = <?php echo '["' . implode('", "', $curr_radius3) . '"]' ?>;
var radius_arr4 = <?php echo '["' . implode('", "', $curr_radius4) . '"]' ?>;
var radius_arr5 = <?php echo '["' . implode('", "', $curr_radius5) . '"]' ?>;
var radius_arr6 = <?php echo '["' . implode('", "', $curr_radius6) . '"]' ?>;
var radius_arr7 = <?php echo '["' . implode('", "', $curr_radius7) . '"]' ?>;

nonascii = /[^\x20-\x7E]/;
nonhex = /[^A-Fa-f0-9]/g;

function reload()
{
	window.location.href="config_mbss.php";
}

function deleteRadiusRow(tableID)
{
	var chk_box_arr = new Array();
        var table = document.getElementById(tableID);
        var rowCount = table.rows.length;
        var checkCount = 0;
        document.getElementById("del_radius").value = 1;

        for(var i=1; i<rowCount; i++) {
                var row = table.rows[i];
                var chkbox = row.cells[0].childNodes[0];
                chk_box_arr[i-1]=chkbox.checked
        }

        document.getElementById("chk_box_radius").value = chk_box_arr;
        document.getElementById("NumRowsRadius").value = rowCount - 1;

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
                alert("select radius to delete");
        }
        else
        {
		set_visible("tr_warning_radius", true);
        }
}

function CheckIP(ipaddr)
{
        var pattern = /^\d{1,3}(\.\d{1,3}){3}$/;
        if (!pattern.exec(ipaddr))
        {
                return false;
        }
        var aryIP = ipaddr.split('.');

        if (parseInt(aryIP[0]) >= 224 || parseInt(aryIP[0]) == 127 || parseInt(aryIP[0]) == 0)
        {
                return false;
        }
        for(key in aryIP)
        {
                if (parseInt(aryIP[key]) > 255 || parseInt(aryIP[key]) < 0)
                {
                        return false;
                }
        }
        return true;
}

function validate_enterprise_param(rad_ip, rad_port, shared_key)
{
        shared_key.value=shared_key.value.replace(/(\")/g, '\"');

        if (rad_ip.value.length == 0)
        {
                alert("Radius IP address cannot be empty");
                return false;
        }
        if (!CheckIP(rad_ip.value))
        {
                alert("Invalid IP address");
                return false;
        }
        if (rad_port.value.length > 0 && isNaN(rad_port.value) == true)
        {
                alert("Invalid Port Number");
                return false;
        }
	if (rad_port.value < 1 || rad_port.value > 65535)
        {
                alert("Allowed port number range is 1-65535");
                return false;
        }
        if (rad_port.value.length == 0)
        {
                alert("Radius port number cannot be empty");
                return false;
        }
        if (shared_key.value.length == 0)
        {
                alert("Shared key cannot be empty");
                return false;
        }
        if (shared_key.value.length < 8 || shared_key.value.length > 128)
        {
                alert("Allowed passphrase is 8 to 127 ASCII characters or 128 Hexadecimal digits");
                return false;
        }
        if ((nonascii.test(shared_key.value)))
        {
                alert("Allowed passphrase is 8 to 127 ASCII characters or 128 Hexadecimal digits");
                return false;
        }
	if (shared_key.value.length == 128 && (nonhex.test(shared_key.value)))
        {
                alert("Allowed passphrase is 8 to 127 ASCII characters or 128 Hexadecimal digits");
                return false;
        }

        return true;
}

function addRadiusRow(tableID)
{
	set_visible('radius_table', true);

	var radius_ip = document.getElementById("txt_radius_ipaddr");
	var radius_port = document.getElementById("txt_radius_port");
	var shared_key = document.getElementById("txt_shared_key");

	if (!validate_enterprise_param(radius_ip, radius_port, shared_key))
	{
		return false;
	}

	var radius_val = radius_ip.value + " " + radius_port.value + " " + shared_key.value;
	document.getElementById("add_radius").value = 1;
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
	element2.name="txtbox_radius[]";
	element2.value=radius_val;
	element2.readOnly = true;
	cell2.appendChild(element2);

	document.getElementById("NumRowsRadius").value = rowCount;
	document.getElementById("txt_radius_ipaddr").value="";
	document.getElementById("txt_shared_key").value="";

	set_visible("tr_warning_radius", true);
}

function populate_radius(curr_radius)
{
        var table = document.getElementById("radius_table");

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
        element2.name="txtbox_radius[]";
        element2.value=curr_radius;
        element2.readOnly = true;
        cell2.appendChild(element2);
}

function flush_radius_table()
{
	var table = document.getElementById("radius_table");
	var rowCount = table.rows.length;
	for (var i=1; i<rowCount; i++) {
		table.deleteRow(i);
		rowCount--;
		i--;
	}
}

function populate_encryption(pmf_value)
{
	var cmb_encryption = document.getElementById("cmb_proto");
	if (pmf_value == "0") //Disabled
	{
		cmb_encryption.options.length = 3;
		cmb_encryption.options[0].text = "NONE-OPEN"; cmb_encryption.options[0].value = "NONE";
		cmb_encryption.options[1].text = "WPA2-AES"; cmb_encryption.options[1].value = "11i";
		cmb_encryption.options[2].text = "WPA2-AES-Enterprise"; cmb_encryption.options[2].value = "WPA2-EAP";
	}
	else if (pmf_value == "1") //Enabled
	{
		cmb_encryption.options.length = 3;
		cmb_encryption.options[0].text = "NONE-OPEN"; cmb_encryption.options[0].value = "NONE";
		cmb_encryption.options[1].text = "WPA2-AES"; cmb_encryption.options[1].value = "11i";
		cmb_encryption.options[2].text = "WPA2-AES-Enterprise"; cmb_encryption.options[2].value = "WPA2-EAP";
	}
	else if (pmf_value == "2") //Required
	{
		cmb_encryption.options.length = 3;
		cmb_encryption.options[0].text = "NONE-OPEN"; cmb_encryption.options[0].value = "NONE";
		cmb_encryption.options[1].text = "WPA2-AES-SHA256"; cmb_encryption.options[1].value = "11i_pmf";
		cmb_encryption.options[2].text = "WPA2-AES-Enterprise"; cmb_encryption.options[2].value = "WPA2-EAP";
	}
}

function validate_psk()
{
        pw = document.getElementById("txt_psk");
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
        if (pw.value.length == 64 && (nonhex.test(pw.value)))
        {
                alert("Allowed Passphrase is 8 to 63 ASCII characters or 64 Hexadecimal digits");
                return false;
        }

        return true;
}

function validate()
{
	//Validate SSID
	var ssid = document.getElementById("txt_ssid");
	ssid.value=ssid.value.replace(/(\")/g, '\"');

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

	if ((ssid.value[0] == ' ') || (ssid.value[ssid.value.length - 1] == ' '))
	{
		alert("SPACE is not allowed at the start or end of the SSID");
		return false;
	}
	//Validate PSK
	if (document.mainform.cmb_pmf.selectedIndex == 0)
	{
		if (document.mainform.cmb_proto.selectedIndex > 0
			&& document.mainform.cmb_proto.selectedIndex <= 2)
		{
			if (!validate_psk())
				return false;
		}
	}
	else
	{
		if (document.mainform.cmb_proto.selectedIndex > 0
			&& document.mainform.cmb_proto.selectedIndex <= 1)
		{
			if (!validate_psk())
				return false;
		}
	}
	//Validate VLAN
	var vlan = document.getElementById("txt_vlan");
	var num = /^[0-9]*[1-9][0-9]*$/;
	if (vlan.value != "")
	{
		if(num.test(vlan.value))
		{
			if (vlan.value <1 || vlan.value > 4095)
			{
				alert("Vlan ID is only allowed between 1-4095");
				return false;
			}
		}
		else
		{
			alert("Vlan ID is only allowed between 1-4095");
			return false;
		}
	}

	document.mainform.submit();
}

function modechange(obj)
{
	if(obj.name == "cmb_interface")
	{
		if (isset('cmb_interface', 'wifi1'))
		{
			var priority = "<?php echo $priority1; ?>";
			set_control_value('chk_bss', '<?php echo $chk1; ?>', 'checkbox');
			set_control_value('txt_ssid', '<?php echo $ssid1; ?>', 'text');
			set_control_value('txt_vlan', '<?php echo $vlan1; ?>', 'text');
			set_control_value('chk_br', '<?php echo $br1; ?>', 'checkbox');
			set_control_value('cmb_priority', priority, 'combox');

			var pmf = "<?php echo $pmf1; ?>";
			var proto = "<?php echo $proto1; ?>";
			var psk = "<?php echo $psk1; ?>";

			populate_encryption(pmf);
			set_control_value('cmb_pmf', pmf, 'combox');
			set_control_value('cmb_proto', proto, 'combox');
			set_control_value('txt_psk', psk, 'text');

			document.getElementById("radius_display_table").style.display = 'none';
			document.getElementById("radius_table").style.display = 'none';

			if (proto == "NONE" || proto == "" || proto == "WPA2-EAP" || proto =="WPAand11i-EAP")
			{
				set_visible('tr_passphrase', false);
			}

			if (proto == "11i" || proto == "11i_pmf" || proto == "WPAand11i")
			{
				set_visible('tr_passphrase', true);
			}
			if (proto == "WPA2-EAP" || proto == "WPAand11i-EAP")
			{
				set_visible('radius_display_table', true);
				set_visible('radius_table', true);
			}

			var radius_arr_len = "<?php echo $radius_arr_len1; ?>";
			if (radius_arr_len == 0)
			{
				set_visible('radius_table', false);
			} else {
				flush_radius_table();
				for( var i=0; i<radius_arr_len; i++) {
					populate_radius(radius_arr1[i]);
				}
			}
		}
		if (isset('cmb_interface', 'wifi2'))
		{
			var priority = "<?php echo $priority2; ?>";
			set_control_value('chk_bss', '<?php echo $chk2; ?>', 'checkbox');
			set_control_value('txt_ssid', '<?php echo $ssid2; ?>', 'text');
			set_control_value('txt_vlan', '<?php echo $vlan2; ?>', 'text');
			set_control_value('chk_br', '<?php echo $br2; ?>', 'checkbox');
			set_control_value('cmb_priority', priority, 'combox');

			var pmf = "<?php echo $pmf2; ?>";
			var proto = "<?php echo $proto2; ?>";
			var psk = "<?php echo $psk2; ?>";

			populate_encryption(pmf);
			set_control_value('cmb_pmf', pmf, 'combox');
			set_control_value('cmb_proto', proto, 'combox');
			set_control_value('txt_psk', psk, 'text');

			document.getElementById("radius_display_table").style.display = 'none';
			document.getElementById("radius_table").style.display = 'none';

			if (proto == "NONE" || proto == "" || proto == "WPA2-EAP" || proto =="WPAand11i-EAP")
			{
				set_visible('tr_passphrase', false);
			}

			if (proto == "11i" || proto == "11i_pmf" || proto == "WPAand11i")
			{
				set_visible('tr_passphrase', true);
			}
			if (proto == "WPA2-EAP" || proto == "WPAand11i-EAP")
			{
				set_visible('radius_display_table', true);
				set_visible('radius_table', true);
			}

			var radius_arr_len = "<?php echo $radius_arr_len2; ?>";
			if (radius_arr_len == 0) {
				set_visible('radius_table', false);
			} else {
				flush_radius_table();
				for( var i=0; i<radius_arr_len; i++) {
					populate_radius(radius_arr2[i]);
				}
			}
		}
		if (isset('cmb_interface', 'wifi3'))
		{
			var priority = "<?php echo $priority3; ?>";
			set_control_value('chk_bss', '<?php echo $chk3; ?>', 'checkbox');
			set_control_value('txt_ssid', '<?php echo $ssid3; ?>', 'text');
			set_control_value('txt_vlan', '<?php echo $vlan3; ?>', 'text');
			set_control_value('chk_br', '<?php echo $br3; ?>', 'checkbox');
			set_control_value('cmb_priority', priority, 'combox');

			var pmf = "<?php echo $pmf3; ?>";
			var proto = "<?php echo $proto3; ?>";
			var psk = "<?php echo $psk3; ?>";
			populate_encryption(pmf);
			set_control_value('cmb_pmf', pmf, 'combox');
			set_control_value('cmb_proto', proto, 'combox');
			set_control_value('txt_psk', psk, 'text');

			document.getElementById("radius_display_table").style.display = 'none';
			document.getElementById("radius_table").style.display = 'none';

			if (proto == "NONE" || proto == "" || proto == "WPA2-EAP" || proto =="WPAand11i-EAP")
			{
				set_visible('tr_passphrase', false);
			}
			if (proto == "11i" || proto == "11i_pmf" || proto == "WPAand11i")
			{
				set_visible('tr_passphrase', true);
			}

			if (proto == "WPA2-EAP" || proto == "WPAand11i-EAP")
			{
				set_visible('radius_display_table', true);
				set_visible('radius_table', true);
			}

			var radius_arr_len = "<?php echo $radius_arr_len3; ?>";
                        if (radius_arr_len == 0)
                        {
                                set_visible('radius_table', false);
                        }
                        else
                        {
				flush_radius_table();
                                for( var i=0; i<radius_arr_len; i++) {
                                        populate_radius(radius_arr3[i]);
                                }
                        }
		}
		if (isset('cmb_interface', 'wifi4'))
		{
			var priority = "<?php echo $priority4; ?>";
			set_control_value('chk_bss', '<?php echo $chk4; ?>', 'checkbox');
			set_control_value('txt_ssid', '<?php echo $ssid4; ?>', 'text');
			set_control_value('txt_vlan', '<?php echo $vlan4; ?>', 'text');
			set_control_value('chk_br', '<?php echo $br4; ?>', 'checkbox');
			set_control_value('cmb_priority', priority, 'combox');

			var pmf = "<?php echo $pmf4; ?>";
			var proto = "<?php echo $proto4; ?>";
			var psk = "<?php echo $psk4; ?>";
			populate_encryption(pmf);
			set_control_value('cmb_pmf', pmf, 'combox');
			set_control_value('cmb_proto', proto, 'combox');
			set_control_value('txt_psk', psk, 'text');

			document.getElementById("radius_display_table").style.display = 'none';
			document.getElementById("radius_table").style.display = 'none';

			if (proto == "NONE" || proto == "" || proto == "WPA2-EAP" || proto =="WPAand11i-EAP")
			{
				set_visible('tr_passphrase', false);
			}
			if (proto == "11i" || proto == "11i_pmf" || proto == "WPAand11i")
			{
				set_visible('tr_passphrase', true);
			}

			if (proto == "WPA2-EAP" || proto == "WPAand11i-EAP")
			{
				set_visible('radius_display_table', true);
				set_visible('radius_table', true);
			}

			var radius_arr_len = "<?php echo $radius_arr_len4; ?>";
                        if (radius_arr_len == 0)
                        {
                                set_visible('radius_table', false);
                        }
                        else
                        {
				flush_radius_table();
                                for( var i=0; i<radius_arr_len; i++) {
                                        populate_radius(radius_arr4[i]);
                                }
                        }
		}

		if (isset('cmb_interface', 'wifi5'))
		{
			var priority = "<?php echo $priority5; ?>";
			set_control_value('chk_bss', '<?php echo $chk5; ?>', 'checkbox');
			set_control_value('txt_ssid', '<?php echo $ssid5; ?>', 'text');
			set_control_value('txt_vlan', '<?php echo $vlan5; ?>', 'text');
			set_control_value('chk_br', '<?php echo $br5; ?>', 'checkbox');
			set_control_value('cmb_priority', priority, 'combox');

			var pmf = "<?php echo $pmf5; ?>";
			var proto = "<?php echo $proto5; ?>";
			var psk = "<?php echo $psk5; ?>";

			populate_encryption(pmf);
			set_control_value('cmb_pmf', pmf, 'combox');
			set_control_value('cmb_proto', proto, 'combox');
			set_control_value('txt_psk', psk, 'text');

			document.getElementById("radius_display_table").style.display = 'none';
			document.getElementById("radius_table").style.display = 'none';

			if (proto == "NONE" || proto == "" || proto == "WPA2-EAP" || proto =="WPAand11i-EAP")
			{
				set_visible('tr_passphrase', false);
			}

			if (proto == "11i" || proto == "11i_pmf" || proto == "WPAand11i")
			{
				set_visible('tr_passphrase', true);
			}
			if (proto == "WPA2-EAP" || proto == "WPAand11i-EAP")
			{
				set_visible('radius_display_table', true);
				set_visible('radius_table', true);
			}

			var radius_arr_len = "<?php echo $radius_arr_len5; ?>";
			if (radius_arr_len == 0)
			{
				set_visible('radius_table', false);
			} else {
				flush_radius_table();
				for( var i=0; i<radius_arr_len; i++) {
					populate_radius(radius_arr5[i]);
				}
			}
		}

		if (isset('cmb_interface', 'wifi6'))
		{
			var priority = "<?php echo $priority6; ?>";
			set_control_value('chk_bss', '<?php echo $chk6; ?>', 'checkbox');
			set_control_value('txt_ssid', '<?php echo $ssid6; ?>', 'text');
			set_control_value('txt_vlan', '<?php echo $vlan6; ?>', 'text');
			set_control_value('chk_br', '<?php echo $br6; ?>', 'checkbox');
			set_control_value('cmb_priority', priority, 'combox');

			var pmf = "<?php echo $pmf6; ?>";
			var proto = "<?php echo $proto6; ?>";
			var psk = "<?php echo $psk6; ?>";

			populate_encryption(pmf);
			set_control_value('cmb_pmf', pmf, 'combox');
			set_control_value('cmb_proto', proto, 'combox');
			set_control_value('txt_psk', psk, 'text');

			document.getElementById("radius_display_table").style.display = 'none';
			document.getElementById("radius_table").style.display = 'none';

			if (proto == "NONE" || proto == "" || proto == "WPA2-EAP" || proto =="WPAand11i-EAP")
			{
				set_visible('tr_passphrase', false);
			}

			if (proto == "11i" || proto == "11i_pmf" || proto == "WPAand11i")
			{
				set_visible('tr_passphrase', true);
			}
			if (proto == "WPA2-EAP" || proto == "WPAand11i-EAP")
			{
				set_visible('radius_display_table', true);
				set_visible('radius_table', true);
			}

			var radius_arr_len = "<?php echo $radius_arr_len6; ?>";
			if (radius_arr_len == 0)
			{
				set_visible('radius_table', false);
			} else {
				flush_radius_table();
				for( var i=0; i<radius_arr_len; i++) {
				populate_radius(radius_arr6[i]);
				}
			}
		}

		if (isset('cmb_interface', 'wifi7'))
		{
			var priority = "<?php echo $priority7; ?>";
			set_control_value('chk_bss', '<?php echo $chk7; ?>', 'checkbox');
			set_control_value('txt_ssid', '<?php echo $ssid7; ?>', 'text');
			set_control_value('txt_vlan', '<?php echo $vlan7; ?>', 'text');
			set_control_value('chk_br', '<?php echo $br7; ?>', 'checkbox');
			set_control_value('cmb_priority', priority, 'combox');

			var pmf = "<?php echo $pmf7; ?>";
			var proto = "<?php echo $proto7; ?>";
			var psk = "<?php echo $psk7; ?>";

			populate_encryption(pmf);
			set_control_value('cmb_pmf', pmf, 'combox');
			set_control_value('cmb_proto', proto, 'combox');
			set_control_value('txt_psk', psk, 'text');

			document.getElementById("radius_display_table").style.display = 'none';
			document.getElementById("radius_table").style.display = 'none';

			if (proto == "NONE" || proto == "" || proto == "WPA2-EAP" || proto =="WPAand11i-EAP")
			{
				set_visible('tr_passphrase', false);
			}

			if (proto == "11i" || proto == "11i_pmf" || proto == "WPAand11i")
			{
				set_visible('tr_passphrase', true);
			}
			if (proto == "WPA2-EAP" || proto == "WPAand11i-EAP")
			{
				set_visible('radius_display_table', true);
				set_visible('radius_table', true);
			}

			var radius_arr_len = "<?php echo $radius_arr_len7; ?>";
			if (radius_arr_len == 0)
			{
				et_visible('radius_table', false);
			} else {
				flush_radius_table();
				for( var i=0; i<radius_arr_len; i++) {
				populate_radius(radius_arr7[i]);
				}
			}
		}
	}

	if (obj.name == "cmb_proto")
	{
		set_visible('tr_passphrase', false);
		set_visible('radius_display_table', false);
		set_visible('radius_table', false);

		if (document.mainform.cmb_proto.selectedIndex > 0 && document.mainform.cmb_proto.selectedIndex <= 1)
		{
			set_visible('tr_passphrase', true);
		}
		else if (document.mainform.cmb_proto.selectedIndex > 1)
		{
			flush_radius_table();
			set_visible('radius_display_table', true);
			set_visible('radius_table', true);
		}
	}
}

function onload_event()
{
	init_menu();
	var priority = "<?php echo $priority1; ?>";
	var pmf = "<?php echo $pmf1; ?>";
	var proto = "<?php echo $proto1; ?>";
	var psk = "<?php echo $psk1; ?>";
	var radius_arr_len = "<?php echo $radius_arr_len1; ?>";

	populate_encryption(pmf);
	set_control_value('chk_bss', '<?php echo $chk1; ?>', 'checkbox');
	set_control_value('txt_ssid', '<?php echo $ssid1; ?>', 'text');
	set_control_value('txt_vlan', '<?php echo $vlan1; ?>', 'text');
	set_control_value('chk_br', '<?php echo $br1; ?>', 'checkbox');
	set_control_value('cmb_priority', priority, 'combox');

	set_control_value('cmb_pmf', pmf, 'combox');
	set_control_value('cmb_proto', proto, 'combox');
	set_control_value('txt_psk', psk, 'text');

	if (radius_arr_len == 0)
	{
		set_visible('radius_table', false);
	}
	else
	{
		for( var i=0; i<radius_arr_len; i++) {
			populate_radius(radius_arr1[i]);
		}
	}

	//Security
	document.getElementById("radius_display_table").style.display = 'none';
	document.getElementById("radius_table").style.display = 'none';

	if (proto == "NONE" || proto == "" || proto == "WPA2-EAP" || proto =="WPAand11i-EAP")
	{
		set_visible('tr_passphrase', false);
	}

	if (proto == "WPA2-EAP" || proto == "WPAand11i-EAP")
	{
		set_visible('radius_display_table', true);
		set_visible('radius_table', true);
	}
	set_visible("tr_warning_radius", false);
}

</script>

<body class="body" onload="onload_event();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="config_mbss.php" id="mainform" name="mainform" method="post">
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php echo $curr_mode;?>',privilege);
		</script>
	</div>
	<div class="right">
		<div class="righttop">
			<p>CONFIG - MBSS</p>
		</div>
		<div class="rightmain">
			<table class="tablemain">
				 <tr id="tr_if">
					<td>Wifi Interface:</td>
					<td>
						<select name="cmb_interface" id="cmb_interface" class="combox" onchange="modechange(this)">
							<option value="wifi1">wifi1</option>
							<option value="wifi2">wifi2</option>
							<option value="wifi3">wifi3</option>
							<option value="wifi4">wifi4</option>
							<option value="wifi5">wifi5</option>
							<option value="wifi6">wifi6</option>
							<option value="wifi7">wifi7</option>
						</select>
					</td>
				</tr>
				<tr id="tr_l">
					<td class="divline" colspan="5";></td>
				</tr>
				<tr>
					<td>Enable:</br><input name="chk_bss" id="chk_bss" type="checkbox"  class="checkbox"/></td>
					<td>SSID:</br><input name="txt_ssid" type="text" id="txt_ssid" class="textbox"/></td>
					<td>VLAN:</br><input name="txt_vlan" style="width:42px" type="text" id="txt_vlan" class="textbox"/></td>
					<td>Broadcast:</br><input name="chk_br" type="checkbox"  id="chk_br" class="checkbox"/></td>
					<td>Priority:</br><select name="cmb_priority" class="combox" id="cmb_priority" style="width:80px">
							<option value="0">0</option>
							<option value="1">1</option>
							<option value="2">2</option>
							<option value="3">3</option>
						</select></td>
				</tr>
				<tr id="tr_l">
					<td class="divline" colspan="5";></td>
				</tr>
				<tr>
					<td>PMF:</br></td>
					<td>
						<select name="cmb_pmf" class="combox" id="cmb_pmf" onchange="populate_encryption(this.value)">
							<option value="0">Disabled</option>
							<option value="1">Enabled</option>
							<option value="2">Required</option>
						</select>
					</td>
				</tr>
				<tr>
					<td>Encryption:</br></td>
					<td>
						<select name="cmb_proto" class="combox" id="cmb_proto" onchange="modechange(this)"></select>
					</td>
				</tr>
				<tr id="tr_passphrase">
					<td>Passphrase:</br></td>
					<td>
						<input name="txt_psk" type="text" id="txt_psk" class="textbox"/>
					</td>
				</tr>
			</table>
			<table id="radius_display_table">
				<td><font size="2">Radius IP:</font></td>
				<td>
					<input name="txt_radius_ipaddr" type="text" id="txt_radius_ipaddr" class="textbox" style="width:80px;"/>
				</td>
				<td><font size ="2">Radius Port:</font></td>
				<td>
					<input name="txt_radius_port" type="text" id="txt_radius_port" class="textbox" style="width:60px;"/>
				</td>
				<td><font size ="2">Shared Key:</font></td>
				<td>
					<input name="txt_shared_key" type="text" id="txt_shared_key" class="textbox" style="width:80px;"/>
				</td>
				<td><button name="btn_add" id="btn_add" type="button" onclick="addRadiusRow('radius_table');" class="button" style="width:60px;">ADD</button></td>
				<td><button name="btn_del" id="btn_del" type="button" onclick="deleteRadiusRow('radius_table');" class="button" style="width:40px;">DEL</button></td>
				<table id="radius_table" width="350px" border="1">
					<td width="20%">option</td>
					<td width="20%">IP Port shared_key</td>
				</table>
				<input name="NumRowsRadius" id="NumRowsRadius" type="hidden"/>
				<input name="chk_box_radius" id="chk_box_radius" type="hidden"/>
				<input name="add_radius" id="add_radius" type="hidden"/>
				<input name="del_radius" id="del_radius" type="hidden"/>
			</table>
			<table class="tablemain">
				<tr id="tr_l">
					<td class="divline" colspan="5";></td>
				</tr>
				<tr id="tr_warning_radius">
					<td colspan="2"; style="color:red; font-size:smaller;">*To apply the changes, click save button*</td>
				</tr>
			</table>
			<div id="result" style="color:red;visibility:hidden; text-align:left; margin-left:20px; margin-top:20px; font:16px Calibri, Candara, corbel, "Franklin Gothic Book";">
			</div>
			<div class="rightbottom">
				<button name="btn_save" id="btn_save" type="button" onclick="validate();"  class="button">Save</button>
				<button name="btn_cancel" id="btn_cancel" type="button" onclick="reload();"  class="button">Cancel</button>
			</div>
			<input id="action" name="action" type="hidden" value="1">
		</div>
	</div>
</div>
<input type="hidden" name="csrf_token" value="<?php echo get_session_token(); ?>" />
</form>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> | <a href="help/h_mbss.php">Help</a><br />
	<div><?php echo $str_copy ?></div>
</div>

</body>
</html>