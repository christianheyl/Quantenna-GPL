<?php
function confirm($msg)
{
	echo "<script langauge=\"javascript\">alert(\"$msg\");</script>";
}

function convert_mode($mode_24)
{
	if($mode_24=="rp")
	{$res = "Station";}
	else if($mode_24=="ap")
	{$res = "Access point";}
	return $res;
}
//Make sure the special character won`t be trimed.
function escape_any_characters($str)
{
	$escape_str = "";
	$len = strlen($str);
	for($i = 0; $i < $len; $i+=1)
	{
		switch(ord(substr($str, $i)))
		{
			case 34: //decimal value of ASCII "
			case 96: //decimal value of ASCII `
			case 92: //decimal value of ASCII \
			case 36: //decimal value of ASCII $
				$escape_str = $escape_str."\\";
				break;
		}
		$escape_str = $escape_str.$str[$i];
	}
	return $escape_str;
}
//=========Get ifconfig part============================
function read_ipaddr()
{
	$ipaddr = exec("ifconfig eth1_0 | grep \"inet addr\" | awk '{print $2}'");
	if($ipaddr == "")
		$ipaddr = exec("ifconfig br0 | grep \"inet addr\" | awk '{print $2}'");

	$ipaddr = substr($ipaddr, 5);
	return $ipaddr;
}

function read_netmask()
{
	$netmask = exec("ifconfig eth1_0 | awk '/Mask:/ {print substr($4, 6)}'");
	if($netmask == "")
		$netmask = exec("ifconfig br0 | awk '/Mask:/ {print substr($4, 6)}'");

	return $netmask;
}

function read_br_ip()
{
	$ipaddr = exec("ifconfig br0 | grep \"inet addr\" | awk '{print $2}'");

	if($ipaddr != "")
		$ipaddr = substr($ipaddr, 5);
	return $ipaddr;
}

//======================================================

//=========Get mac part=================================
function read_emac_addr()
{
	$emac_addr = exec("call_qcsapi get_mac_addr eth1_0");
	if((strpos($info, "API error") === FALSE))
	{
		$res=$emac_addr;
	} else {
		$res="";
	}
	return $res;
}

function read_pciemac_addr()
{
	$pciemac_addr = exec("call_qcsapi get_mac_addr pcie0");
	if((strpos($info, "API error") === FALSE))
	{
		$res=$pciemac_addr;
	} else {
		$res="";
	}
	return $res;
}

function read_wmac_addr()
{
	$wmac_addr = exec("call_qcsapi get_mac_addr wifi0");
	if((strpos($info, "API error") === FALSE))
	{
		$res=$wmac_addr;
	} else {
		$res="";
	}
	return $res;
}
//======================================================
function read_wireless_conf($param)
{
	$content=trim(shell_exec("cat /mnt/jffs2/wireless_conf.txt"));
	$sections=explode("&",$content);
	foreach($sections as $section)
	{
		$res=explode("=",$section);
		if ($res[0] == $param)
		{
			return $res[1];
		}
	}
	return "";
}

function write_wireless_conf($param,$curr_value,$new_value)
{
	$content=trim(shell_exec("cat /mnt/jffs2/wireless_conf.txt"));
	//Add new parameter, if it is not found in the file
	$found = read_wireless_conf($param);

	if ($found == "")
	{
		$new_str = "&".$param."=".$new_value;
		$content=$content.$new_str;
	}
	//Update parameter
	else
	{
		$curr_str = $param."=".$curr_value;
		$new_str = $param."=".$new_value;
		$content = str_replace($curr_str, $new_str, $content);
	}
	file_put_contents("/mnt/jffs2/wireless_conf.txt", $content);
}

function get_privilege($required_privilege)
{
	$key_cookie=$_COOKIE["p"];
	$super_md5="1b3231655cebb7a1f783eddf27d254ca";
	$admin_md5="21232f297a57a5a743894a0e4a801fc3";
	$user_md5="ee11cbb19052e40b07aac0ca060c23ee";
	$privilege="";
	if ($key_cookie=="")
	{
		echo "<script>location.href='login.php';</script>";
	}
	else if ($key_cookie==$super_md5) {
		$privilege=0;
	}
	else if ($key_cookie==$admin_md5) {
		$privilege=1;
	}
	else if ($key_cookie==$user_md5) {
		$privilege=2;
	}
	if ($privilege>$required_privilege)
	{
		confirm("Insufficient privilege");
		echo "<script>location.href='status_device.php';</script>";
	}
	return $privilege;
}

//================== AP proto =====================
function get_ap_proto()
{
        $device=exec("call_qcsapi get_primary_interface");
        if(!(strpos($device, "not found") === false))
        {$device="wifi0";}
        $tmp=exec("call_qcsapi get_ssid $device");
        if(!(strpos($tmp, "QCS API error") === false))
        {return "NONE";}
        $beacon=exec("call_qcsapi get_beacon $device");
        $encryption=exec("call_qcsapi get_WPA_encryption_modes $device");
        $authentication=exec("call_qcsapi get_WPA_authentication_mode $device");
        if ($beacon=="Basic")
                return "NONE";
        else if ($authentication=="EAPAuthentication" && $encryption=="TKIPandAESEncryption")
                return "WPAand11i-EAP";
        else if ($authentication=="EAPAuthentication" && $encryption=="AESEncryption")
                return "WPA2-EAP";
        else if ($beacon=="11i" && $encryption=="AESEncryption" && $authentication=="PSKAuthentication")
                return "11i";
        else if ($beacon=="11i" && $encryption=="AESEncryption" && $authentication=="SHA256PSKAuthentication")
                return "11i_pmf";
        else if ($beacon=="WPAand11i" && $encryption=="TKIPandAESEncryption")
                return "WPAand11i";
}

function set_ap_proto($proto)
{
	$device=exec("call_qcsapi get_primary_interface");
	if(!(strpos($device, "not found") === false))
	{$device="wifi0";}
	if ($proto=="NONE")
	{
		exec("call_qcsapi set_beacon $device Basic");
		exec("call_qcsapi set_WPA_authentication_mode $device PSKAuthentication");
		exec("call_qcsapi set_WPA_encryption_modes $device AESEncryption");
	}
	else if ($proto=="11i")
	{
		exec("call_qcsapi set_beacon $device 11i");
		exec("call_qcsapi set_WPA_authentication_mode $device PSKAuthentication");
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
		exec("call_qcsapi set_WPA_authentication_mode $device PSKAuthentication");
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
//====================================================

?>
