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
function reload()
{
	window.location.href="config_aplist24.php";
}
</script>

<?php
$curr_mode=exec("qweconfig get mode.wlan1");
if ($curr_mode== "0")
{
	confirm("This page is only for the Bridge Mode and Repeator Mode.");
	echo "<script language='javascript'>location.href='status_device.php'</script>";
}
?>

<script type="text/javascript">
function show_item(name)
{
	var item = document.getElementById(name);
	if (item)
		item.style.display = '';
}

function hide_item(name)
{
	var item = document.getElementById(name);
	if (item)
		item.style.display = 'none';
}

function click_ap(ssid,security,protocol,authentication,encryption)
{
	show_item("select_ap_ssid");
	show_item("select_ap_btn");
	show_item("select_ap_line");
	var apssid=document.getElementById("select_ssid");
	apssid.innerHTML="AP:"+ssid;
	if(security=="Yes")
	{
		show_item("select_ap_pwd");
	}
	else
	{
		hide_item("select_ap_pwd");
	}
	var form2=document.forms[1];
	document.forms[1].select_ssid.value=ssid;
	document.forms[1].select_security.value=security;
	document.forms[1].select_protocol.value=protocol;
	document.forms[1].select_authentication.value=authentication;
	document.forms[1].select_encryption.value=encryption;
}

function connect()
{
	document.forms[1].action.value="1";
	document.forms[1].select_pwd.value=document.forms[0].select_ap_passphrase.value;
	document.postform.submit();
}

function rescan()
{
	document.forms[1].action.value="0";
	document.postform.submit();
}
</script>

<?php
$curr_ssid="";
$curr_networkid=0;
$supplicant_file = "/mnt/jffs2/wpa_supplicant.conf";

$arr_protocol = array("WPA","11i","WPAand11i");
$arr_encryption = array("TKIPEncryption","AESEncryption","TKIPandAESEncryption");

function find_enabled_ssid(&$ssid, &$network_id)
{
	$file_path="/mnt/jffs2/wpa_supplicant.conf";
	$fp = fopen($file_path, 'r');
	$done = 0;
	$network_found = 0;
	$ssid_match = 0;
	$ssid = "";
	$network_id = -1;
	$disabled = 0;
	while(!feof($fp))
	{
		$buffer = stream_get_line($fp, 100, "\n");
		$token = trim(strtok($buffer, '='));
		//ignore comments
		if($token && substr($token, 0) == '#') continue;
		while($token)
		{
			if((strcmp($token, "ssid") == 0) && ($network_found == 1))
			{
				$token = trim(strtok('='));
				$ssid = $token;
				$network_id++;
				$ssid_match = 1;
				break;
			}
			if(strcmp($token, "network") == 0)
			{
				$network_found = 1;
				break;
			}
			if((strcmp($token, "}") == 0) && ($network_found == 1))
			{
				if($disabled != 1)
				{
					$done = 1;
				}
				else
				{
					$disabled = 0;
				}
				$network_found = 0;
				$ssid_match = 0;
				break;
			}
			if((strcmp($token, "disabled") == 0) && $ssid_match == 1)
			{
				$disabled = 1;
				break;
			}
			$token = trim(strtok('='));
		}
		if($done == 1) break;
	}
	fclose($fp);
	$ssid = substr($ssid, 1, strlen($ssid) - 2);
	//if($done != 1) //no network block was enabled -enable the last one
	// or if there are later blocks that are not disabled - select the network that is returned
	/*
		error = 0 (no probms)
		error = -1 (no ssid match)
		error = -2 (ssid found but no such param)
	*/
	$error = ($done == 1)? 0: (($ssid_match == 1)? -2: -1);
	return $error;
}

function disable_rest($file_path, $ssid)
{
	$fp = fopen($file_path, 'r');
	$network_found = 0;
	$ssid_match = 0;
	$ret_val = -1; //ssid not found, enabling the last network
	$supp_contents = "";
	$disable_found = 0;
	$network_id = -1;
	while(!feof($fp))
	{
		$buffer = stream_get_line($fp, 100, "\n");
		$token = trim(strtok($buffer, '='));
		//ignore comments
		if($token && substr($token, 0) == '#') continue;
		while($token)
		{
			if((strcmp($token, "ssid") == 0) && ($network_found == 1))
			{
				$token = trim(strtok('='));
				$network_id++;
				if(strcmp("\"$ssid\"", $token) == 0)
					{ $ssid_match = 1; $ret_val = 1; }
				break;
			}
			if(strcmp($token, "network") == 0)
			{
				$network_found = 1;
				break;
			}
			if((strcmp($token, "}") == 0) && ($network_found == 1))
			{
				if($ssid_match == 0 && $disable_found == 0)
					$buffer = "\tdisabled=1\n}";
				$network_found = 0;
				$ssid_match = 0;
				$disable_found = 0;
				break;
			}
			if((strcmp($token, "disabled") == 0))
			{
				if($ssid_match == 1)
				{
					//if the given ssid is disabled, remove the disabled parameter
					$buffer = "";
				}
				$disable_found = 1;
			}
			$token = trim(strtok('='));
		}
		if($buffer != "") $supp_contents .= "$buffer\n";
	}
	fclose($fp);
	file_put_contents($file_path, $supp_contents);
	return $ret_val;
}

if (isset($_POST['action']))
{
	if ($_POST['action']=="0")
	{
		exec("call_qcsapi start_scan wifi0");
		sleep(3);
	}
	else if ($_POST['action']=="1")
	{
		$p_ssid=$_POST['select_ssid'];
		$set_ssid=escape_any_characters($p_ssid);
		$set_security=$_POST['select_security'];
		$set_protocol=$_POST['select_protocol'];
		$set_encryption=$_POST['select_encryption'];
		$set_authentication=$_POST['select_authentication'];
		$p_pwd=$_POST['select_pwd'];
		$set_pwd=escape_any_characters($p_pwd);
		//check if the SSID info exist, if not create
		$tmp=exec("call_qcsapi verify_ssid wifi0 \"$set_ssid\"");
		if(!(strpos($tmp, "QCS API error") === false))
		{
			$tmp=exec("call_qcsapi create_ssid wifi0 \"$set_ssid\"");
		}
		//Check security, if "No", disable the security.
		if ($set_security=="No")
		{
			$tmp=exec("call_qcsapi SSID_set_authentication_mode wifi0 \"$set_ssid\" NONE");
		}
		elseif ($set_security=="Yes")
		{
			//set the protocol
			$tmp=$set_protocol-1;
			exec("call_qcsapi SSID_set_proto wifi0 \"$set_ssid\" $arr_protocol[$tmp]");
			//set the encryption
			$tmp=$set_encryption-1;
			exec("call_qcsapi SSID_set_encryption_modes wifi0 \"$set_ssid\" $arr_encryption[$tmp]");
			if($set_authentication=="1")
			{
				exec("call_qcsapi SSID_set_authentication_mode wifi0 \"$set_ssid\" PSKAuthentication");
			}
			exec("call_qcsapi SSID_set_key_passphrase wifi0 \"$set_ssid\" 0 $set_pwd");
		}
		disable_rest($supplicant_file,$p_ssid);
		exec("wpa_cli reconfigure");
	}
}
$curr_ssid = trim(shell_exec("call_qcsapi get_SSID wifi0"));
?>

<body class="body">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<div style="border:6px solid #9FACB7; width:800px; background-color:#fff;">
	<div class="righttop">ACCESS POINT LIST</div>
	<form name="mainform" method="post" action="config_aplist.php">
	<div class="rightmain">
		Current SSID: <?php echo htmlspecialchars($curr_ssid)?>
		<table class="tablemain">
			<tr>
				<td width="10%" align="center" bgcolor="#96E0E2" ></td>
				<td width="30%" align="center" bgcolor="#96E0E2">SSID</td>
				<td width="30%" align="center" bgcolor="#96E0E2">Mac Address</td>
				<td width="10%" align="center" bgcolor="#96E0E2">Channel</td>
				<td width="10%" align="center" bgcolor="#96E0E2">RSSI</td>
				<td width="10%" align="center" bgcolor="#96E0E2">Security</td>
				</tr>
			<?php
				$count=exec("call_qcsapi get_results_AP_scan wifi0");
				for($i=0;$i<$count;$i++)
				{
					$index=$i+1;
					$res=exec("call_qcsapi get_properties_AP wifi0 $i");
					$lenth=strlen($res);
					$ssid_end=0;
					for($n=$lenth-1; $n>0; $n--)
					{
						$tmp=substr($res,$n,1);
						if($tmp=="\"")
						{
							$ssid_end = $n;
							$ssid=substr($res,1,$ssid_end-1);
							$tmp=substr($res,$ssid_end+2,$lenth-$ssid_end);
							break;
						}
					}
					$token = trim(strtok($tmp, " "));
					$mac=$token;

					$token = trim(strtok(" "));
					$channel=$token;

					$token = trim(strtok(" "));
					$rssi=$token;

					$token = trim(strtok(" "));
					$security=$token;

					$token = trim(strtok(" "));
					$protocol=$token;

					$token = trim(strtok(" "));
					$authentication =$token;

					$token = trim(strtok(" "));
					$encryption  =$token;

					$ssid_token=trim(strtok($ssid, "\""));

					if($security=="0")
					{
						$security="No";
					}
					else
					{
						$security="Yes";
					}
					$new_ssid = addslashes($ssid);
					echo "<tr onclick=\"click_ap('".htmlspecialchars($new_ssid)."','$security','$protocol','$authentication','$encryption')\">\n";
					echo "<td width=\"10%\" align=\"center\" >$index</td>\n";
					echo "<td width=\"30%\" align=\"center\" >".htmlspecialchars($ssid)."</td>\n";
					echo "<td width=\"15%\" align=\"center\" >$mac</td>\n";
					echo "<td width=\"15%\" align=\"center\" >$channel</td>\n";
					echo "<td width=\"15%\" align=\"center\" >$rssi</td>\n";
					echo "<td width=\"15%\" align=\"center\" >$security</td>\n";
					echo "</tr>\n";
				}
			?>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="6";></td>
				</tr>
				<tr id="select_ap_ssid" style="display:none;">
					<td colspan="6" id="select_ssid">
					</td>
				</tr>
				<tr id="select_ap_pwd" style="display:none;">
					<td colspan="6">
						Passphrase: <input type="text" name="select_ap_passphrase" id="select_ap_passphrase" width="142px"/>
					</td>
				</tr>
				<tr id="select_ap_btn" style="display:none;">
					<td colspan="6">
						<button name="btn_assoc" id="btn_assoc" type="button" onclick="connect();"  class="button">Connect</button>
					</td>
				</tr>
				<tr id="select_ap_line" style="display:none;">
					<td class="divline" style="background:url(/images/divline.png);" colspan="6";></td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_rescan" id="btn_rescan" type="button" onclick="rescan();"  class="button">Rescan</button>
			</div>
		</div>
		</form>
	</div>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> <br />
	<div>&copy; 2013 Quantenna Communications, Inc. All Rights Reserved.</div>
</div>

<form enctype="multipart/form-data" action="config_aplist.php" id="postform" name="postform" method="post">
	<input type="hidden" name="action" />
	<input type="hidden" name="select_ssid" />
	<input type="hidden" name="select_security" />
	<input type="hidden" name="select_protocol" />
	<input type="hidden" name="select_authentication" />
	<input type="hidden" name="select_encryption" />
	<input type="hidden" name="select_pwd" />
</form>
</body>
</html>

