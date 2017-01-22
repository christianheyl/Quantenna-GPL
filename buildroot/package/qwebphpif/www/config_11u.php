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

function get_eap_value($eap_method)
{
	if ($eap_method =="EAP_TYPE_TLS")
		return "13";
	if ($eap_method =="EAP_TYPE_SIM")
		return "18";
	if ($eap_method =="EAP_TYPE_TTLS")
		return "21";
	if ($eap_method =="EAP_TYPE_AKA")
		return "23";
	if ($eap_method =="PEAP")
		return "25";
}

function get_auth_method_value($auth_method)
{
	if ($auth_method =="Non-EAP Inner Authentication")
		return "2";
	if ($auth_method =="Inner authentication EAP Method")
		return "3";
	if ($auth_method =="Credential Type")
		return "5";
}

function get_auth_param_value($auth_param)
{
	if ($auth_param =="PAP" || $auth_param == "SIM")
		return "1";
	if ($auth_param =="CHAP" || $auth_param == "USIM")
		return "2";
	if ($auth_param =="MSCHAP" || $auth_param == "NFC Secure Element")
		return "3";
	if ($auth_param =="MSCHAPV2" || $auth_param == "Hardware Token")
		return "4";
	if ($auth_param =="Softoken")
		return "5";
	if ($auth_param =="Certificate")
		return "6";
	if ($auth_param =="username/password")
		return "7";
	if ($auth_param =="Anonymous")
		return "9";
	if ($auth_param =="Vendor Specific")
		return "10";
}

function get_eap_auth_value($eap_auth_val)
{
	$eap_val=explode("[",$eap_auth_val);
        $eap_auth_count = count($eap_val);
	$eap_method = get_eap_value($eap_val[0]);
	$auth_method_param = "";
	for($i = 1; $i <$eap_auth_count; $i++) {
		$eap_auth = substr_replace($eap_val[$i], "", -1);
		$auth_val = explode(":", $eap_auth);
		$auth_method = get_auth_method_value($auth_val[0]);
		$auth_param = get_auth_param_value($auth_val[1]);
		$auth_method_param= $auth_method_param."[".$auth_method.":".$auth_param."]";
	}
	return $eap_method.$auth_method_param;
}

function get_eap_name($eap_method)
{
    if ($eap_method =="13")
        return "EAP_TYPE_TLS";
    if ($eap_method =="18")
        return "EAP_TYPE_SIM";
    if ($eap_method =="21")
        return "EAP_TYPE_TTLS";
    if ($eap_method =="23")
        return "EAP_TYPE_AKA";
    if ($eap_method =="25")
        return "PEAP";
}

function get_auth_method_name($auth_method, $auth_param)
{
    $auth_method_val="";
    $auth_param_val="";
    if ($auth_method == "2")
    {
        $auth_method_val = "Non-EAP Inner Authentication";
        if($auth_param == "1")
            $auth_param_val = "PAP";
        if($auth_param == "2")
            $auth_param_val = "CHAP";
        if($auth_param == "3")
            $auth_param_val = "MSCHAP";
        if($auth_param == "4")
            $auth_param_val = "MSCHAPV2";
    }
    if ($auth_method == "5")
    {
        $auth_method_val = "Credential Type";
        if($auth_param == "1")
            $auth_param_val = "SIM";
        if($auth_param == "2")
            $auth_param_val = "USIM";
        if($auth_param == "3")
            $auth_param_val = "NFC Secure Element";
        if($auth_param == "4")
            $auth_param_val = "Hardware Token";
        if($auth_param == "5")
            $auth_param_val = "Softoken";
        if($auth_param == "6")
            $auth_param_val = "Certificate";
        if($auth_param == "7")
            $auth_param_val = "username/password";
        if($auth_param == "9")
            $auth_param_val = "Anonymous";
        if($auth_param == "10")
            $auth_param_val = "Vendor Specific";
    }

    return $auth_method_val.":".$auth_param_val;
}

function get_eap_auth_name($eap_auth_val)
{
    $eap_val=explode("[",$eap_auth_val);
    $eap_auth_count = count($eap_val);
    $eap_method = get_eap_name($eap_val[0]);
    $auth_method_param = "";
    for($i = 1; $i <$eap_auth_count; $i++) {
        $eap_auth = substr_replace($eap_val[$i], "", -1);
        $auth_val = explode(":", $eap_auth);
        $auth = get_auth_method_name($auth_val[0], $auth_val[1]);
        $auth_method_param= $auth_method_param."[".$auth."]";
    }
    return $eap_method.$auth_method_param;
}

$curr_mode=exec("call_qcsapi get_mode wifi0");
$curr_11u_status="1";
$curr_internet_access="";
$curr_net_type="";
$curr_net_auth_type="";
$curr_hessid="";
$curr_comeback_delay="";
$curr_roaming="";
$curr_domain="";
$nai_realm="";
$nai_realm_arr_len="0";
$curr_venue_gr="";
$curr_venue_type="";
$curr_venue_name="";
$venue_arr_len=0;
$venue_name_list = array();
$cell_net_list="";
$cell_net_arr_len = 0;
$curr_ipv4="";
$curr_ipv6="";
$roaming_arr_len=0;
function get_value()
{
	global  $curr_11u_status,$curr_internet_access,$curr_net_type,$curr_net_auth_type,$curr_hessid,
		$curr_comeback_delay,$curr_roaming,$curr_domain,$cell_net_list,$cell_net_arr_len,
		$curr_ipv4,$curr_ipv6,$roaming_arr_len;
	global  $nai_realm,$nai_realm_arr_len;
	global  $curr_venue_gr,$curr_venue_type,$curr_venue_name,$venue_arr_len,$venue_name_list;

	//Get 802.11u Status
	$curr_11u_status=trim(shell_exec("call_qcsapi get_interworking wifi0"));
	if(is_qcsapi_error($curr_11u_status))
		$curr_11u_status="0";
	//Get Internet Access
	$curr_internet_access=trim(shell_exec("call_qcsapi get_80211u_params wifi0 internet"));
	//Get Network Type
	$curr_net_type=trim(shell_exec("call_qcsapi get_80211u_params wifi0 access_network_type"));
	//Get Network Auth Type
	$curr_net_auth_type=trim(shell_exec("call_qcsapi get_80211u_params wifi0 network_auth_type"));
	//Get HESSID
	$curr_hessid=trim(shell_exec("call_qcsapi get_80211u_params wifi0 hessid"));
	if(is_qcsapi_error($curr_hessid))
		$curr_hessid="";
	//Get GAS comeback delay
	$curr_comeback_delay=trim(shell_exec("call_qcsapi get_80211u_params wifi0 gas_comeback_delay"));
	if(is_qcsapi_error($curr_comeback_delay))
		$curr_comeback_delay="";
	//Get Roaming Consortium
	$curr_roaming=trim(shell_exec("call_qcsapi get_roaming_consortium wifi0"));
	if(is_qcsapi_error($curr_roaming))
	{
		$roaming_arr_len = 0;
	}
	else
	{
		$curr_roaming=explode("\n",$curr_roaming);
		$roaming_arr_len = count($curr_roaming);
	}
	//Get IP Address Availability
	$curr_ip_addr_avail=trim(shell_exec("call_qcsapi get_80211u_params wifi0 ipaddr_type_availability"));
	if(is_qcsapi_error($curr_ip_addr_avail))
	{
		$curr_ipv4 = "";
		$curr_ipv6 = "";
	}
	else
	{
		$curr_ip_addr_avail = base_convert($curr_ip_addr_avail, 16, 10);
		$curr_ipv4 = ($curr_ip_addr_avail >> 2) & 0x3f;
		$curr_ipv6 = $curr_ip_addr_avail & 0x3;
	}
	//Get Domain Name
	$curr_domain=trim(shell_exec("call_qcsapi get_80211u_params wifi0 domain_name"));
	if(is_qcsapi_error($curr_domain))
		$curr_domain="";

	//Get Nai Realm
	$nai_realm=trim(shell_exec("call_qcsapi get_nai_realms wifi0"));
        if(is_qcsapi_error($nai_realm))
        {
                $nai_realm_arr_len = 0;
        }
        else
        {
                $nai_realm=explode("\n",$nai_realm);
                $nai_realm_arr_len = count($nai_realm);

                for($i=0; $i < $nai_realm_arr_len; $i++) {
                        $realm_val=explode(",",$nai_realm[$i]);
                        $realm_len = count($realm_val);
                        $eap_auth_list = "";
                        for($j=2; $j<$realm_len; $j++) {
                                $eap_auth_val = get_eap_auth_name($realm_val[$j]);
                                $eap_auth_list = $eap_auth_list.$eap_auth_val.",";
                        }
                        $eap_auth_list = substr_replace($eap_auth_list, "", -1);
                        $nai_realm[$i] = $realm_val[0].",".$realm_val[1].",".$eap_auth_list;
                }
        }

	//Get Venue Group
	$curr_venue_gr=trim(shell_exec("call_qcsapi get_80211u_params wifi0 venue_group"));
	//Get Venue Type
	$curr_venue_type=trim(shell_exec("call_qcsapi get_80211u_params wifi0 venue_type"));
	if(is_qcsapi_error($curr_venue_gr))
	{
		$curr_venue_gr=0;
		$curr_venue_type=0;
	}
	//Get language and Venue Name
	$curr_venue_name=trim(shell_exec("call_qcsapi get_venue_name wifi0"));
        if(is_qcsapi_error($curr_venue_name))
        {
                $venue_arr_len = 0;
        }
        else
        {
                $curr_venue_name=explode("\n",$curr_venue_name);
                $venue_arr_len = count($curr_venue_name);

                for ($i=0; $i< $venue_arr_len; $i++) {
                        $local_venue =substr($curr_venue_name[$i],2);
                        list($local_venue) = explode('"', $local_venue);
                        $venue_name_list[$i] = $local_venue;
                }
        }

	//Get Country Code And Network Code
	$cell_net_list=trim(shell_exec("call_qcsapi get_80211u_params wifi0 anqp_3gpp_cell_net"));
        if(is_qcsapi_error($cell_net_list))
        {
                $cell_net_arr_len = 0;
        }
        else
        {
                $cell_net_list=explode(";",$cell_net_list);
                $cell_net_arr_len = count($cell_net_list);
        }
}

function set_value()
{
	global  $curr_11u_status,$curr_internet_access,$curr_net_type,$curr_net_auth_type,$curr_hessid,
		$curr_comeback_delay,$curr_domain,$curr_venue_gr,$curr_venue_type,$curr_ipv4,$curr_ipv6;
	global  $cell_net_list,$cell_net_arr_len;

	//Save
	if ($_POST['action'] == 0)
	{
		$new_11u_status=$_POST['cmb_11u_status'];
		$new_internet_access=$_POST['chk_internet_access'];
		if ($new_internet_access=="on")
		{$new_internet_access=1;}
		else
		{$new_internet_access=0;}
		$new_net_type=$_POST['cmb_net_type'];
		$new_net_auth_type=$_POST['cmb_net_auth_type'];
		$new_hessid=$_POST['txt_hessid'];
		$new_comeback_delay=$_POST['txt_comeback_delay'];

		$new_ipv4=$_POST['cmb_ipv4'];
		$new_ipv6=$_POST['cmb_ipv6'];
		$new_domain=$_POST['txt_domain_name'];
		$new_realm=$_POST['txt_realm'];
		$new_eap=$_POST['cmb_eap'];
		$new_auth=$_POST['cmb_auth'];
		$new_credential=$_POST['cmb_credential'];

		$new_venue_gr=$_POST['cmb_v_gr'];
		$new_venue_type=$_POST['cmb_v_type'];

		$venue_count = $_POST['NumRows_venue'];
                $venue_chkbox_arr = $_POST['chk_box_venue'];
                $venue_chkbox_arr = explode(",", $venue_chkbox_arr);

                $add_venue_name = $_POST['add_venue_name'];
                $del_venue_name = $_POST['del_venue_name'];


		$count = $_POST['NumRows'];
		$chkbox_arr = $_POST['chk_box'];
		$chkbox_arr = explode(",", $chkbox_arr);

		$del_roaming = $_POST['del_roaming'];
                $add_roaming = $_POST['add_roaming'];

		$cell_net_count = $_POST['NumRows_cell_net'];
                $add_cell_net = $_POST['add_cell_net'];

                $cell_net_chkbox_arr = $_POST['chk_box_cell_net'];
                $cell_net_chkbox_arr = explode(",", $cell_net_chkbox_arr);
                $del_cell_net = $_POST['del_cell_net'];

		$nai_realm_count = $_POST['NumRows_nai_realm'];
		$add_nai_realm = $_POST['add_nai_realm'];

		$nai_realm_chkbox_arr = $_POST['chk_box_nai_realm'];
		$nai_realm_chkbox_arr = explode(",", $nai_realm_chkbox_arr);
		$del_nai_realm = $_POST['del_nai_realm'];

		//Set 11u Status
		if ($new_11u_status != $curr_11u_status)
		{
			exec("call_qcsapi set_interworking wifi0 $new_11u_status");
		}
		//Set Internet Access
		if ($new_internet_access != $curr_internet_access)
		{
			exec("call_qcsapi set_80211u_params wifi0 internet $new_internet_access");
		}
		//Set Network Type
		if ($new_net_type != $curr_net_type)
		{
			exec("call_qcsapi set_80211u_params wifi0 access_network_type $new_net_type");
		}
		//Set Network Auth Type
		if ($new_net_auth_type != $curr_net_auth_type)
		{
			 exec("call_qcsapi set_80211u_params wifi0 network_auth_type $new_net_auth_type");
		}
		//Set Hessid
		if ($new_hessid != $curr_hessid)
		{
			exec("call_qcsapi set_80211u_params wifi0 hessid $new_hessid");
		}
		//Remove Hessid parameter from hostapd.conf file
		if ($curr_hessid != "" && $new_hessid =="")
		{
			exec("call_qcsapi remove_11u_param wifi0 hessid");
		}
		//Set GAS comeback delay
		if ($new_comeback_delay != $curr_comeback_delay)
		{
			exec("call_qcsapi set_80211u_params wifi0 gas_comeback_delay $new_comeback_delay");
		}
		//Remove GAS comeback delay parameter from hostapd.conf file
		if ($curr_comeback_delay != "" && $new_comeback_delay =="")
		{
			exec("call_qcsapi remove_11u_param wifi0 gas_comeback_delay");
		}
		//Set Roaming Consortium
		if ($add_roaming == 1) {
			for($i = 0; $i < $count; $i++) {
				$add_val = $_POST['txtbox_roaming'][$i];
				exec("call_qcsapi add_roaming_consortium wifi0 $add_val");
			}
		}
		//Remove Roaming Consortium
		if ($del_roaming == 1) {
			for($i = 0; $i <= $count; $i++) {
				if ($chkbox_arr[$i] == "true")
				{
					$del_val = $_POST['txtbox_roaming'][$i];
					exec("call_qcsapi del_roaming_consortium wifi0 $del_val");
				}
			}
		}

		//Set IP Address Availability
		if ($new_ipv4 != $curr_ipv4 || $new_ipv6 != $curr_ipv6)
		{
			exec("call_qcsapi set_80211u_params wifi0 ipaddr_type_availability $new_ipv4 $new_ipv6");
		}
		//Set Domain Name
		if ($new_domain != $curr_domain)
		{
			exec("call_qcsapi set_80211u_params wifi0 domain_name $new_domain");
		}
		//Remove Domain Name parameter from hostapd.conf file
		if ($curr_domain != "" && $new_domain =="")
		{
			exec("call_qcsapi remove_11u_param wifi0 domain_name");
		}
		//Set Nai Realm
		if ($add_nai_realm == 1)
		{
			for($i = 0; $i < $nai_realm_count; $i++) {
				$nai_realm = $_POST['txtbox_nai_realm'][$i];
				$realm_val=explode(",",$nai_realm);
				$realm_len = count($realm_val);
				$eap_auth_list = "";
				for($j=2; $j<$realm_len; $j++) {
					$eap_auth_val = get_eap_auth_value($realm_val[$j]);
					$eap_auth_list = $eap_auth_list.$eap_auth_val.",";
				}
				$eap_auth_list = substr_replace($eap_auth_list, "", -1);
				exec("call_qcsapi add_nai_realm wifi0 $realm_val[0] $realm_val[1] $eap_auth_list");
			}
		}

		//Remove Nai Realm parameter from hostapd.conf file
		if ($del_nai_realm == 1) {
			for($i = 0; $i <= $nai_realm_count; $i++) {
				if ($nai_realm_chkbox_arr[$i] == "true")
				{
					$nai_realm_val = $_POST['txtbox_nai_realm'][$i];
					$realm_val=explode(",",$nai_realm_val);
					exec("call_qcsapi del_nai_realm wifi0 $realm_val[1]");
				}
			}
		}
		//Set Venue Group
                if ($new_venue_gr != $curr_venue_gr)
                {
                        exec("call_qcsapi set_80211u_params wifi0 venue_group $new_venue_gr");
                }
                //Set Venue Type
                if ($new_venue_type != $curr_venue_type)
                {
                        exec("call_qcsapi set_80211u_params wifi0 venue_type $new_venue_type");
                }
                //Set  Language And Venue Name
		if ($add_venue_name == 1) {
                        for($i = 0; $i < $venue_count; $i++) {
                                $add_venue_name = $_POST['txtbox_venue'][$i];
                                list($lang,$venue_name)=explode(":", $add_venue_name);
                                $venue_name = '"'.$venue_name.'"';
                                exec("call_qcsapi add_venue_name wifi0 $lang $venue_name");
                        }
                }
		//Remove venue_name parameter from hostapd.conf file
		if ($del_venue_name == 1) {
                        for($i = 0; $i <= $venue_count; $i++) {
                                if ($venue_chkbox_arr[$i] == "true")
                                {
                                        $del_venue_name = $_POST['txtbox_venue'][$i];
                                        list($lang,$venue_name)=explode(":", $del_venue_name);
                                        $venue_name = '"'.$venue_name.'"';
                                        exec("call_qcsapi del_venue_name wifi0 $lang $venue_name");
                                }
                        }
                }
		//Set Country Code And Network Code
		if ($add_cell_net == 1) {
                        $cell_net_val = "";
                        for($i = 0; $i < $cell_net_count; $i++) {
                                $add_value = $_POST['txtbox_cell_net'][$i];
                                $add_value = $add_value.";";
                                $cell_net_val = $cell_net_val.$add_value;
                        }
                        $cell_net_val = substr_replace($cell_net_val, "", -1);
                        $cell_net_val = '"'.$cell_net_val.'"';
                        exec("call_qcsapi set_80211u_params wifi0 anqp_3gpp_cell_net $cell_net_val");
                }
		//Remove anqp_3gpp_cell_net parameter from hostapd.conf file
		if ($del_cell_net == 1) {
                        $del_cell_net_list = array();
                        $del_count = 0;
                        //Get the array of anqp_3gpp_cell_net to delete
                        for($i = 0, $j = 0; $i <= $cell_net_count; $i++) {
                                if ($cell_net_chkbox_arr[$i] == "true")
                                {
                                        $del_val = $_POST['txtbox_cell_net'][$i];
                                        $del_cell_net_list[$j++] = $del_val;
                                        $del_count++;
                                }
                        }

                        if ($cell_net_arr_len == $del_count)
                        {
                                exec("call_qcsapi remove_11u_param wifi0 anqp_3gpp_cell_net");
                        }
			else
                        {
                                $result=array_diff($cell_net_list,$del_cell_net_list);
                                $local_cell_net_value="";
                                for ($i = 0; $i <= $cell_net_arr_len; $i++) {
                                        if ($result[$i])
                                        {
                                                $value = $result[$i];
                                                $value = $value.";";
                                                $local_cell_net_value = $local_cell_net_value.$value;
                                        }
                                }
                                $local_cell_net_value = substr_replace($local_cell_net_value, "", -1);
                                $local_cell_net_value = '"'.$local_cell_net_value.'"';
                                exec("call_qcsapi set_80211u_params wifi0 anqp_3gpp_cell_net $local_cell_net_value");
                        }
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

var roaming_arr = <?php echo '["' . implode('", "', $curr_roaming) . '"]' ?>;
var venue_name_arr = <?php echo '["' . implode('", "', $venue_name_list) . '"]' ?>;
var cell_net_arr = <?php echo '["' . implode('", "', $cell_net_list) . '"]' ?>;
var nai_realm_arr = <?php echo '["' . implode('", "', $nai_realm) . '"]' ?>;

function populate_venue_type(venue_group)
{
	var cmb_v_type = document.getElementById("cmb_v_type");
	var curr_venue_type = "<?php echo $curr_venue_type; ?>";
	cmb_v_type.options.length = 0;

	if (venue_group == "0")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED ","0"));
        }
	else if (venue_group == "1")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED ASSEMBLY","0"));
                cmb_v_type.options.add(new Option("ARENA","1"));
                cmb_v_type.options.add(new Option("STADIUM","2"));
                cmb_v_type.options.add(new Option("PASSENGER TERMINAL","3"));
                cmb_v_type.options.add(new Option("AMPHITHEATER ","4"));
                cmb_v_type.options.add(new Option("AMUSEMENT PARK ","5"));
                cmb_v_type.options.add(new Option("PLACE OF WORSHIP","6"));
                cmb_v_type.options.add(new Option("CONVENTION CENTER","7"));
                cmb_v_type.options.add(new Option("LIBRARY","8"));
                cmb_v_type.options.add(new Option("MUSEUM","9"));
                cmb_v_type.options.add(new Option("RESTAURANT","10"));
                cmb_v_type.options.add(new Option("THEATER","11"));
                cmb_v_type.options.add(new Option("BAR","12"));
                cmb_v_type.options.add(new Option("COFFEE SHOP","13"));
                cmb_v_type.options.add(new Option("ZOO","14"));
                cmb_v_type.options.add(new Option("EMERGENCY COORDINATION","15"));
        }
	else if (venue_group == "2")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED BUSSINESS","0"));
                cmb_v_type.options.add(new Option("DOCTOR","1"));
                cmb_v_type.options.add(new Option("BANK","2"));
                cmb_v_type.options.add(new Option("FIRE STATION","3"));
                cmb_v_type.options.add(new Option("POLICE STATION","4"));
                cmb_v_type.options.add(new Option("POST OFFICE","6"));
                cmb_v_type.options.add(new Option("PROFESSIONAL OFFICE","7"));
                cmb_v_type.options.add(new Option("RESEARCH AND DEVELOPMENT","8"));
                cmb_v_type.options.add(new Option("ATTORNEY OFFICE","9"));
        }
	else if (venue_group == "3")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED EDUCATIONAL","0"));
                cmb_v_type.options.add(new Option("SCHOOL, PRIMARY","1"));
                cmb_v_type.options.add(new Option("SCHOOL, SECONDARY ","2"));
                cmb_v_type.options.add(new Option("UNIVERSITY OR COLLEGE","3"));
        }
	else if (venue_group == "4")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED FACTORY","0"));
                cmb_v_type.options.add(new Option("FACTORY ","1"));
        }
        else if (venue_group == "5")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED INSTITUTIONAL","0"));
                cmb_v_type.options.add(new Option("HOSPITAL","1"));
                cmb_v_type.options.add(new Option("CARE FACILITY","2"));
                cmb_v_type.options.add(new Option("PRISON OR JAIL","5"));
        }
        else if (venue_group == "6")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED MERCANTILE","0"));
                cmb_v_type.options.add(new Option("RETAIL STORE","1"));
                cmb_v_type.options.add(new Option("GROCERY MARKET","2"));
                cmb_v_type.options.add(new Option("AUTOMOTIVE SERVICE STATION","3"));
                cmb_v_type.options.add(new Option("SHOPPING MALL","4"));
                cmb_v_type.options.add(new Option("GAS STATION","5"));
        }
	else if (venue_group == "7")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED RESIDENTIAL","0"));
                cmb_v_type.options.add(new Option("PRIVATE RESIDENCE ","1"));
                cmb_v_type.options.add(new Option("HOTEL OR MOTEL","2"));
                cmb_v_type.options.add(new Option("DORMITORY ","3"));
                cmb_v_type.options.add(new Option("BOARDING HOUSE","4"));
        }
        else if (venue_group == "8")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED STORAGE","0"));
        }
        else if (venue_group == "9")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED UTILITY","0"));
        }
	else if (venue_group == "10")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED VEHICULAR","0"));
                cmb_v_type.options.add(new Option("AUTOMOBILE","1"));
                cmb_v_type.options.add(new Option("AIRPLANE","2"));
                cmb_v_type.options.add(new Option("BUS","3"));
                cmb_v_type.options.add(new Option("FERRY","4"));
                cmb_v_type.options.add(new Option("SHIP","5"));
                cmb_v_type.options.add(new Option("TRAIN","6"));
                cmb_v_type.options.add(new Option("MOTOR BIKE","7"));
        }
        else if (venue_group == "11")
        {
                cmb_v_type.options.add(new Option("UNSPECIFIED OUTDOOR","0"));
                cmb_v_type.options.add(new Option("MUNI-MESH NETWORK","1"));
                cmb_v_type.options.add(new Option("CITY PARK","2"));
                cmb_v_type.options.add(new Option("REST AREA","3"));
                cmb_v_type.options.add(new Option("TRAFFIC CONTROL","4"));
                cmb_v_type.options.add(new Option("BUS STOP","5"));
                cmb_v_type.options.add(new Option("KIOSK","6"));
        }

	for (var i = 0; i < cmb_v_type.options.length; i++)
	{
		if (cmb_v_type.options[i].value == curr_venue_type)
			cmb_v_type.options[i].selected = true;
	}
}

function modechange(obj)
{
	if (obj.name == "cmb_11u_status")
	{
		if (isset('cmb_11u_status', '0'))
		{
			set_disabled('chk_internet_access', true);
	                set_disabled('cmb_net_type', true);
			set_disabled('cmb_net_auth_type', true);
			set_disabled('txt_hessid', true);
			set_disabled('txt_comeback_delay', true);
			set_disabled('txt_oui', true);
			set_disabled('cmb_ipv4', true);
			set_disabled('cmb_ipv6', true);
			set_disabled('txt_domain_name', true);
			set_disabled('txt_realm', true);
			set_disabled('cmb_eap', true);
			set_disabled('cmb_auth', true);
			set_disabled('cmb_credential', true);
			set_disabled('cmb_v_gr', true);
			set_disabled('cmb_v_type', true);
			set_disabled('txt_lang', true);
			set_disabled('txt_v_name', true);
			set_disabled('txt_country_code', true);
			set_disabled('txt_net_code', true);
			set_disabled('btn_roaming_add', true);
			set_disabled('btn_roaming_del', true);
			set_disabled('btn_add_country', true);
			set_disabled('btn_del_country', true);
			set_disabled('btn_add_venue', true);
			set_disabled('btn_del_venue', true);
			set_disabled('btn_realm_add',true);
			set_disabled('btn_realm_del',true);
			set_disabled('btn_eap_add',true);
			set_disabled('btn_eap_del',true);
			set_disabled('btn_auth_add',true);
			set_disabled('btn_auth_del',true);
			set_disabled('btn_eap_add_realm',true);
			set_disabled('btn_auth_add_eap',true);
		}
		else
		{
			set_disabled('chk_internet_access', false);
                        set_disabled('cmb_net_type', false);
                        set_disabled('cmb_net_auth_type', false);
                        set_disabled('txt_hessid', false);
			set_disabled('txt_comeback_delay', false);
                        set_disabled('txt_oui', false);
			set_disabled('cmb_ipv4', false);
			set_disabled('cmb_ipv6', false);
                        set_disabled('txt_domain_name', false);
                        set_disabled('txt_realm', false);
                        set_disabled('cmb_eap', false);
                        set_disabled('cmb_auth', false);
                        set_disabled('cmb_credential', false);
                        set_disabled('cmb_v_gr', false);
                        set_disabled('cmb_v_type', false);
                        set_disabled('txt_lang', false);
                        set_disabled('txt_v_name', false);
                        set_disabled('txt_country_code', false);
                        set_disabled('txt_net_code', false);
			set_disabled('btn_roaming_add', false);
			set_disabled('btn_roaming_del', false);
			set_disabled('btn_add_country', false);
			set_disabled('btn_del_country', false);
			set_disabled('btn_add_venue', false);
			set_disabled('btn_del_venue', false);
			set_disabled('btn_realm_add',false);
			set_disabled('btn_realm_del',false);
			set_disabled('btn_eap_add',false);
			set_disabled('btn_eap_del',false);
			set_disabled('btn_auth_add',false);
			set_disabled('btn_auth_del',false);
			set_disabled('btn_eap_add_realm',false);
			set_disabled('btn_auth_add_eap',false);
		}
	}
	if (obj.name == "cmb_v_gr")
	{
		populate_venue_type(obj.value);
	}
}

function deleteRoamingRow(tableID)
{
	var chk_box_arr = new Array();
        var table = document.getElementById(tableID);
        var rowCount = table.rows.length;
	var checkCount = 0;
	document.getElementById("del_roaming").value = 1;

        for(var i=1; i<rowCount; i++) {
		var row = table.rows[i];
                var chkbox = row.cells[0].childNodes[0];
		chk_box_arr[i-1]=chkbox.checked
	}

	document.getElementById("chk_box").value = chk_box_arr;
	document.getElementById("NumRows").value = rowCount - 1;

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
		alert("select roaming consortium to delete");
	}
	else
	{
		set_visible('tr_warning', true);
	}
}

function validate_roaming_consortium(roaming)
{
        var HexPattern = /^[0-9a-fA-F]+$/;
        if (roaming.value.length > 0 && HexPattern.test(roaming.value) == false)
        {
                alert("Invalid Roaming Consortium");
                return false;
        }

	if (roaming.value.length > 30 || roaming.value.length < 6 || roaming.value.length & 1)
	{
		alert("Invalid Roaming Consortium");
		return false;
	}

	return true;
}

function addRoamingRow(tableID)
{
	set_visible('oui_table', true);
	var oui_value = document.getElementById("txt_oui");
	if (!validate_roaming_consortium(oui_value))
	{
		return false;
	}
	document.getElementById("add_roaming").value = 1;
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
	element2.name="txtbox_roaming[]";
	element2.value=oui_value.value;
	element2.readOnly = true;
	cell2.appendChild(element2);

	document.getElementById("NumRows").value = rowCount;
	document.getElementById("txt_oui").value="";

	set_visible('tr_warning', true);
}

function deleteVenueNameRow(tableID)
{
	var chk_box_arr = new Array();
        var table = document.getElementById(tableID);
        var rowCount = table.rows.length;
	var checkCount = 0;
	document.getElementById("del_venue_name").value = 1;

        for(var i=1; i<rowCount; i++) {
		var row = table.rows[i];
                var chkbox = row.cells[0].childNodes[0];
		chk_box_arr[i-1]=chkbox.checked
	}

	document.getElementById("chk_box_venue").value = chk_box_arr;
	document.getElementById("NumRows_venue").value = rowCount - 1;

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
		alert("select roaming consortium to delete");
	}
	else
	{
		set_visible('tr_warning_venue', true);
	}
}

function validate_lang_code(lang)
{
	var AlphaPattern = /^[a-zA-Z]+$/;
	if (lang.value.length == 0)
	{
		alert("LANGUAGE CODE can not be empty (Require alphabetical characters between 2 or 3)");
		return false;
	}

        if (((lang.value.length > 0) && (AlphaPattern.test(lang.value) == false)) ||
		(lang.value.length < 2) || (lang.value.length > 3))
        {
		alert("Invalid LANGUAGE CODE (valid values are alphabetical characters between 2 or 3)");
                return false;
        }

	return true;
}

function validate_venue_name(venue_name)
{
	if (venue_name.length == 0)
        {
                alert("Venue Name can not be empty (Require characters length between 1 to 252)");
                return false;
        }

	if (venue_name.length > 252)
	{
		alert("Invalid Venue Name (characters length should be between 1 to 252)");
		return false;
	}
	return true;
}
function addVenueNameRow(tableID)
{
	set_visible('venue_table', true);
	var lang = document.getElementById("txt_lang");
	var ven_name = document.getElementById("txt_v_name");
	var venue_name = ven_name.value.replace(/\r?\n/g,'\\n');

	if (!validate_lang_code(lang) || !validate_venue_name(venue_name))
	{
		return false;
	}
	var venue_name_val = lang.value + ":" + venue_name;

	document.getElementById("add_venue_name").value = 1;

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
	element2.name="txtbox_venue[]";
	element2.value=venue_name_val;
	element2.readOnly = true;
	cell2.appendChild(element2);

	document.getElementById("NumRows_venue").value = rowCount;
	document.getElementById("txt_lang").value="";
	document.getElementById("txt_v_name").value="";

	set_visible('tr_warning_venue', true);
}

function delete_cell_net_Row(tableID)
{
	var chk_box_arr = new Array();
        var table = document.getElementById(tableID);
        var rowCount = table.rows.length;
	var checkCount = 0;
	document.getElementById("del_cell_net").value = 1;

        for(var i=1; i<rowCount; i++) {
		var row = table.rows[i];
                var chkbox = row.cells[0].childNodes[0];
		chk_box_arr[i-1]=chkbox.checked;
	}

	document.getElementById("chk_box_cell_net").value = chk_box_arr;
	document.getElementById("NumRows_cell_net").value = rowCount - 1;

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
		alert("select anqp 3gpp cellular network to delete");
	}
	else
	{
		set_visible('tr_warning_cell_net', true);
	}
}

function validate_mcc(mcc)
{
	if (mcc.value.length == 0)
	{
		alert("Country Code can not be empty");
		return false;
	}

	if (mcc.value.length > 0 && isNaN(mcc.value) == true)
        {
                alert("Invalid Country Code");
                return false;
        }

	if (mcc.value.length > 3)
        {
                alert("Invalid Country Code");
                return false;
        }
	return true;
}

function validate_mnc(mnc)
{
	if (mnc.value.length == 0)
	{
		alert("Network Code can not be empty");
		return false;
	}

	if (mnc.value.length > 0 && isNaN(mnc.value) == true)
        {
                alert("Invalid Network Code");
                return false;
        }

	if (mnc.value.length > 3)
        {
                alert("Invalid Network Code");
                return false;
        }
	return true;
}

function add_cell_net_Row(tableID)
{
	set_visible('cell_net_table', true);
	var mcc = document.getElementById("txt_country_code");
	var mnc = document.getElementById("txt_net_code");
	if (!validate_mcc(mcc) || !validate_mnc(mnc))
	{
		return false;
	}
	var cell_net_val = mcc.value + "," + mnc.value;

	document.getElementById("add_cell_net").value = 1;

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
	element2.name="txtbox_cell_net[]";
	element2.value=cell_net_val;
	element2.readOnly = true;
	cell2.appendChild(element2);

	document.getElementById("NumRows_cell_net").value = rowCount;
	document.getElementById("txt_country_code").value="";
	document.getElementById("txt_net_code").value="";

	set_visible('tr_warning_cell_net', true);
}

function populate_auth_param(auth_method)
{
	var cmb_auth_param = document.getElementById("cmb_auth_param");

	cmb_auth_param.options.length = 0;
	if (auth_method == "2")
        {
                cmb_auth_param.options.add(new Option("PAP","1"));
		cmb_auth_param.options.add(new Option("CHAP","2"));
		cmb_auth_param.options.add(new Option("MSCHAP","3"));
		cmb_auth_param.options.add(new Option("MSCHAPV2","4"));
        }

	if (auth_method == "5")
	{
		cmb_auth_param.options.add(new Option("SIM","1"));
		cmb_auth_param.options.add(new Option("USIM","2"));
		cmb_auth_param.options.add(new Option("NFC Secure Element","3"));
		cmb_auth_param.options.add(new Option("Hardware Token","4"));
		cmb_auth_param.options.add(new Option("Softoken","5"));
		cmb_auth_param.options.add(new Option("Certificate","6"));
		cmb_auth_param.options.add(new Option("username/password","7"));
		cmb_auth_param.options.add(new Option("Anonymous","9"));
		cmb_auth_param.options.add(new Option("Vendor Specific","10"));
	}
}

function delete_auth_Row(tableID)
{
	var auth_table = document.getElementById(tableID);
	var rowCount = auth_table.rows.length;

	for(var i=1; i<rowCount; i++) {
		var row = auth_table.rows[i];
		var chkbox = row.cells[0].childNodes[0];
		if(null != chkbox && true == chkbox.checked) {
                        auth_table.deleteRow(i);
                        rowCount--;
                        i--;
		}
        }
}

function delete_eap_Row(tableID)
{
	var eap_table = document.getElementById(tableID);
        var rowCount = eap_table.rows.length;

        for(var i=1; i<rowCount; i++) {
                var row = eap_table.rows[i];
                var chkbox = row.cells[0].childNodes[0];
                if(null != chkbox && true == chkbox.checked) {
                        eap_table.deleteRow(i);
                        rowCount--;
                        i--;
                }
        }

	set_visible('auth_display_table', false);
        set_visible('auth_table', false);
        set_visible('btn_auth_add_eap', false);
        set_visible('tr_auth', false);
}

function delete_nai_realm_Row(tableID)
{
	var chk_box_arr = new Array();
	var table = document.getElementById(tableID);
	var rowCount = table.rows.length;
	var checkCount = 0;
	document.getElementById("del_nai_realm").value = 1;

	for(var i=1; i<rowCount; i++) {
                var row = table.rows[i];
                var chkbox = row.cells[0].childNodes[0];
                chk_box_arr[i-1]=chkbox.checked
        }

	document.getElementById("chk_box_nai_realm").value = chk_box_arr;
	document.getElementById("NumRows_nai_realm").value = rowCount - 1;


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
		alert("select Nai Realm to delete");
	}
	else
	{
		set_visible('tr_nai_realm_warning', true);
	}

}

function add_eap_row_to_realm()
{
	//Get Realm Value
	var realm_table = document.getElementById("realm_table");
	var rowCount = realm_table.rows.length;
	var row_realm = realm_table.rows[rowCount-1];
	var realm = row_realm.cells[1].childNodes[0];

	document.getElementById("add_nai_realm").value = 1;
	//Row count of Nai Realm table
	document.getElementById("NumRows_nai_realm").value = rowCount - 1;

	//Delete Realm Column
	realm_table.rows[rowCount -1].deleteCell(1);

	var nai_realm = realm.value;
	//Get EAP value
	var eap_table = document.getElementById("eap_table");
	var rowCount = eap_table.rows.length;
	for(var i=1; i<rowCount; i++) {
		var row = eap_table.rows[i];
		var eap = row.cells[1].childNodes[0];
		nai_realm = nai_realm + "," + eap.value;
	}

	//Delete EAP Row
        for(var i=1; i<rowCount; i++) {
                eap_table.deleteRow(i);
                rowCount--;
                i--;
        }

	//Add Eap Method to Realm value
	var cell2 = row_realm.insertCell(1);
        var element2 = document.createElement("input");
        element2.type="text";
        element2.name="txtbox_nai_realm[]";
	element2.style.width = 400 + 'px';
        element2.value=nai_realm;
        element2.readOnly = true;
        cell2.appendChild(element2);

	//Hide EAP Control
	set_visible('eap_display_table', false);
	set_visible('eap_table', false);
	set_visible('btn_eap_add_realm', false);
	set_visible('tr_eap', false);
	set_visible('tr_nai_realm_warning', true);
	set_visible('tr_apply_eap', false);

	//Hide Auth Auth Control
	set_visible('auth_display_table', false);
        set_visible('auth_table', false);
        set_visible('btn_auth_add_eap', false);
        set_visible('tr_auth', false);
        set_visible('tr_apply_auth', false);
}

function add_auth_row_to_eap()
{
	//Check the rowcount in Eap table
	var eap_table = document.getElementById("eap_table");
	var rowCount = eap_table.rows.length;

	//Check EAP Method
	var eap_row = eap_table.rows[rowCount -1];
	var eap = eap_row.cells[1].childNodes[0];
	var eap_auth_value = eap.value;

	eap_table.rows[rowCount-1].deleteCell(1);

	//Add Auth to EAP value
	var auth_table = document.getElementById("auth_table");
	var rowCount = auth_table.rows.length;
	for(var i=1; i<rowCount; i++) {
		var row = auth_table.rows[i];
		var auth_val = row.cells[1].childNodes[0];
		eap_auth_value = eap_auth_value + "[" + auth_val.value + "]";
	}

	//Delete Auth Row
	for(var i=1; i<rowCount; i++) {
		auth_table.deleteRow(i);
		rowCount--;
		i--;
	}

	//Add Auth Method and Auth Param to EAP Method
	var cell2 = eap_row.insertCell(1);
        var element2 = document.createElement("input");
        element2.type="text";
        element2.name="txtbox_ng[]";
	element2.style.width = 400 + 'px';
        element2.value=eap_auth_value;
        element2.readOnly = true;
        cell2.appendChild(element2);

	//Hide Auth Control
	set_visible('auth_display_table', false);
	set_visible('auth_table', false);
	set_visible('btn_auth_add_eap', false);
	set_visible('tr_auth', false);
	set_visible('tr_apply_auth', false);

}


function changeAuth(obj)
{
	populate_auth_param(obj.value);
}

function add_auth_Row(tableID)
{
	var auth_method = document.getElementById("cmb_auth");
	var auth_method_selectedText = auth_method.options[auth_method.selectedIndex].text;
	var auth_param	 = document.getElementById("cmb_auth_param");
	var auth_param_selectedText = auth_param.options[auth_param.selectedIndex].text;
	var auth_type = auth_method_selectedText + ":" + auth_param_selectedText;

	var table = document.getElementById(tableID);

        var rowCount = table.rows.length;
	var row= table.insertRow(rowCount);

        var cell1 = row.insertCell(0);
        var element1 = document.createElement("input");
        element1.type = "checkbox";
        element1.name="chkbox[]";
        cell1.appendChild(element1);

	var cell2 = row.insertCell(1);
	var element2 = document.createElement("input");
	element2.type="text";
	element2.name="txtbox_auth[]";
	element2.value=auth_type;
	element2.readOnly = true;
	cell2.appendChild(element2);

	set_visible('tr_apply_auth', true);
}

function add_eap_Row(tableID)
{
	var eap = document.getElementById("cmb_eap");
	var auth = document.getElementById("cmb_auth");
	var eap_selectedText = eap.options[eap.selectedIndex].text;

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
	element2.name="txtbox_eap[]";
	element2.value=eap_selectedText;
	element2.readOnly = true;
	cell2.appendChild(element2);

	set_visible('auth_display_table', true);
	set_visible('auth_table', true);
        set_visible('btn_auth_add_eap', true);
        set_visible('tr_auth', true);
	auth.value = "2";
	populate_auth_param(2);
	set_visible('tr_apply_eap', true);
}

function validate_realm(realm)
{
	DomainPattern = new RegExp("^[A-Za-z0-9-]+(\\.[A-Za-z0-9-]+)*(\\.[A-Za-z]{2,})$");
	if (realm.value.length == 0)
        {
                alert("Realm can not be empty");
                return false;
        }

	if (realm.value.length > 0 && DomainPattern.test(realm.value) == false)
        {
                alert("Invalid Realm Name");
                return false;
        }

	return true;
}

function add_nai_realm_Row(tableID)
{
	set_visible('realm_table', true);

	var encoding = document.getElementById("cmb_encoding");
	var realm = document.getElementById("txt_realm");

	if (!validate_realm(realm))
	{
		return false;
	}

	var realm_val = encoding.value + "," + realm.value;

	var realm_table = document.getElementById(tableID);

        var rowCount = realm_table.rows.length;
        var row = realm_table.insertRow(rowCount);

        var cell1 = row.insertCell(0);
        var element1 = document.createElement("input");
        element1.type = "checkbox";
        element1.name="chkbox[]";
        cell1.appendChild(element1);

	var cell2 = row.insertCell(1);
	var element2 = document.createElement("input");
	element2.type="text";
	element2.name="txtbox_nai_realm[]";
	element2.value=realm_val;
	element2.readOnly = true;
	cell2.appendChild(element2);

	document.getElementById("txt_realm").value="";

	set_visible('eap_display_table', true);
	set_visible('eap_table', true);
        set_visible('btn_eap_add_realm', true);
        set_visible('tr_eap', true);
}

function reload()
{
	window.location.href="config_11u.php";
}

function validate()
{
	//Validate GAS comeback delay
	var gas_comeback_delay = document.getElementById("txt_comeback_delay");

	if (((gas_comeback_delay.value.length > 0) && isNaN(gas_comeback_delay.value)) ||
		((gas_comeback_delay.value < 0) || (gas_comeback_delay.value > 65535)))
	{
		alert("Invalid GAS comeback delay (valid values are between 0 and 65535)");
		return false;
	}
	//Validate For MAC Address
	var MACAddress = document.getElementById("txt_hessid").value;
	var MACRegex=new RegExp("^([0-9a-fA-F][0-9a-fA-F]:){5}([0-9a-fA-F][0-9a-fA-F])$");
	if (MACAddress.length > 0 &&  MACRegex.test(MACAddress) == false)
	{
		alert("Invalide HESSID, It should be valid Mac Address");
		return false;
	}
	//Validate Domain Name
	DomainPattern = new RegExp("^[A-Za-z0-9-]+(\\.[A-Za-z0-9-]+)*(\\.[A-Za-z]{2,})$");
	var domain_name = document.getElementById("txt_domain_name");
	if (domain_name.value.length > 0 && DomainPattern.test(domain_name.value) == false)
	{
		alert("Invalid Domain Name");
		return false;
	}

	document.mainform.submit();
}

function populate_roaming(curr_oui)
{
	var table = document.getElementById("oui_table");

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
        element2.name="txtbox_roaming[]";
        element2.value=curr_oui;
        element2.readOnly = true;
        cell2.appendChild(element2);
}

function populate_venue_name(curr_venue_name)
{
        var table = document.getElementById("venue_table");

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
        element2.name="txtbox_venue[]";
        element2.value=curr_venue_name;
        element2.readOnly = true;
        cell2.appendChild(element2);
}

function populate_3gpp_cell_net(curr_cell_net)
{
        var table = document.getElementById("cell_net_table");

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
        element2.name="txtbox_cell_net[]";
        element2.value=curr_cell_net;
        element2.readOnly = true;
        cell2.appendChild(element2);
}

function populate_nai_realm(curr_nai_realm)
{
        var table = document.getElementById("realm_table");

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
        element2.name="txtbox_nai_realm[]";
        element2.style.width = 400 + 'px';
        element2.value=curr_nai_realm;
        element2.readOnly = true;
        cell2.appendChild(element2);
}

function onload_event()
{
	var venue_group = "<?php echo $curr_venue_gr; ?>";
	var curr_11u_status = "<?php echo $curr_11u_status; ?>";
	var domain_name = "<?php echo $curr_domain; ?>";
	var roaming_arr_len = "<?php echo $roaming_arr_len; ?>";
	var venue_name_arr_len = "<?php echo $venue_arr_len; ?>";
	var cell_net_arr_len = "<?php echo $cell_net_arr_len; ?>";
	var nai_realm_arr_len = "<?php echo $nai_realm_arr_len; ?>";

	init_menu();

	if (domain_name == "")
	{
		document.getElementById("txt_domain_name").value = "quantenna.com";
	}
	else
	{
		document.getElementById("txt_domain_name").value = domain_name;
	}

	if (curr_11u_status == "0")
	{
		set_disabled('chk_internet_access', true);
		set_disabled('cmb_net_type', true);
		set_disabled('cmb_net_auth_type', true);
		set_disabled('txt_hessid', true);
		set_disabled('txt_comeback_delay', true);
		set_disabled('txt_oui', true);
		set_disabled('cmb_ipv4', true);
		set_disabled('cmb_ipv6', true);
		set_disabled('txt_domain_name', true);
		set_disabled('txt_realm', true);
		set_disabled('cmb_eap', true);
		set_disabled('cmb_auth', true);
		set_disabled('cmb_credential', true);
		set_disabled('cmb_v_gr', true);
		set_disabled('cmb_v_type', true);
		set_disabled('txt_lang', true);
		set_disabled('txt_v_name', true);
		set_disabled('txt_country_code', true);
		set_disabled('txt_net_code', true);
		set_disabled('btn_roaming_add', true);
		set_disabled('btn_roaming_del', true);
		set_disabled('btn_add_country', true);
		set_disabled('btn_del_country', true);
		set_disabled('btn_add_venue', true);
		set_disabled('btn_del_venue', true);
		set_disabled('btn_realm_add',true);
		set_disabled('btn_realm_del',true);
		set_disabled('btn_eap_add',true);
		set_disabled('btn_eap_del',true);
		set_disabled('btn_auth_add',true);
		set_disabled('btn_auth_del',true);
		set_disabled('btn_eap_add_realm',true);
		set_disabled('btn_auth_add_eap',true);
	}

	populate_venue_type(venue_group);

	//populate Roaming Consortium
	if (roaming_arr_len == 0)
	{
		set_visible('oui_table', false);
	}
	else
	{
		for( var i=0; i<roaming_arr_len; i++) {
			populate_roaming(roaming_arr[i]);
		}
	}
	//populate venue name
        if (venue_name_arr_len == 0)
        {
                set_visible('venue_table', false);
        }
        else
        {
                for( var i=0; i<venue_name_arr_len; i++) {
			venue_name_arr[i] = venue_name_arr[i].replace(/\r?\n/g,'\\n');
                        populate_venue_name(venue_name_arr[i]);
                }
        }

	//populate anqp_3gpp_cell_net
        if (cell_net_arr_len == 0)
        {
                set_visible('cell_net_table', false);
        }
        else
        {
                for( var i=0; i< cell_net_arr_len; i++) {
                        populate_3gpp_cell_net(cell_net_arr[i]);
                }
        }

	//Populate Nai_realm
        if (nai_realm_arr_len == 0)
        {
                set_visible('realm_table', false);
        }
        else
        {
                for( var i=0; i<nai_realm_arr_len; i++) {
                        populate_nai_realm(nai_realm_arr[i]);
                }
        }


	set_control_value('cmb_11u_status','<?php echo $curr_11u_status; ?>', 'combox');
	set_control_value('cmb_net_type','<?php echo $curr_net_type; ?>', 'combox');
	set_control_value('cmb_net_auth_type','<?php echo $curr_net_auth_type; ?>', 'combox');
	set_control_value('cmb_ipv4','<?php echo $curr_ipv4; ?>', 'combox');
	set_control_value('cmb_ipv6','<?php echo $curr_ipv6; ?>', 'combox');
	set_control_value('cmb_v_gr','<?php echo $curr_venue_gr; ?>', 'combox');

	set_visible('tr_warning', false);
	set_visible('tr_warning_venue', false);
	set_visible('tr_warning_cell_net', false);
	set_visible('tr_nai_realm_warning', false);
	set_visible('tr_apply_auth', false);
	set_visible('tr_apply_eap', false);

	document.getElementById("eap_display_table").style.display = 'none';
        document.getElementById("eap_table").style.display = 'none';
        document.getElementById("btn_eap_add_realm").style.display = 'none';
        document.getElementById("tr_eap").style.display = 'none';
        document.getElementById("auth_display_table").style.display = 'none';
        document.getElementById("auth_table").style.display = 'none';
        document.getElementById("btn_auth_add_eap").style.display = 'none';
        document.getElementById("tr_auth").style.display = 'none';
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
	<form enctype="multipart/form-data" action="config_11u.php" id="mainform" name="mainform" method="post">
	<input type="hidden" name="action" id="action" value="action" />
	<div class="right">
		<div class="righttop">CONFIG - 802.11u</div>
		<div id="TabbedPanels1" class="TabbedPanels">
			<ul class="TabbedPanelsTabGroup">
				<li class="TabbedPanelsTab" tabindex="0">Basic</li>
				<li class="TabbedPanelsTab" tabindex="0" id="tbc_advanced">Advanced</li>
				<li class="TabbedPanelsTab" tabindex="0" id="tbc_nai_realm">Nai Realm Info</li>
				<li class="TabbedPanelsTab" tabindex="0" id="tbc_venue_name">Venue Info</li>
			</ul>
			<div class="TabbedPanelsContentGroup">
				<div class="TabbedPanelsContent">
					<div class="rightmain">
						<table class="tablemain">
							<tr>
								<td width="40%">802.11u Status:</td>
								<td width="60%">
									 <select name="cmb_11u_status" class="combox" id="cmb_11u_status" onchange="modechange(this)">
										<option value="1">Enable</option>
										<option value="0">Disable</option>
									 </select>
								</td>
							</tr>
							<tr id="tr_internet_access">
								<td>Internet Access:</td>
								<td>
									<input name="chk_internet_access" id="chk_internet_access" type="checkbox"  class="checkbox" <?php if($curr_internet_access==1) echo "checked=\"checked\""?>/>
								</td>
							</tr>
							<tr id="tr_net_type">
								<td>Network Type:</td>
								<td>
									<select name="cmb_net_type" class="combox" id="cmb_net_type">
									<option value="0">Private Network</option>
									<option value="1">Private Network with Guest Access</option>
									<option value="2">Chargeable Public Network</option>
									<option value="3">Free Public Network</option>
									<option value="4">Personel Device Network</option>
									<option value="5">Emergency Service Only Network</option>
									<option value="14">Test or Experimental</option>
									<option value="15">Wildcard</option>
									</select>
								</td>
							</tr>
							<tr id="tr_net_auth_type">
								<td>Network Auth Type:</td>
								<td>
									<select name="cmb_net_auth_type" class="combox" id="cmb_net_auth_type">
									<option value="00">Acceptance of Terms</option>
									<option value="01">Online Enrollment</option>
									<option value="02">HTTP/HTTPS Redirection</option>
									<option value="03">DNS Redirection</option>
									</select>
								</td>
							</tr>
							<tr id="tr_hessid">
			                                        <td>HESSID:</td>
								 <td>
									<input name="txt_hessid" type="text" id="txt_hessid" class="textbox" value="<?php  echo htmlspecialchars($curr_hessid,ENT_QUOTES); ?>"/>
								</td>
							</tr>
							<tr id="tr_gas_comeback_delay">
								<td>Gas Comeback Delay:</td>
								<td>
									<input name="txt_comeback_delay" type="text" id="txt_comeback_delay" class="textbox" value="<?php  echo htmlspecialchars($curr_comeback_delay,ENT_QUOTES); ?>"/>
								</td>
							</tr>
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
						</table>
							<tr id="tr_oui">
								<td>Roaming Consortium:</td>
								<td>
									<input name="txt_oui" type="text" id="txt_oui" class="textbox"/>
								</td>
								<td><button name="btn_roaming_add" id="btn_roaming_add" type="button" onclick="addRoamingRow('oui_table');" class="button" style="width:60px;">ADD</button></td>
								<td><button name="btn_roaming_del" id="btn_roaming_del" type="button" onclick="deleteRoamingRow('oui_table');" class="button" style="width:80px;">REMOVE</button></td>

							<table id="oui_table" width="350px" border="1">
								<td width="20%">option</td>
								<td width="20%">Roaming Consortium</td>
							</table>

							<input name="NumRows" id="NumRows" type="hidden"/>
							<input name="chk_box" id="chk_box" type="hidden"/>
							<input name="add_roaming" id="add_roaming" type="hidden"/>
							<input name="del_roaming" id="del_roaming" type="hidden"/>
							</tr>
						<table class="tablemain">
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
							<tr id="tr_ipv4">
								<td>IPv4:</td>
								<td>
									<select name="cmb_ipv4" class="combox" id="cmb_ipv4">
										<option value="0">Address type not available</option>
										<option value="1">Public IPv4 address available</option>
										<option value="2">Port-restricted IPv4 address available</option>
										<option value="3">Single NATed private IPv4 address available</option>
										<option value="4">Double NATed private IPv4 address available</option>
										<option value="5">Port-restricted IPv4 address and single NATed IPv4 address available</option>
										<option value="6">Port-restricted IPv4 address and double NATed IPv4 address available</option>
										<option value="7">Availability of the address type is not known</option>
									</select>
								</td>
							</tr>
							<tr id="tr_ipv6">
								<td>IPv6:</td>
								<td>
									<select name="cmb_ipv6" class="combox" id="cmb_ipv6">
										<option value="0">Address type not available</option>
										<option value="1">Address type available</option>
										<option value="2">Availability of the address type not known</option>
									</select>
								</td>
							</tr>
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
							<tr id="tr_domain_name">
								<td>Domain Name:</td>
								<td>
									<input name="txt_domain_name" type="text" id="txt_domain_name" class="textbox"/>
								</td>

							</tr>
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
						</table>
							<tr id="tr_country_code">
								<td>Country Code:</td>
								<td>
									<input name="txt_country_code" type="text" id="txt_country_code" class="textbox" style="width:60px;" />
								</td>
								<td>Network Code:</td>
								<td>
									<input name="txt_net_code" type="text" id="txt_net_code" class="textbox" style="width:60px;" />
								</td>
								<td><button name="btn_add_country" id="btn_add_country" type="button" onclick="add_cell_net_Row('cell_net_table');" class="button" style="width:60px;">ADD</button></td>
								<td><button name="btn_del_country" id="btn_del_country" type="button" onclick="delete_cell_net_Row('cell_net_table');" class="button" style="width:80px;">REMOVE</button></td>

								<table id="cell_net_table" width="350px" border="1">
									<td width="20%">option</td>
									<td width="20%">Country code,Network Code</td>
								</table>

								<input name="NumRows_cell_net" id="NumRows_cell_net" type="hidden"/>
								<input name="add_cell_net" id="add_cell_net" type="hidden"/>
								<input name="chk_box_cell_net" id="chk_box_cell_net" type="hidden"/>
								<input name="del_cell_net" id="del_cell_net" type="hidden"/>
							</tr>
						<table class="tablemain">
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
							<tr id="tr_warning_cell_net">
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
						</table>
							<tr id="tr_nai_realm">
								<td>Encoding:</td>
								<td>
									<select name="cmb_encoding" class="combox" id="cmb_encoding" style="width:50px;">
									<option value="0">0</option>
									<option value="1">1</option>
									</select>
								</td>
								<td>Realm:</td>
								<td>
									<input name="txt_realm" type="text" id="txt_realm" class="textbox" />
								</td>
								<td><button name="btn_realm_add" id="btn_realm_add" type="button" onclick="add_nai_realm_Row('realm_table');" class="button" style="width:60px;">ADD</button></td>
								<td><button name="btn_realm_del" id="btn_realm_del" type="button" onclick="delete_nai_realm_Row('realm_table');" class="button" style="width:80px;">REMOVE</button></td>
								<table id="realm_table" width="350px" border="1">
									<td width="10%">option</td>
									<td width="80%">nai realm</td>
								</table>
								 <input name="NumRows_nai_realm" id="NumRows_nai_realm" type="hidden"/>
								<input name="chk_box_nai_realm" id="chk_box_nai_realm" type="hidden"/>
								<input name="add_nai_realm" id="add_nai_realm" type="hidden"/>
								<input name="del_nai_realm" id="del_nai_realm" type="hidden"/>

							</tr>
							<table class="tablemain">
								<tr id="tr_l">
									<td class="divline" colspan="2";></td>
								</tr>

								<table id="eap_display_table">
									<td id="txt_eap_name">EAP METHOD:</td>
									<td>
										<select name="cmb_eap" class="combox" id="cmb_eap" style="width:250px;">
											<option value="13">EAP_TYPE_TLS</option>
											<option value="18">EAP_TYPE_SIM</option>
											<option value="21">EAP_TYPE_TTLS</option>
											<option value="23">EAP_TYPE_AKA</option>
											<option value="25">PEAP</option>
										</select>
									</td>
									<td><button name="btn_eap_add" id="btn_eap_add" type="button" onclick="add_eap_Row('eap_table');" class="button" style="width:60px;">ADD</button></td>
									<td><button name="btn_eap_del" id="btn_eap_del" type="button" onclick="delete_eap_Row('eap_table');" class="button" style="width:80px;">REMOVE</button></td>
									<table id="eap_table" width="350px" border="1">
										<td width="20%">option</td>
										<td width="20%">EAP Method</td>
									</table>
									<td><button name="btn_eap_add_realm" id="btn_eap_add_realm" type="button" onclick="add_eap_row_to_realm();" class="button" style="width:60px;">APPLY</button></td>
								</table>
							</table>
							<table class="tablemain">
								<tr id="tr_apply_eap">
									<td colspan="2"; style="color:red; font-size:smaller;">*To add EAP Method to Realm, click apply button*</td>
								</tr>

								<tr id="tr_eap">
									<td class="divline" colspan="2";></td>
								</tr>

								<table id="auth_display_table">
									<td>Auth Method:</td>
									<td>
										<select name="cmb_auth" class="combox" id="cmb_auth" style="width:200px;" onchange="changeAuth(this)">
											<option value="2">Non-EAP Inner Authentication</option>
											<option value="5">Credential Type</option>
										</select>
									</td>
									<td>Auth Param:</td>
									<td>
										<select name="cmb_auth_param" class="combox" id="cmb_auth_param">
										</select>

									</td>
									<td><button name="btn_auth_add" id="btn_auth_add" type="button" onclick="add_auth_Row('auth_table');" class="button" style="width:60px;">ADD</button></td>
									<td><button name="btn_auth_del" id="btn_auth_del" type="button" onclick="delete_auth_Row('auth_table');" class="button" style="width:80px;">REMOVE</button></td>
									<table id="auth_table" width="450px" border="1">
										 <td width="20%">option</td>
										<td width="20%">value</td>
									</table>
									<td><button name="btn_auth_add_eap" id="btn_auth_add_eap" type="button" onclick="add_auth_row_to_eap();" class="button" style="width:60px;">APPLY</button></td>
								</table>
							</table>



						<table class="tablemain">
							<tr id="tr_apply_auth">
								<td colspan="2"; style="color:red; font-size:smaller;">*To add Auth Method to EAP method, click apply button*</td>
							</tr>
							<tr id="tr_auth">
								<td class="divline" colspan="2";></td>
							</tr>
							<tr id="tr_nai_realm_warning">
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
							<tr id="tr_v_gr">
								<td>Venue Group:</td>
								<td>
									<select name="cmb_v_gr" class="combox" id="cmb_v_gr" onchange="modechange(this)">
										<option value="0">Unspecified</option>
										<option value="1">Assembly</option>
										<option value="2">Business</option>
										<option value="3">Educational</option>
										<option value="4">Factory and Industrial</option>
										<option value="5">Institutional</option>
										<option value="6">Mercantile</option>
										<option value="7">Residential</option>
										<option value="8">Storage</option>
										<option value="9">Utility and Misc</option>
										<option value="10">Vehicular</option>
										<option value="11">Outdoor</option>
									</select>
								</td>
							</tr>
							<tr id="tr_v_type">
								<td>Venue Type:</td>
								<td>
									<select name="cmb_v_type" class="combox" id="cmb_v_type">
									</select>
								</td>
							</tr>
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
						</table>
							<tr id="tr_venue_name">
								<td>Language:</td>
								<td>
									<input name="txt_lang" type="text" id="txt_lang" class="textbox" style="width:60px;"/>
								</td>
								<td>Venue Name:</td>
								<td>
									<textarea name="txt_v_name" id="txt_v_name"></textarea>
								</td>
								<td><button name="btn_add_venue" id="btn_add_venue" type="button" onclick="addVenueNameRow('venue_table');" class="button" style="width:60px;">ADD</button></td>
								<td><button name="btn_del_venue" id="btn_del_venue" type="button" onclick="deleteVenueNameRow('venue_table');" class="button" style="width:80px;">REMOVE</button></td>

							<table id="venue_table" width="350px" border="1">
								<td width="20%">option</td>
								<td width="20%">Language:Venue Name</td>
							</table>
							<input name="NumRows_venue" id="NumRows_venue" type="hidden"/>
							<input name="chk_box_venue" id="chk_box_venue" type="hidden"/>
							<input name="add_venue_name" id="add_venue_name" type="hidden"/>
							<input name="del_venue_name" id="del_venue_name" type="hidden"/>
							</tr>
						<table class="tablemain">
							<tr id="tr_l">
								<td class="divline" colspan="2";></td>
							</tr>
							<tr id="tr_warning_venue">
								<td colspan="2"; style="color:red; font-size:smaller;">*To apply the changes, click save button*</td>
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

