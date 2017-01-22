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
if($curr_mode!="0")
{
	echo "<script langauge=\"javascript\">alert(\"Only support in the Access Point mode.\");</script>";
	echo "<script language='javascript'>location.href='status_device.php'</script>";
	return;
}

$chk=0;
$ssid="";
$proto="open";
$psk="";
$br=0;
$passed_id=0;
if (isset($_GET['id']))
{
	$passed_id=substr($_GET['id'],0,1);
	switch ($passed_id)
	{
		case "0":
			$interface_id="vap0";
			break;
		case "1":
			$interface_id="vap1";
			break;
		case "2":
			$interface_id="vap2";
			break;
		case "3":
			$interface_id="vap3";
			break;
		default:
			$interface_id="vap0";
			break;
	}
}
else
{
	$interface_id="vap0";
}

//=================Load Value======================
function getValue()
{
	global $chk,$ssid,$proto,$psk,$br,$interface_id;
	
	$chk=exec("qweconfig get enable.$interface_id.wlan1");
	//if ($chk=="1")
	//{
		$ssid=exec("qweconfig get ssid.$interface_id.wlan1");
		$proto=exec("qweconfig get encryption.$interface_id.wlan1");
		$psk=exec("qweconfig get passphrase.$interface_id.wlan1");
		$br=exec("qweconfig get broadcastssid.$interface_id.wlan1");
	//}
}
//=====================================================
//========================Save Value===================
function setValue()
{
	global $chk,$ssid,$proto,$psk,$br,$interface_id;
	if ($_POST['action'] == "1")
	{
		$change_flag=0;

		$new_chk = $_POST['chk_bss'];
		$new_ssid = $_POST['txt_ssid'];
		$new_proto = $_POST['cmb_proto'];
		$new_br = $_POST['chk_br'];
		$new_psk = $_POST['txt_psk'];
		if ($new_chk=="on")
		{$new_chk="1";}
		else
		{$new_chk="0";}
		if ($new_br=="on")
		{$new_br="1";}
		else
		{$new_br="0";}
		//Enable or disable mbss
		if ($new_chk != $chk)
		{
			$change_flag++;
			if ($new_chk == "1")
			{
				exec("qweconfig set enable.$interface_id.wlan1 1");
				if($new_ssid!=$ssid)
				{
					exec("qweconfig set ssid.$interface_id.wlan1 $new_ssid");
				}
				if($new_proto!=$proto)
				{
					exec("qweconfig set encryption.$interface_id.wlan1 $new_proto");
				}
				if($new_psk!=$psk)
				{
					exec("qweconfig set passphrase.$interface_id.wlan1 $new_psk");
				}
				if($new_br!=$br)
				{
					exec("qweconfig set broadcastssid.$interface_id.wlan1 $new_br");
				}
			}
			else
			{
				exec("qweconfig set enable.$interface_id.wlan1 0");
			}
		}
		else//Only change configuration
		{
			if($new_ssid!=$ssid)
			{
				$change_flag++;
				exec("qweconfig set ssid.$interface_id.wlan1 $new_ssid");
			}
			if($new_proto!=$proto)
			{
				$change_flag++;
				exec("qweconfig set encryption.$interface_id.wlan1 $new_proto");
			}
			if($new_psk!=$psk)
			{
				$change_flag++;
				exec("qweconfig set passphrase.$interface_id.wlan1 $new_psk");
			}
				$change_flag++;
			if($new_br!=$br)
			{
				$change_flag++;
				exec("qweconfig set broadcastssid.$interface_id.wlan1 $new_br");
			}
		}
		if ($change_flag>0)
		{
			exec("start-stop-daemon -S -b -x /bin/qweaction -- wlan1 commit");
		}
	}
}

//=====================================================
getValue();

if(isset($_POST['action']))
{
	setValue();
	getValue();
}
?>

<script type="text/javascript">

nonascii = /[^\x20-\x7E]/;
nonhex = /[^A-Fa-f0-9]/g;

function reload()
{
	var cmb_if = document.getElementById("cmb_interface");
	window.location.href="config_mbss24.php?id="+cmb_if.selectedIndex;

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
	//Validate PSK\
	if (document.mainform.cmb_proto.selectedIndex > 0)
	{
		if (!validate_psk())
			return false;
	}
	document.mainform.action="config_mbss24.php?id="+"<?php echo $passed_id;?>";
	document.mainform.submit();
}

function modechange(obj)
{
	if (obj.name == "cmb_proto")
	{
		if (document.mainform.cmb_proto.selectedIndex > 0)
		{
			set_visible('tr_passphrase', true);
		}
		else
		{
			set_visible('tr_passphrase', false);
		}
	}
	if (obj.name == "chk_bss")
	{
		if (document.mainform.chk_bss.checked)
		{
			set_disabled('txt_ssid',false);
			set_disabled('cmb_proto',false);
			set_disabled('txt_psk',false);
			set_disabled('chk_br',false);
		}
		else
		{
			set_disabled('txt_ssid',true);
			set_disabled('cmb_proto',true);
			set_disabled('txt_psk',true);
			set_disabled('chk_br',true);
		}
	}
}

function onload_event()
{
	init_menu();
	var chk = "<?php echo $chk; ?>";
	var proto="<?php echo $proto; ?>";

	set_control_value('cmb_interface', '<?php echo $interface_id; ?>', 'combox');
	set_control_value('chk_bss', chk, 'checkbox');
	set_control_value('txt_ssid', '<?php echo $ssid; ?>', 'text');
	set_control_value('cmb_proto', proto, 'combox');
	set_control_value('txt_psk', '<?php echo $psk; ?>', 'text');
	set_control_value('chk_br', '<?php echo $br; ?>', 'checkbox');

	if (proto == "open")
	{
		set_visible('tr_passphrase', false);
	}
	if (chk == "0")
	{
		set_disabled('txt_ssid',true);
		set_disabled('cmb_proto',true);
		set_disabled('txt_psk',true);
		set_disabled('chk_br',true);
	}
}

</script>

<body class="body" onload="onload_event();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" id="mainform" name="mainform" method="post">
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php $tmp=exec("call_qcsapi get_mode wifi0"); echo $tmp;?>','<?php $tmp=exec("qweconfig get mode.wlan1"); echo $tmp;?>',privilege);
		</script>
	</div>
	<div class="right">
		<div class="righttop">
			<p>2.4G WI-FI - MBSS</p>
		</div>
		<div class="rightmain">
			<table class="tablemain">
				 <tr id="tr_if">
					<td width="40%">Wifi Interface:</td>
					<td width="60%">
						<select name="cmb_interface" id="cmb_interface" class="combox" onchange="reload();">
							<option value="vap0">vap0</option>
							<option value="vap1">vap1</option>
							<option value="vap2">vap2</option>
							<option value="vap3">vap3</option>
						</select>
					</td>
				</tr>
				<tr id="tr_l">
					<td class="divline" colspan="2";></td>
				</tr>
				<tr>
					<td>Enable:</td>
                    <td>
			<input name="chk_bss" id="chk_bss" type="checkbox"  class="checkbox" onchange="modechange(this);"/>
                    </td>
				</tr>
				<tr>
					<td>SSID:</td>
                    <td>
			<input name="txt_ssid" type="text" id="txt_ssid" class="textbox"/>
                    </td>
				</tr>
				<tr>
					<td>Broadcast:</td>
                    <td>
			<input name="chk_br" type="checkbox"  id="chk_br" class="checkbox"/>
                    </td>
				</tr>
				<tr>
					<td>Encryption:</br></td>
					<td>
						<select name="cmb_proto" class="combox" id="cmb_proto" onchange="modechange(this)">
                            <option value="open"> NONE-OPEN </option>
                            <option value="wpa_tkip"> WPA-TKIP </option>
                            <option value="wpa_aes"> WPA-AES </option>
                            <option value="wpa2_tkip"> WPA2-TKIP </option>
                            <option value="wpa2_aes"> WPA2-AES </option>
                            <option value="mixed"> MIXED-MODE </option>
						</select>
					</td>
				</tr>
				<tr id="tr_passphrase">
					<td>Passphrase:</br></td>
					<td>
						<input name="txt_psk" type="text" id="txt_psk" class="textbox"/>
					</td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_save" id="btn_save" type="button" onclick="validate();"  class="button">Save</button>
				<button name="btn_cancel" id="btn_cancel" type="button" onclick="reload();"  class="button">Cancel</button>
			</div>
			<input id="action" name="action" type="hidden" value="1">
		</div>
	</div>
</div>
</form>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> | <a href="help/h_mbss.php">Help</a><br />
	<div>&copy; 2013 Quantenna Communications, Inc. All Rights Reserved.</div>
</div>

</body>
</html>
