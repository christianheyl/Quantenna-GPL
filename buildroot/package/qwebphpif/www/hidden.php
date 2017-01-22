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
$privilege = get_privilege(0);

?>


<script type="text/javascript">
var privilege="<?php echo $privilege; ?>";
</script>

<?php
$curr_mode=exec("call_qcsapi get_mode wifi0");

$curr_pp_name="";
$curr_mspp_name="";
$curr_region="";
$file_region="";
$curr_scs_status="";
$curr_qtm_status="";
$curr_bf_status="";
$curr_coc_status="";
$curr_pp_status="";
$curr_mspp_status="";
$arr="";
$curr_hw_wps_pbc="";
$curr_if_wps_status="";


if (isset($_GET['id']))
{
	$passed_id=substr($_GET['id'],0,1);
	$interface_id="wifi".$passed_id;
}
else
{
	if($_POST['passed_id']!="")
	{
		$passed_id= $_POST['passed_id'];
		$interface_id="wifi".$passed_id;
	}
	else
	{
		$interface_id="wifi0";
	}
}




function read_data()
{
	$contents = file_get_contents("/mnt/jffs2/wireless_conf.txt");
	parse_str(trim($contents));
	$arr = array($bw,$channel,$mode,$mcs,$ssid,$agg,$bf,$sec);
	return $arr;
}


function load_value()
{
	global $curr_mode,$curr_pp_name,$curr_pp_status,$curr_nonwps_pp_status,$curr_mspp_name,$curr_mspp_status,$curr_region,$file_region,$curr_scs_status,$curr_qtm_status,$curr_bf_status,$arr,$curr_coc_status,$ssid0,$mac0,$arr_ssid,$arr_index,$sum_active_interface,$ssid1,$mac1,$ssid2,$mac2,$ssid3,$mac3,$ssid4,$mac4,$ssid5,$mac5,$ssid6,$mac6,$ssid7,$mac7,$interface_id,$curr_hw_wps_pbc,$curr_if_wps_status;
	read_data();

	$arr_ssid="";
	$arr_index="";
	$sum_active_interface= 1;

	//=============Get SSID=======================
	$ssid0=exec("call_qcsapi get_SSID wifi0");
	$mac0=exec("call_qcsapi get_macaddr wifi0");
	$arr_ssid[0]="wifi0"."("."$mac0".")";
	$arr_index=0;

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
	}


	$arr = read_data();
	$curr_scs_status = exec("call_qcsapi get_scs_status wifi0") == "Enabled (1)"? 1:0;
	$curr_qtm_status = exec("call_qcsapi qtm wifi0 show") == "QCS API error 1: Operation not permitted"? 0:1;
	$curr_bf_status = $arr[6];
	$curr_coc_status = exec("qpm show | grep ^level | awk '{print $2}'") == "0"? 0:1;
	$curr_if_wps_status = exec("call_qcsapi get_wps_configured_state $interface_id")=="configured"?1:0;

	if ($curr_mode == "Access point")
	{
		//load ATT pp
		$curr_pp_status=exec("call_qcsapi get_wps_access_control $interface_id");
		$curr_pp_name=exec("call_qcsapi registrar_get_pp_devname $interface_id");
		$curr_nonwps_pp_status=exec("call_qcsapi get_non_wps_pp_enable $interface_id");
		if(is_qcsapi_error($curr_pp_name))
		{
			$curr_pp_name="";
		}

		//load MS pp
		$curr_mspp_status=exec("call_qcsapi get_pairing_enable $interface_id");
		if(is_qcsapi_error($curr_mspp_status))
		{
			$curr_mspp_status=0;
		}
		$curr_mspp_name=exec("call_qcsapi get_pairing_id $interface_id");
		if(is_qcsapi_error($curr_mspp_name))
		{
			$curr_mspp_name="";
		}

		$curr_hw_wps_pbc=substr(exec("call_qcsapi registrar_get_default_pbc_bss"),-1);
	}
	$curr_region = trim(shell_exec("cat /etc/region"));
	$file_region = exec("call_qcsapi get_config_param wifi0 region");
	if(is_qcsapi_error($file_region))
	{
		$file_region="";
	}
}

function set_value()
{
	global $curr_mode,$curr_pp_name,$curr_pp_status,$curr_nonwps_pp_status,$curr_mspp_name,$curr_mspp_status,$curr_region,$file_region,$curr_scs_status,$curr_qtm_status,$curr_bf_status,$arr,$curr_coc_status,$interface_id;
	//set att pp status
	$new_pp_name=$_POST["txt_pp_name"];
	$new_pp_status = $_POST['ppStatus'];
	$new_nonwps_pp_status = $_POST['nonwpsppStatus'];
	$new_hw_wps_pbc = $_POST['cmb_interface_hb'];

	if(strcmp($new_pp_status,$curr_pp_status)!=0)
	{
		if(0==$new_pp_status)
		{
			exec("call_qcsapi set_wps_access_control $interface_id 0");
		}
		else
		{
			exec("call_qcsapi set_wps_access_control $interface_id 1");
		}
	}

	if(strcmp($new_nonwps_pp_status,$curr_nonwps_pp_status)!=0)
	{
		if(0==$new_nonwps_pp_status)
		{
			exec("call_qcsapi set_non_wps_pp_enable $interface_id 0");
		}
		else
		{
			exec("call_qcsapi set_non_wps_pp_enable $interface_id 1");
		}
	}

	//set att pp string
	if($new_pp_name!=$curr_pp_name&&$new_pp_status==1)
	{
		if ($new_pp_name=="")
		{
			exec("call_qcsapi registrar_set_pp_devname $interface_id \"\"");
		}
		else
		{
			exec("call_qcsapi registrar_set_pp_devname $interface_id \"$new_pp_name\"");
		}
	}

	//set ms pp status
	$new_mspp_status = $_POST['msppStatus'];
	$new_mspp_name=$_POST["txt_mspp_name"];
	if(strcmp($new_mspp_status,$curr_mspp_status)!=0)
	{
		if(0==$new_mspp_status)
		{
			exec("call_qcsapi set_pairing_enable $interface_id 0");
		}
		else
		{
			exec("call_qcsapi set_pairing_enable $interface_id 1");
		}
	}

	//set ms pp string
	if($new_mspp_name!=$curr_mspp_name&&$new_mspp_status==1)
	{
		if ($new_mspp_name=="")
		{
			exec("call_qcsapi set_pairing_id $interface_id \"\"");
		}
		else
		{
			exec("call_qcsapi set_pairing_id $interface_id \"$new_mspp_name\"");
		}
	}


	if(strcmp($new_hw_wps_pbc,$curr_hw_wps_pbc)!=0)
	{
		$hw_wps_pbc_if="wifi".$new_hw_wps_pbc;
		exec("call_qcsapi registrar_set_default_pbc_bss $hw_wps_pbc_if");
	}

	//Set Region
	$new_region=$_POST['cmb_region'];
	if($curr_region != $new_region)
	{
		$contents = file_get_contents("/mnt/jffs2/wireless_conf.txt");
		/* If wireless_conf.txt has no such field */
		if($file_region == "")
		{
			$curr_region = $new_region;
			$region_str = "&region=".$new_region;
			$contents = $contents.$region_str;
			file_put_contents("/mnt/jffs2/wireless_conf.txt", $contents);
		}
		else
		{
			$region_config_file = "/proc/bootcfg/tx_power_QSR1000_$new_region.txt";
			$sta_region_config_file = "/proc/bootcfg/tx_power_QSR1000_sta_$new_region.txt";
			if(file_exists($region_config_file) ||
				(($curr_mode == "Station") && file_exists($sta_region_config_file)))
			{
				$old_region_str = "region=".$curr_region;
				$new_region_str = "region=".$new_region;
				$contents = str_replace($old_region_str, $new_region_str, $contents);
				file_put_contents("/mnt/jffs2/wireless_conf.txt", $contents);
			}
			else
			{
				confirm("Configuration file '$region_config_file' doesn`t exist. Please upload.");
				return false;
			}
		}
		$_SESSION['qtn_can_reboot']=TRUE;
		header('Location: system_rebooted.php');
		exit();
	}

	//set SCS
	$new_scs_status = $_POST['scsStatus'];
	if(strcmp($new_scs_status,$curr_scs_status)!=0)
	{
		$contents = file_get_contents("/mnt/jffs2/wireless_conf.txt");
		if(0==$new_scs_status)
		{
			exec("call_qcsapi enable_scs wifi0 0");
			exec("call_qcsapi set_channel wifi0 $old_chan");
		}
		else
		{
			exec("call_qcsapi enable_scs wifi0 1");
			// confirm("Warning: Channel can't be changed manually when scs is enabled.");
		}
		$curr_scs_status = "scs=".$curr_scs_status;
		$new_scs_status = "scs=".$new_scs_status;
		$contents = str_replace($curr_scs_status, $new_scs_status, $contents);
		file_put_contents("/mnt/jffs2/wireless_conf.txt", $contents);
	}

	//set QTM
	$new_qtm_status = $_POST['qtmStatus'];
	if(strcmp($new_qtm_status,$curr_qtm_status)!=0)
	{
		$contents = file_get_contents("/mnt/jffs2/wireless_conf.txt");
		if(0==$new_qtm_status)
		{
			exec("call_qcsapi qtm wifi0 set enabled 0");
		}
		else
		{
			exec("call_qcsapi qtm wifi0 set enabled 1");
		}
		$curr_qtm_status = "qtm=".$curr_qtm_status;
		$new_qtm_status = "qtm=".$new_qtm_status;
		$contents = str_replace($curr_qtm_status, $new_qtm_status, $contents);
		file_put_contents("/mnt/jffs2/wireless_conf.txt", $contents);
	}

	//set BeamForming
	$new_bf_status = $_POST['bfStatus'];
	if(strcmp($new_bf_status,$curr_bf_status)!=0)
	{
		$contents = file_get_contents("/mnt/jffs2/wireless_conf.txt");
		if(0==$new_bf_status)
		{
			exec("bfoff");
		}
		else
		{
			exec("bfon");
		}
		$curr_bf_status = "bf=".$curr_bf_status;
		$new_bf_status = "bf=".$new_bf_status;
		$contents = str_replace($curr_bf_status, $new_bf_status, $contents);
		file_put_contents("/mnt/jffs2/wireless_conf.txt", $contents);
	}

	//set COC
	$new_coc_status = $_POST['cocStatus'];
	if(strcmp($new_coc_status,$curr_coc_status)!=0)
	{
		if(0==$new_coc_status)
		{
			exec("qpm level 0");
		}
		else
		{
			exec("qpm level 1");
		}

	}
}

load_value();
if (isset($_POST['action']))
{
	if (!(isset($_POST['csrf_token']) && $_POST['csrf_token'] === get_session_token())) {
		header('Location: login.php');
		exit();
	}
	set_value();
	load_value();
}
?>


<script language="javascript">
var mode = "<?php echo $curr_mode; ?>";

function validate()
{
	var cf = document.forms[0];
	var curr_if_wps_status = "<?php echo $curr_if_wps_status; ?>";

	if (cf.enblscs[0].checked)
	{
		cf.scsStatus.value = 0;
	}
	else
	{
		cf.scsStatus.value = 1;
	}

	if (cf.enblqtm[0].checked)
	{
		cf.qtmStatus.value = 0;
	}
	else
	{
		cf.qtmStatus.value = 1;
	}

	if (cf.enblbf[0].checked)
	{
		cf.bfStatus.value = 0;
	}
	else
	{
		cf.bfStatus.value = 1;
	}

	if (cf.enblcoc[0].checked)
	{
		cf.cocStatus.value = 0;
	}
	else
	{
		cf.cocStatus.value = 1;
	}
	if (mode == "Access point")
	{
		if (cf.enblpp[0].checked)
		{
			cf.ppStatus.value = 0;
		}
		else if (cf.enblpp[1].checked && curr_if_wps_status == 0)
		{
			alert("WPS needs to be enabled firstly for enabling Pairing Protection ");
			return false;
		}
		else if (cf.enblpp[1].checked && curr_if_wps_status == 1)
		{
			cf.ppStatus.value = 1;
		}

		if (cf.nonwpspp[0].checked)
		{
			cf.nonwpsppStatus.value = 0;
		}
		else
		{
			cf.nonwpsppStatus.value = 1;
		}

		if (cf.enblmspp[0].checked)
		{
			cf.msppStatus.value = 0;
		}
		else
		{
			cf.msppStatus.value = 1;
		}

		cf.passed_id.value = cf.cmb_interface.value;

	}
	txt_pp_name=document.getElementById("txt_pp_name");
	if (txt_pp_name.value.length>128)
	{
		alert("Invalid length of PPS1 string");
		return false;
	}
/*
	txt_mspp_name=document.getElementById("txt_mspp_name");
	if (txt_mspp_name.value.length>22)
	{
		alert("Invalid length of PPS2 string");
		return false;
	}
*/
	document.mainform.submit();
}

function ppstatuschange(obj)
{
	if(obj.value==0)
	{
		set_disabled("txt_pp_name",true);
	}
	else if (obj.value==1)
	{
		set_disabled("txt_pp_name",false);
	}

}

function msppstatuschange(obj)
{
	if(obj.value==0)
	{
		set_disabled("txt_mspp_name",true);
	}
	else if (obj.value==1)
	{
		set_disabled("txt_mspp_name",false);
	}

}


function populate_interface_list(index)
{
	var cmb_if = document.getElementById(index);
	var passed_id="";
	if (index=="cmb_interface")
	{
		passed_id="<?php echo $passed_id; ?>";
	}
	else if (index=="cmb_interface_hb")
	{
		passed_id="<?php echo $curr_hw_wps_pbc; ?>";
	}

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
		if (passed_id == cmb_if.options[i].value)
		{
			cmb_if.options[i].selected=true;
		}


	}

}

function reload()
{
	var tmp_index= "<?php echo $arr_index; ?>";
	var interface_index=tmp_index.split(",");

	var cmb_if = document.getElementById("cmb_interface");
	if (mode == "Station")
	{
		window.location.href="hidden.php";
	}
	else
	{
		window.location.href="hidden.php?id="+interface_index[cmb_if.selectedIndex];
	}
}

function modechange()
{
	reload();
}


function onload_event()
{

	init_menu();
	if (mode == "Access point")
	{
		populate_interface_list("cmb_interface");
		populate_interface_list("cmb_interface_hb");
	}
	else
	{
		set_visible('tr_if', false);
		set_visible('tr_mspp', false);
		set_visible('tr_msps', false);
		set_visible('tr_wpspp', false);
		set_visible('tr_wpsps', false);
		set_visible('tr_nwpp', false);
		set_visible('tr_hb', false);
		set_visible('tr_l1', false);
		set_visible('tr_l2', false);
		set_visible('tr_l3', false);
		set_visible('tr_aplist', false);
		set_visible('tr_l5', false);
	
	}
}

</script>

<body class="body" onload="onload_event();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="hidden.php" id="mainform" name="mainform" method="post">
<input id="action" name="action" type="hidden" >
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php echo $curr_mode;?>',privilege);
		</script>
	</div>
	<div class="right">
	<div class="righttop">Hidden</div>
		<div class="rightmain">
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="35%">Region:</td>
					<td width="65%">
						<select name="cmb_region" class="combox" id="cmb_region">
						<?php
							$region_list = trim(shell_exec("call_qcsapi get_regulatory_regions wifi0"));
							$region_array = explode(',', $region_list);
							$region_array[] = "none";
							foreach($region_array as $r)
							{
								echo "<option value=\"$r\"";
								if($curr_region == $r) echo " selected ";
								echo "> $r </option>\n";
							}
						?>
						</select>
					</td>
				</tr>
				<tr>
					<td>SCS:</td>
					<td>
						<input type="radio" name="enblscs" id="enblscs"  <?php if(0==$curr_scs_status){echo "checked=\"checked\"";}?>>
							Disable&nbsp;&nbsp;
						<input type="radio" name="enblscs" id="enblscs"  <?php if(1==$curr_scs_status){echo "checked=\"checked\"";}?>>
							Enable
						<input type="hidden" name="scsStatus" />
					</td>
				</tr>
				<tr>
					<td>QTM:</td>
					<td>
						<input type="radio" name="enblqtm" id="enblqtm"  <?php if(0==$curr_qtm_status){echo "checked=\"checked\"";}?>>
							Disable&nbsp;&nbsp;
						<input type="radio" name="enblqtm" id="enblqtm"  <?php if(1==$curr_qtm_status){echo "checked=\"checked\"";}?>>
							Enable
						<input type="hidden" name="qtmStatus" />
					</td>
				</tr>

				<tr>
					<td>BF:</td>
					<td>
						<input type="radio" name="enblbf" id="enblbf"  <?php if(0==$curr_bf_status){echo "checked=\"checked\"";}?>>
							Disable&nbsp;&nbsp;
						<input type="radio" name="enblbf" id="enblbf"  <?php if(1==$curr_bf_status){echo "checked=\"checked\"";}?>>
							Enable
						<input type="hidden" name="bfStatus" />
					</td>
				</tr>
				<tr>
					<td>CoC:</td>
					<td>
						<input type="radio" name="enblcoc" id="enblcoc"  <?php if(0==$curr_coc_status){echo "checked=\"checked\"";}?>>
							Disable&nbsp;&nbsp;
						<input type="radio" name="enblcoc" id="enblcoc"  <?php if(1==$curr_coc_status){echo "checked=\"checked\"";}?>>
							Enable
						<input type="hidden" name="cocStatus" />
					</td>
				</tr>
				<tr id="tr_l2">
					<td class="divline" colspan="2";></td>
				</tr>
				<tr id="tr_if">
					<td width="40%">Wifi Interface:</td>
					<td width="60%">
						<select name="cmb_interface" class="combox" id="cmb_interface" onchange="modechange(this)">
						</select>
					</td>
				</tr>
				<tr id="tr_wpspp">
					<td>PPS1:</td>
					<td>
						<input type="radio" name="enblpp" id="enblpp" onclick="ppstatuschange(this);" value="0" <?php if(0==$curr_pp_status){echo "checked=\"checked\"";}?>>
							Disable&nbsp;&nbsp;
						<input type="radio" name="enblpp" id="enblpp" onclick="ppstatuschange(this);" value="1" <?php if(1==$curr_pp_status){echo "checked=\"checked\"";}?>>
							Enable
						<input type="hidden" name="ppStatus" />
					</td>
				</tr>
				<tr id="tr_wpsps">
					<td width="30%">PPS1 String:</td>
					<td width="70%">
						<input name="txt_pp_name" type="text" id="txt_pp_name" class="textbox" style="width:167px;" value="<?php echo htmlspecialchars($curr_pp_name,ENT_QUOTES);?>" <?php if(0==$curr_pp_status){echo "disabled=\"disabled\"";}?>/>
					</td>
				</tr>
				<tr id="tr_nwpp">
					<td>PPS1+:</td>
					<td>
						<input type="radio" name="nonwpspp" id="nonwpspp"  value="0" <?php if(0==$curr_nonwps_pp_status){echo "checked=\"checked\"";}?>>
							Disable&nbsp;&nbsp;
						<input type="radio" name="nonwpspp" id="nonwpspp"  value="1" <?php if(1==$curr_nonwps_pp_status){echo "checked=\"checked\"";}?>>
							Enable
						<input type="hidden" name="nonwpsppStatus" />
					</td>
				</tr>
				<tr id="tr_mspp">
					<td>PPS2:</td>
					<td>
						<input type="radio" name="enblmspp" id="enblmspp" onclick="msppstatuschange(this);" value="0" <?php if(0==$curr_mspp_status){echo "checked=\"checked\"";}?>>
							Disable&nbsp;&nbsp;
						<input type="radio" name="enblmspp" id="enblmspp" onclick="msppstatuschange(this);" value="1" <?php if(1==$curr_mspp_status){echo "checked=\"checked\"";}?>>
							Enable
						<input type="hidden" name="msppStatus" />
					</td>
				</tr>
				<tr id="tr_msps">
					<td width="30%">PPS2 String:</td>
					<td width="70%">
						<input name="txt_mspp_name" type="text" id="txt_mspp_name" class="textbox" style="width:167px;" value="<?php echo htmlspecialchars($curr_mspp_name,ENT_QUOTES);?>" <?php if(0==$curr_mspp_status){echo "disabled=\"disabled\"";}?>/>
					</td>
				</tr>
				<tr id="tr_l3">
					<td class="divline" colspan="2";></td>
				</tr>
				<tr id="tr_hb">
					<td>WPS Hardware Button:</td>
					<td>
						<select name="cmb_interface_hb" id="cmb_interface_hb" class="combox" >
						</select>
					</td>
				</tr>
				<tr id="tr_l4">
					<td class="divline" colspan="2";></td>
				</tr>
				<tr id="tr_aplist">
					<td>AP List:</td>
					<td>
						<button name="btn_save" id="btn_save" type="button" onclick="popnew('aplist.php');" class="button">AP List</button>
					</td>
				</tr>
				<tr id="tr_l5">
					<td class="divline" colspan="2";></td>
				</tr>
	
			</table>
			<div class="rightbottom">
				<button name="btn_save" id="btn_save" type="button" onclick="validate();" class="button">Save</button>
				<button name="btn_cancel" id="btn_cancel" type="button"  onclick="reload();" class="button">Cancel</button>
			</div>
		</div>
	</div>
	</div>
</div>
<input type="hidden" name="passed_id" id="passed_id"  />
<input type="hidden" name="passed_interface_value" id="passed_interface_value" />
<input type="hidden" name="csrf_token" value="<?php echo get_session_token(); ?>" />
</form>
<div class="bottom">Quantenna Communications</div>
</body>
</html>

