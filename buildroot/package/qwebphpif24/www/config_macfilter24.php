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
$curr_id=0;
$curr_interface="ap";
$curr_filter="";
$curr_maclist="";
$curr_maccount=0;

if($curr_mode != "0")
{
	echo "<head>";
	echo "This page is available only when the device is configured as an Access Point";
	echo "<meta HTTP-EQUIV=\"REFRESH\" content=\"3; url=status_device.php\">";
	echo "</head>";
	echo "</html>";
	exit();
}

function get_value()
{
	global $curr_interface,$curr_filter,$curr_maclist,$curr_maccount;
	$curr_filter=exec("qweconfig get aclmode");
	if ( $curr_filter == "0" ) #none
	{
		$curr_maclist="";
	}
	else if ( $curr_filter == "1" ) #white list
	{
		$curr_maclist=exec("qweconfig list_allow_mac");
	}
	else if ( $curr_filter == "2" ) #black
	{
		$curr_maclist=exec("qweconfig list_deny_mac");
	}
	//Get the current configured MAC adddresses number
	if($curr_maclist != "")
	{
		$tmp = explode(',', $curr_maclist);
		$curr_maccount = count($tmp);
	}
}

function set_value()
{
	global $curr_interface,$curr_filter,$curr_maclist,$curr_maccount;
	$action = $_POST['action'];
	$interface = $_POST['interface'];
	$mac=$_POST['mac'];
	$change_flag=0;

	if($action == "save")
	{
		$old_filter = $_POST['old_filter'];
		$filter = $_POST['filter'];
		if($old_filter != $filter)
		{
			$change_flag=$change_flag+1;
			exec("qweconfig set aclmode $filter");
		}
	}
	else if($action == "add")
	{
		$change_flag=$change_flag+1;
		if ($curr_filter == "1" ) //white
		{
			exec("qweconfig allow_mac $mac");
		}
		else if ($curr_filter == "2") //black
		{
			exec("qweconfig deny_mac $mac");
		}
	}
	else if($action == "remove")
	{
		$change_flag=$change_flag+1;
		if ($curr_filter == "1" ) //white
		{
			exec("qweconfig deny_mac $mac");
		}
		else if ($curr_filter == "2") //black
		{
			exec("qweconfig allow_mac $mac");
		}
	}
	if ($change_flag>0)
	{
		exec("start-stop-daemon -S -b -x /bin/qweaction -- wlan1 commit");
	}
}

get_value();
if(isset($_POST['action']))
{
	set_value();
	get_value();
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

function reload_page()
{
	window.location.href=document.location.href;
}

function validate(action_name)
{
	var txt=document.getElementById("txt_macaddr");
	document.config_macfilter.interface.value="<?php echo $curr_interface;?>";
	if (action_name == 0)//Save Click
	{
		document.config_macfilter.action.value="save";
		document.config_macfilter.old_filter.value="<?php echo $curr_filter;?>";
		document.config_macfilter.filter.value=document.mainform.cmb_macaddrfilter.value;
	}
	else if (action_name == 1)//Add
	{
		if (CheckMac(txt.value)==false)
		{
			alert("Invalid MAC address");
			txt.focus();
			return false;
		}
		document.config_macfilter.action.value="add";
		document.config_macfilter.mac.value=document.mainform.txt_macaddr.value;
	}
	else if (action_name == 2)//Remove
	{
		if (CheckMac(txt.value)==false)
		{
			alert("Invalid MAC address");
			txt.focus();
			return false;
		}
		document.config_macfilter.action.value="remove";
		document.config_macfilter.mac.value=document.mainform.txt_macaddr.value;
	}
	document.config_macfilter.submit();
}

</script>
<body class="body" onload="init_menu();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="config_macfilter24.php" id="mainform" name="mainform" method="post">
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php $tmp=exec("call_qcsapi get_mode wifi0"); echo $tmp;?>','<?php $tmp=exec("qweconfig get mode.wlan1"); echo $tmp;?>',privilege);
		</script>
	</div>
	<div class="right">
	<div class="righttop">2.4G WI-FI - MAC FILTER</div>
		<div class="rightmain">
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%">MAC Address Filtering:</td>
					<td width="70%">
						<select name="cmb_macaddrfilter" class="combox" id="cmb_macaddrfilter" style="width:180px;">
							<option <?php if($curr_filter == "0") echo "selected=\"selected\""?> value="0">None</option>
							<option <?php if($curr_filter == "2") echo "selected=\"selected\""?> value="2">Black List Mode</option>
							<option <?php if($curr_filter == "1") echo "selected=\"selected\""?> value="1">White List Mode</option>
						</select>
                        <button name="btn_save" id="btn_save" type="button" onclick="validate(0);"  class="button">Save</button>

					</td>
				</tr>
				<tr>
					<td align="center">MAC Address:</td>
					<td><input name="txt_macaddr" type="text" id="txt_macaddr" class="textbox" style="width:176px;" maxlength=17 onkeyup="AddMacTxt();"/>
					<input id="macaddr" name="macaddr" type="hidden" value="">
                    <button name="btn_add" id="btn_add" type="button" onclick="validate(1);" class="button">Add</button>
                    <button name="btn_remove" id="btn_remove" type="button" onclick="validate(2);" class="button">Remove</button>
					</td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
				<tr>
					<td colspan="2";>
					<?php
					if ($curr_filter=="2")
					{echo "<a style=\"font-weight:bold\">Black List:</a>";}
					else if ($curr_filter=="1")
					{echo "<a style=\"font-weight:bold\">White List:</a>";}
					?>
					</td>
				</tr>
				<tr>
					<td colspan="2";>
					<?php
					if($curr_maclist != "")
					{
						$tmp = explode(',', $curr_maclist);
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
					</td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_refresh" id="btn_refresh" type="button" onclick="reload_page();"  class="button">Refresh</button>
			</div>
		</div>
	</div>
</div>
</form>

<form name="config_macfilter" method="POST" action="config_macfilter24.php">
	<input type="hidden" name="action" />
	<input type="hidden" name="interface" />
	<input type="hidden" name="old_filter" />
	<input type="hidden" name="filter" />
	<input type="hidden" name="mac" />
</form>

<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> | <a href="help/h_mac_list.php">Help</a><br />
	<div>&copy; 2013 Quantenna Communications, Inc. All Rights Reserved.</div>
</div>
</body>
</html>

