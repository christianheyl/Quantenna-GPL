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
$curr_mode=exec("qweconfig get mode.wlan1");//Valid $VALUE are 0, 1 and 2 , 0 means AP, 1 means Station, 2 means Repeater
$curr_band="";
$curr_ssid="";
$curr_channel="";
$curr_channel_list="1,2,3,4,5,6,7,8,9,10,11";
$curr_proto="open";
$curr_psk="";
$curr_broadcast="";
$curr_wmm="0";
$curr_bintval=0;
$curr_dtim_period=0;
$curr_short_gi=0;
$curr_bw="";
$curr_coexist="";
$curr_frag_threshold="0";
$curr_rts_threshold="0";


function get_value()
{
	global $curr_mode,$curr_ssid,$curr_channel,$curr_channel_list,$curr_proto,$curr_psk,$curr_band,$curr_broadcast,$curr_wmm,$curr_bintval,$curr_dtim_period,$curr_short_gi,$curr_bw,$curr_coexist,$curr_frag_threshold,$curr_rts_threshold;
	//==========================================================================================================
	$curr_mode=exec("qweconfig get mode.wlan1");
	if ($curr_mode == "0")//AP
	{
		//Get SSID
		$curr_ssid=exec("qweconfig get ssid.wlan1");
		//Get Configed Channel
		$curr_channel=exec("qweconfig get channel.wlan1");
		//Get Current Proto
		$curr_proto = exec("qweconfig get encryption.wlan1");
		//Get Current PSK
		$curr_psk = exec("qweconfig get passphrase.wlan1");
		//Get Band
		$curr_band=exec("qweconfig get band.wlan1");
		//Get Current Bandwidth
		$curr_bw = exec("qweconfig get bandwidth.wlan1");
		//Get Broadcast SSID
		$curr_broadcast = exec("qweconfig get broadcastssid.wlan1");
		//Get WMM
		$curr_wmm = exec("qweconfig get wmm.wlan1");
		//Get Beacon Interval
		$curr_bintval = exec("qweconfig get bcnint.wlan1");
		//Get DTIM Period
		$curr_dtim_period = exec("qweconfig get dtimperiod.wlan1");
		//Get Short GI
		$curr_short_gi = exec("qweconfig get shortgi.wlan1");
		//Get 20/40 Coexist
		$curr_coexist = exec("qweconfig get coexist.wlan1");
		//Get Fragmentation threshold
		$curr_frag_threshold = exec("qweconfig get fragthres.wlan1");
		//Get RTS/CTS  threshold
		$curr_rts_threshold = exec("qweconfig get rtsthres.wlan1");
	}
	elseif ($curr_mode == "1" || $curr_mode =="2")
	{
		//Get current SSID
		$curr_ssid=trim(exec("qweconfig get ssid.sta.wlan1"));
		//Get Current Proto
		$curr_proto = exec("qweconfig get encryption.sta.wlan1");
		//Get Current PSK
		$curr_psk = exec("qweconfig get passphrase.sta.wlan1");
	}
}

function set_value()
{
	global $curr_mode,$curr_ssid,$curr_channel,$curr_channel_list,$curr_proto,$curr_psk,$curr_band,$curr_broadcast,$curr_wmm,$curr_bintval,$curr_dtim_period,$curr_short_gi,$curr_bw,$curr_coexist,$curr_frag_threshold,$curr_rts_threshold;
	//Save
	if ($_POST['action'] == 0)
	{
		$new_mode=$_POST['cmb_devicemode'];
		if ($new_mode != $curr_mode)
		{
			exec("qweconfig set mode.wlan1 $new_mode");
			exec("start-stop-daemon -S -b -x /bin/qweaction -- wlan1 commit");
		}
		else
		{
			$new_ssid=$_POST['txt_essid'];
			$new_proto=$_POST['cmb_encryption'];
			$new_psk=$_POST['txt_passphrase'];
			$change_flag=0;
			if ($curr_mode == "0")//AP
			{
				$new_channel=$_POST['cmb_channel'];
				$new_band=$_POST['cmb_band'];
				$new_bw=$_POST['cmb_bandwidth'];
				$new_broadcast=$_POST['chk_broadcast'];
				$new_broadcast=($new_broadcast == "1")?"1":"0";

				$new_bintval=$_POST['txt_beaconinterval'];
				$new_dtim_period=$_POST['txt_dtimperiod'];

				$new_short_gi=$_POST['chb_shortgi'];
				$new_short_gi=($new_short_gi == "1")?"1":"0";

				$new_coexist=$_POST['chb_coexist'];
				$new_coexist=($new_coexist == "1")?"1":"0";

				$new_frag_threshold=$_POST['txt_frag_threshold'];
				$new_rts_threshold=$_POST['txt_rts_threshold'];
				//Set SSID;
				if ($new_ssid != $curr_ssid)
				{
					$change_flag++;
					$new_ssid = escape_any_characters($new_ssid);
					exec("qweconfig set ssid.wlan1 \"$new_ssid\"");
				}
				//Set Channel
				if ($new_channel != $curr_channel)
				{
					$change_flag++;
					exec("qweconfig set channel.wlan1 $new_channel");
				}
				//Set Protocol
				if ($new_proto != $curr_proto)
				{
					$change_flag++;
					exec("qweconfig set encryption.wlan1 $new_proto");
				}
				//Set Passphrase
				if ($new_psk != $curr_psk)
				{
					$change_flag++;
					$new_psk = escape_any_characters($new_psk);
					exec("qweconfig set passphrase.wlan1 \"$new_psk\"");
				}
				//set bw
				if ($new_bw!=$curr_bw)
				{
					$change_flag++;
					exec("qweconfig set bandwidth.wlan1 $new_bw");
				}
				//Set Broadcast SSID
				if ($new_broadcast != $curr_broadcast)
				{
					$change_flag++;
					exec("qweconfig set broadcastssid.wlan1 $new_broadcast");
				}
				//Set Beacon Interval
				if ($new_bintval != $curr_bintval){
					exec("qweconfig set bcnint.wlan1 $new_bintval");
				}
				//Set DTIM Period
				if ($new_dtim_period != $curr_dtim_period){
					exec("qweconfig set dtimperiod.wlan1 $new_dtim_period");
				}
				//Set Short GI
				if ($new_short_gi != $curr_short_gi)
				{
					$change_flag++;
					exec("qweconfig set shortgi.wlan1 $new_short_gi");
				}
				//Set 20/40 coexist
				if ($new_coexist != $curr_coexist)
				{
					$change_flag++;
					exec("qweconfig set coexist.wlan1 $new_coexist");
				}
				//Set Fragmentation threshold
				if ($new_frag_threshold != $curr_frag_threshold)
				{
					$change_flag++;
					exec("qweconfig set fragthres.wlan1 $new_frag_threshold");
				}
				//Set RTS/CTS  threshold
				if ($new_rts_threshold != $curr_rts_threshold)
				{
					$change_flag++;
					exec("qweconfig set rtsthres.wlan1 $new_rts_threshold");
				}
			}
			else if ($curr_mode == "1" || $curr_mode == "2")
			{
				//Set SSID
				if ($new_ssid != $curr_ssid)
				{
					$change_flag++;
					$ssid= escape_any_characters($new_ssid);
					exec("qweconfig set ssid.sta.wlan1 \"$new_ssid\"");
				}
				//Set Protocol
				if ($new_proto != $curr_proto)
				{
					$change_flag++;
					exec("qweconfig set encryption.sta.wlan1 $new_proto");
				}
				//Set Passphrase
				if ($new_psk != $curr_psk)
				{
					$change_flag++;
					$new_psk = escape_any_characters($new_psk);
					exec("qweconfig set passphrase.sta.wlan1 \"$new_psk\"");
				}
			}
			if ($change_flag>0)
			{
				exec("start-stop-daemon -S -b -x /bin/qweaction -- wlan1 commit");
			}
		}
	}
}


get_value();
if (isset($_POST['action']))
{
	set_value();
	get_value();
}

?>
<script type="text/javascript">
var mode = "<?php echo $curr_mode; ?>";

function isNatNumber(num)
{
	//numerals = /[0-9]/g;
	//if(numerals.test(num) && num > 1) return true;
	if(/^\d+$/.test(num)) return true;
	else return false;
}
function isHex(entry)
{
	validChar='0123456789ABCDEF';	// legal chars
	strlen=entry.length;			// test string length
	if(strlen != "12")
	{
		alert("MAC Address needs to be 12 characters long");
		return false;
	}
	entry=entry.toUpperCase(); // case insensitive
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

function populate_channellist()
{
	var cmb_channel = document.getElementById("cmb_channel");

	var ch_list="<?php echo $curr_channel_list;?>";
	var curr_channel="<?php echo $curr_channel; ?>";
	var tmp=ch_list.split(",");
	var n=0;
	if (mode == "0")
	{
		cmb_channel.options.add(new Option("Auto","0"));
		n=1;
		if( curr_channel=="0")
			cmb_channel.options[0].selected = true;
	}
	for(var i=0;i<tmp.length;i++)
	{
		cmb_channel.options.add(new Option(tmp[i],tmp[i]));
		if( curr_channel==tmp[i])
			cmb_channel.options[i+n].selected = true;
	}
}

function modechange(obj)
{
	var v;

	if(typeof modechange.toggle == 'undefined') modechange.toggle = 0;
	if(obj.name == "cmb_devicemode")
	{
		v = (modechange.toggle == 0)? true : false;
		modechange.toggle = (modechange.toggle == 0) ? 1 : 0;
		set_disabled('txt_essid', v);
		set_disabled('cmb_channel', v);
		set_disabled('cmb_encryption', v);
		set_disabled('txt_passphrase', v);

		set_disabled('cmb_band', v);
		set_disabled('cmb_bandwidth', v);
		set_disabled('chk_broadcast', v);
		set_disabled('txt_beaconinterval', v);
		set_disabled('txt_dtimperiod', v);
		set_disabled('chb_shortgi', v);
		set_disabled('chb_coexist', v);
		set_disabled('txt_frag_threshold', v);
		set_disabled('txt_rts_threshold', v);

		set_disabled('btn_aplist', v);
	}

	if(obj.name == 'cmb_encryption')
	{
		v = (document.mainform.cmb_encryption.selectedIndex > 0)? true : false;
		set_visible('tr_passphrase', v);
	}
}

function validate(action_name)
{
	var tmp = document.getElementById("action");
	tmp.value = action_name;

	if (action_name==0)//Save Button
	{
		nonhex = /[^A-Fa-f0-9]/g;
		nonascii = /[^\x20-\x7E]/;
		var ssid = document.getElementById("txt_essid");
		ssid.value=ssid.value.replace(/(\")/g, '\"');


		var cmb_devicemode = document.getElementById("cmb_devicemode");
		var broadcast = document.getElementById("chk_broadcast");
		var bint = document.getElementById("txt_beaconinterval");
		var dtim = document.getElementById("txt_dtimperiod");
		var frag_threshold = document.getElementById("txt_frag_threshold");
		var rts_threshold = document.getElementById("txt_rts_threshold");

		if(mode == cmb_devicemode.value )
		{
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

			if(document.mainform.cmb_encryption.selectedIndex > 0)
			{
				pw = document.getElementById("txt_passphrase");
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

				if(pw.value.length == 64 && (nonhex.test(pw.value)))
				{
					alert("Allowed Passphrase is 8 to 63 ASCII characters or 64 Hexadecimal digits");
					return false;
				}
			}
			else if(document.mainform.cmb_encryption.selectedIndex == "0")
			{
				if (mode == "0")
				{
					var tag = confirm('Disable the security?');
					if( tag != true )
					{
						return false;
					}
				}
			}

			if (broadcast.checked==false)
			{
				if (mode == "0")
				{
					var tag1=confirm('Disable the broadcast SSID? WPS will also be disabled!');
					if( tag1 != true )
					{
						changetab();
						return false;
					}
				}
			}
			if(mode=="0" && cmb_devicemode.selectedIndex=="0" && bint.value == "")
			{
				alert("Please enter beacon interval");
				return false;
			}

			if(mode=="0" && (bint.value != "") && (!(/^\d+$/.test(bint.value))))
			{
				alert("The beacon interval value should be natural numbers");
				return false;
			}
			if(mode=="0" && bint.value != "" && (bint.value < 25 || bint.value > 5000))
			{
				alert("Beacon interval needs to be between 25 and 5000");
				return false;
			}

			if(mode=="0" && cmb_devicemode.selectedIndex=="0" && dtim.value == "")
			{
				alert("Please enter DTIM period value");
				return false;
			}

			if(mode=="0" && (dtim.value != "") && (!(/^\d+$/.test(dtim.value))))
			{
				alert("The DTIM period value should be natural numbers");
				return false;
			}
			if(mode=="0" && (dtim.value != "" )&& ((dtim.value > 15) || (dtim.value < 1)))
			{
				alert("DTIM period needs to be between 1 and 15");
				return false;
			}

			if(mode=="0" && frag_threshold.value == "")
			{
				alert("Please enter fragmentation threshold value");
				return false;
			}
			if(mode=="0" && (frag_threshold.value != "0") && (!(/^\d+$/.test(frag_threshold.value))))
			{
				alert("The fragmentation threshold value should be natural numbers");
				return false;
			}
			if(mode=="0" && (frag_threshold.value != "0" )&& ((frag_threshold.value > 2346) || (frag_threshold.value < 256)))
			{
				alert("Fragmentation threshold needs to be between 256 and 2346");
				return false;
			}

			if(mode=="0" && rts_threshold.value == "")
			{
				alert("Please RTS/CTS threshold value");
				return false;
			}
			if(mode=="0" && (rts_threshold.value != "0") && (!(/^\d+$/.test(rts_threshold.value))))
			{
				alert("The RTS/CTS threshold value should be natural numbers");
				return false;
			}
			if(mode=="0" && (rts_threshold.value != "0" )&& ((rts_threshold.value > 2347) || (rts_threshold.value < 1)))
			{
				alert("RTS/CTS threshold needs to be between 1 and 2347");
				return false;
			}
		}
		document.mainform.submit();
	}
	else if(action_name==1)//Cancel Button
	{
		window.location.href="config_wireless24.php";
	}
	else if(action_name==2)//scan ap
	{
		popnew("config_aplist24.php");
	}
}

function onload_event()
{
	var curr_proto = "<?php echo $curr_proto; ?>";
	init_menu();
	populate_channellist();
	if (curr_proto == "open" || curr_proto == ""){
		set_visible('tr_passphrase', false);
	}
	if (privilege > 1)
	{
		set_visible('tbc_advanced', false);
	}

	if (mode == "0")
	{
		//set_visible("btn_aplist", false);
	}
	else
	{
		set_visible("tr_channel", false);
		set_visible("tbc_advanced", false);
	}

	set_control_value('cmb_devicemode','<?php echo $curr_mode; ?>', 'combox');
	set_control_value('cmb_encryption','<?php echo $curr_proto; ?>', 'combox');
	set_control_value('txt_passphrase','<?php echo $curr_psk; ?>', 'text');

	set_control_value('cmb_band','<?php echo $curr_band; ?>', 'combox');
	set_control_value('cmb_bandwidth','<?php echo $curr_bw; ?>', 'combox');
	set_control_value('chk_broadcast','<?php echo $curr_broadcast; ?>', 'checkbox');
	set_control_value('txt_beaconinterval','<?php echo $curr_bintval; ?>', 'text');
	set_control_value('txt_dtimperiod','<?php echo $curr_dtim_period; ?>', 'text');
	set_control_value('chb_shortgi','<?php echo $curr_short_gi; ?>', 'checkbox');
	set_control_value('chb_coexist','<?php echo $curr_coexist; ?>', 'checkbox');
	set_control_value('txt_frag_threshold','<?php echo $curr_frag_threshold; ?>', 'text');
	set_control_value('txt_rts_threshold','<?php echo $curr_rts_threshold; ?>', 'text');

	var frag_threshold = '<?php echo $curr_frag_threshold; ?>';
	var rts_threshold = '<?php echo $curr_rts_threshold; ?>';
	if (frag_threshold == '0')
	{
		set_visible("tr_frag_threshold", false);
	}

	if (rts_threshold == '0')
	{
		set_visible("tr_rts_threshold", false);
	}
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
			createMenu('<?php $tmp=exec("call_qcsapi get_mode wifi0"); echo $tmp;?>','<?php $tmp=exec("qweconfig get mode.wlan1"); echo $tmp;?>',privilege);
		</script>
	</div>
	<form enctype="multipart/form-data" action="config_wireless24.php" id="mainform" name="mainform" method="post">
	<input type="hidden" name="action" id="action" value="action" />
	<div class="right">
		<div class="righttop">2.4G WI-FI - CONFIG</div>
		<div id="TabbedPanels1" class="TabbedPanels">
			<ul class="TabbedPanelsTabGroup">
				<li class="TabbedPanelsTab" tabindex="0">Basic</li>
				<li class="TabbedPanelsTab" tabindex="0" id="tbc_advanced">Advanced</li>
			</ul>
			<div class="TabbedPanelsContentGroup">
				<div class="TabbedPanelsContent">
					<div class="rightmain">
						<table class="tablemain">
						<tr>
							<td width="40%">Device Mode:</td>
							<td width="60%">
								<select name="cmb_devicemode" class="combox" id="cmb_devicemode" onchange="modechange(this)">
									<option value="0">Access Point</option>
									<option value="1">Station</option>
									<option value="2">Repeater</option>
								</select>
							</td>
						</tr>
						<tr>
							<td class="divline" colspan="2";></td>
						</tr>
						<tr>
							<td>ESSID:</td>
							<td>
								<input name="txt_essid" type="text" id="txt_essid" class="textbox" value="<?php  echo htmlspecialchars($curr_ssid,ENT_QUOTES); ?>"/>
								<!--button name="btn_aplist" id="btn_aplist" type="button" onclick="validate(2);" class="button">Scan AP</button!-->
							</td>
						</tr>
						<tr id="tr_channel">
							<td>Channel:</td>
							<td>
								<select id="cmb_channel" name="cmb_channel" class="combox" onchange="modechange(this)">
								</select>
							</td>
						</tr>
						<tr>
							<td class="divline" colspan="2";></td>
						</tr>
						<tr>
							<td>Encryption:</td>
							<td>
								<select name="cmb_encryption" id="cmb_encryption" class="combox" onchange="modechange(this)">
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
							<td>Passphrase:</td>
							<td>
								<input name="txt_passphrase" type="text" id="txt_passphrase" value="<?php  echo htmlspecialchars($curr_psk,ENT_QUOTES); ?>" class="textbox"/>
							</td>
						</tr>
						<tr>
							<td class="divline" colspan="2";></td>
						</tr>
					</table>
						<div class="rightbottom">
							<button name="btn_save_basic" id="btn_save_basic" type="button" onclick="validate(0);"  class="button">Save</button>
							<button name="btn_cancel_basic" id="btn_cancel_basic" type="button" onclick="validate(1);"  class="button">Cancel</button>
						</div>
					</div>
				</div>
				<div class="TabbedPanelsContent">
					<div class="rightmain">
						<table class="tablemain">
                        <tr>
							<td width="40%">Wireless Band:</td>
							<td width="60%">
								<select name="cmb_band" class="combox" id="cmb_band">
									<option value="0"> 802.11g </option>
									<option value="1"> 802.11bg </option>
									<option value="2"> 802.11bgn </option>
								</select>
							</td>
						</tr>
						<tr id="tr_broadcast">
							<td>Broadcast SSID:</td>
							<td>
								<input name="chk_broadcast" id="chk_broadcast" type="checkbox" class="checkbox" value="1" />
							</td>
						</tr>
						<tr id="tr_bw">
							<td>Bandwidth:</td>
							<td>
								<select name="cmb_bandwidth" id="cmb_bandwidth"  class="combox">
									<option value="0"> 20M </option>
									<option value="1"> 40M </option>
								</select>
							</td>
						</tr>
						<tr>
							<td class="divline" colspan="2";></td>
						</tr>
						<tr id="tr_beaconinterval">
							<td>Beacon Interval (in ms):</td>
							<td>
								<input name="txt_beaconinterval" type="text" id="txt_beaconinterval" class="textbox" />
							</td>
						</tr>
						<tr id="tr_dtimperiod">
							<td>DTIM Period:</td>
							<td>
								<input name="txt_dtimperiod" type="text" id="txt_dtimperiod" class="textbox" />
							</td>
						</tr>
						<tr id="tr_shortgi">
							<td>Short GI:</td>
							<td>
								<input name="chb_shortgi" type="checkbox" id="chb_shortgi" value="1" />
							</td>
						</tr>
						<tr id="tr_coexist">
							<td>20/40MHz Coexist:</td>
							<td>
								<input name="chb_coexist" type="checkbox" id="chb_coexist" value="1" />
							</td>
						</tr>
						<tr id="tr_frag_threshold">
							<td>Fragmentation threshold:</td>
							<td>
								<input name="txt_frag_threshold" type="text" id="txt_frag_threshold" class="textbox" />
							</td>
						</tr>
						<tr id="tr_rts_threshold">
							<td>RTS/CTS  threshold:</td>
							<td>
								<input name="txt_rts_threshold" type="text" id="txt_rts_threshold" class="textbox" />
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
		</div>
	</div>
	</form>
<script type="text/javascript">
var TabbedPanels1 = new Spry.Widget.TabbedPanels("TabbedPanels1");
function changetab()
{
	TabbedPanels1.showPanel(1);
}
</script>
</div>
<div class="bottom">Quantenna Communications</div>

</body>
</html>

