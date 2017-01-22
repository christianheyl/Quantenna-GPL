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
$privilege = get_privilege(1);

?>


<script type="text/javascript">
var privilege="<?php echo $privilege; ?>";
</script>

<?php
$curr_mode=exec("call_qcsapi get_mode wifi0");
if($curr_mode == "Station")
{
	echo "<head>";
	echo "This page is available only when the device is configured as an Access Point";
	echo "<meta HTTP-EQUIV=\"REFRESH\" content=\"3; url=status_device.php\">";
	echo "</head>";
	echo "</html>";
	exit();
}

$flag=0;
$res="";
function get_filter()
{
	$res="";
	$mac_filter = trim(exec("call_qcsapi get_macaddr_filter wifi0"));
	if ($mac_filter==0)
	{$res="NONE";}
	else if ($mac_filter==1)
	{$res="Authorize if not denied";}
	else if ($mac_filter==2)
	{$res="Deny if not authorized";}
	return $res;
}

if(isset($_POST['action']))
{
	if (!(isset($_POST['csrf_token']) && $_POST['csrf_token'] === get_session_token())) {
		header('Location: login.php');
		exit();
	}

	$action = $_POST['action'];
	if($action == 0) //Action0: Save
	{
		if(isset($_POST['macaddr']) && isset($_POST['settype']))
		{
			$macaddr = $_POST['macaddr'];
			$deny_or_auth = $_POST['settype'];
			$tmp="";
			if ($macaddr != "NONE")
			{
				if($deny_or_auth == 0)
				{
					$tmp="call_qcsapi authorize_macaddr wifi0 ".$macaddr;
				}
				else
				{
					$tmp="call_qcsapi deny_macaddr wifi0 ".$macaddr;
				}
				exec($tmp);
			}
		}
		if(isset($_POST['cmb_macaddrfilter']))
		{
			$mac_filter = trim(exec("call_qcsapi get_macaddr_filter wifi0"));
			$new_filter=$_POST['cmb_macaddrfilter'];
			if ($mac_filter != $new_filter)
			{
				exec("call_qcsapi set_macaddr_filter wifi0 $new_filter");
			}
		}
	}
	else if($action == 1) //Action2: Verify MAC address
	{
		$flag=1;
		if(isset($_POST['macaddr']))
		{
			$macaddr = $_POST['macaddr'];
			$result = exec("call_qcsapi is_macaddr_authorized wifi0 $macaddr");
			if(is_qcsapi_error($result))
			{
				$arraylist=split(':',$result);
				$res=$arraylist[1];
			}
			else if($result == 0)
			{$res="MAC address $macaddr is blocked from associating";}
			else if($result == 1)
			{$res="MAC address $macaddr can associate";}
		}
	}
}

$mac_filter = trim(exec("call_qcsapi get_macaddr_filter wifi0"));
if($mac_filter == 1)
{
	$result = exec("call_qcsapi get_denied_macaddr wifi0 400");
}
else if($mac_filter == 2)
{
	$result = exec("call_qcsapi get_authorized_macaddr wifi0 400");
}
else
{
	$result = "";
}

$mac_count=0;
if($result != "")
{
	$tmp = explode(',', $result);
	$mac_count = count($tmp);
}
?>
<script type="text/javascript">

function isHex(entry)
{
	validChar='0123456789ABCDEF';	// legal chars
	strlen=entry.length;		// test string length
	if(strlen != "12")
	{
		alert("MAC Address needs to be 12 characters long");
		return false;
	}
	entry=entry.toUpperCase();	// case insensitive
	// Now scan for illegal characters
	for(idx=0;idx<strlen;idx++)
	{
		if(validChar.indexOf(entry.charAt(idx)) < 0)
		{
			alert("All characters must be hex characters (0-9 and a-f or A-F)!");
			return false;
		}
	} // end scanning
	return true;
}

function CheckMac(address)
{
	var reg_test=/^([0-9a-fA-F]{2})(([//\s:-][0-9a-fA-F]{2}){5})$/;
	var tmp_test_mac=address.substring(0,2);
	var tmp_output=parseInt(tmp_test_mac,16).toString(2);
	String.prototype.Right = function(i) {return this.slice(this.length - i,this.length);};
	tmp_output = tmp_output.Right(1);
	if(!reg_test.test(address))
	{
		return false;
	}
	else
	{
		if(1 == tmp_output)
		return false;
		if("00:00:00:00:00:00"==address)
		return false;
		return true;
	}
}

function AddMacTxt()
{
	var txt=document.getElementById("txt_macaddr");
	var len= txt.value.length;
	//alert("change"+len);
	if ((len+1)%3==0)
	{
		if (len != 17)
		{
			txt.value=txt.value+":";
		}
	}
}

function validate(action_name)
{
	var tmp = document.getElementById("action");
	tmp.value = action_name;
	var txt=document.getElementById("txt_macaddr");
	var macaddr=document.getElementById("macaddr");
	var maccount='<?php echo $mac_count; ?>';

	if(action_name==2)//Refresh Button
	{
		window.location.href="list_mac_address.php";
	}
	else//Set 0 & Verify Button 1
	{
		if(txt.value != "")
		{
			if (maccount>=20)
			{
				alert("The maximum of the MAC addresses added into the list is 20.");
				return false;
			}
			if (CheckMac(txt.value)==true)
			{
				macaddr.value=txt.value;
			}
			else
			{
				alert("Invalid MAC address");
				txt.focus();
				return false;
			}
		}
		else if (txt.value == "")
		{
			if(action_name==0)
			{
				macaddr.value = "NONE";
			}
			else if (action_name==1)
			{
				alert("MAC Address needs to be 12 hex characters long.");
				txt.focus();
				return false;
			}
		}
		document.mainform.submit();
	}
}
</script>
<body class="body" onload="init_menu();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="list_mac_address.php" id="mainform" name="mainform" method="post">
<input id="action" name="action" type="hidden" >
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php echo $curr_mode;?>',privilege);
		</script>
	</div>
	<div class="right">
	<div class="righttop">MAC ADDRESS LIST</div>
		<div class="rightmain">
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%">MAC Address Filtering:</td>
					<td width="70%">
						<select name="cmb_macaddrfilter" class="combox" id="cmb_macaddrfilter" style="width:171px;">
							<option <?php if($mac_filter == 0) echo "selected=\"selected\""?> value="0">None</option>
							<option <?php if($mac_filter == 1) echo "selected=\"selected\""?> value="1">Authorize if not denied</option>
							<option <?php if($mac_filter == 2) echo "selected=\"selected\""?> value="2">Deny if not authorized</option>
						</select>
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>

				<tr>
					<td align="center">MAC Address:</td>
					<td><input name="txt_macaddr" type="text" id="txt_macaddr" class="textbox" style="width:167px;" maxlength=17 onkeyup="AddMacTxt();"/>
					<input id="macaddr" name="macaddr" type="hidden" value="">
					<button name="btn_verify" id="btn_verify" type="button" onclick="validate(1);"  class="button">Verify</button>
					</td>
				</tr>
				<tr <?php if($flag=0) echo "style=\"display:none\""?>>
					<td></td>
					<td>
						<div  style="text-align:left; font:16px Calibri, Candara, corbel, "Franklin Gothic Book";"><b><?php echo $res;?></b></div>
					</td>
				</tr>
				<tr>
					<td></td>
					<td>
						<select name="settype" class="combox" id="settype" style="width:171px;">
							<option selected="selected" value="0">Authorize</option>
							<option value="1">Deny</option>
						</select>
					</td>
				</tr>
				<tr>
					<td></td>
					<td>
						<button name="btn_save" id="btn_save" type="button" onclick="validate(0);"  class="button">Save</button>
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td colspan="2";>
					<?php
					$mac_filter = trim(exec("call_qcsapi get_macaddr_filter wifi0"));
					if ($mac_filter==1)
					{echo "<a style=\"font-weight:bold\">Denied MAC Address List:</a>";}
					else if ($mac_filter==2)
					{echo "<a style=\"font-weight:bold\">Authorized MAC Address List:</a>";}
					?>
					</td>
				</tr>
				<tr><td colspan="2";>
				<?php
				if($result != "")
				{
					$tmp = explode(',', $result);
					$len = count($tmp);
					foreach ($tmp as $i)
					{
						echo "</td></tr><tr><td colspan=\"2\";>$i";
					}
				}
				else
				{
					echo "No results";
				}
				?>
				</td></tr>
			</table>
			<div class="rightbottom">
				<button name="btn_refresh" id="btn_refresh" type="button" onclick="validate(2);"  class="button">Refresh</button>
			</div>
		</div>
	</div>
</div>
<input type="hidden" name="csrf_token" value="<?php echo get_session_token(); ?>" />
</form>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> | <a href="help/h_mac_list.php">Help</a><br />
	<div><?php echo $str_copy ?></div>
</div>
</body>
</html>

