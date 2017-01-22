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
$curr_mode=exec("call_qcsapi get_mode wifi0");
if (strcmp($curr_mode, "Station" ) == 0)
{
	confirm("This page is only for the AP Mode.");
	echo "<script language='javascript'>location.href='status_device.php'</script>";
	return;
}
?>

<script type="text/javascript">
function reload()
{
	window.location.href="aplist.php";
}
function get_chlist()
{
	document.forms[1].action.value="0";
	document.postform.submit();
}
</script>

<?php
exec("call_qcsapi start_scan wifi0 background");
$tmp=exec("call_qcsapi wait_scan_completes wifi0 30");
if ($tmp=="complete")
{
	exec("iwlist wifi0 scanning last > /tmp/ap_list");
}
if (isset($_POST['action']))
{
	if ($_POST['action']=="0")
	{
		$tmp=exec("call_qcsapi enable_scs wifi0 1");
		$tmp=exec("call_qcsapi set_scs_report_only wifi0 1");
		$tmp=exec("call_qcsapi set_scs_smpl_enable wifi0 1");
		$tmp=exec("call_qcsapi set_scs_smpl_intv wifi0 2");
		$tmp=exec("call_qcsapi set_scs_smpl_dwell_time wifi0 10");
		echo '<script type="text/javascript">popupnew("chlist.php");</script>';
	}
}
?>

<body class="body">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<div style="border:6px solid #9FACB7; width:800px; background-color:#fff;">
	<div class="righttop">ACCESS POINT LIST</div>
	<form name="mainform" method="post" action="chlist.php">
	<div class="rightmain">
		<button name="btn_scan" id="btn_scan" type="button" onclick="reload();" class="button" style="width:200px">Scan for Alternate Networks</button>
		<button name="btn_chlist" id="btn_chlist" type="button" onclick="get_chlist();" class="button" style="width:200px">Channel List</button>
		<table class="tablemain">
			<tr>
				<td width="10%" align="center" bgcolor="#96E0E2" ></td>
				<td width="40%" align="center" bgcolor="#96E0E2">SSID</td>
				<td width="30%" align="center" bgcolor="#96E0E2">Mac Address</td>
				<td width="10%" align="center" bgcolor="#96E0E2">Channel</td>
				<td width="10%" align="center" bgcolor="#96E0E2">RSSI</td>
			</tr>
			<?php
				$file_path="/tmp/ap_list";
				$fp = fopen($file_path, 'r');
				$i=0;
				$mac="";
				$ssid="";
				$channel = "";
				$rssi = "";
				while(!feof($fp))
				{
					$buffer = stream_get_line($fp, 100, "\n");
					$token = trim(strtok($buffer, ':'));
					//ignore comments
					//if($token && substr($token, 0) == '#') continue;
					//echo $token."\n";

					if(!(strpos($token, "Address") === FALSE))
					{
						$token = trim(strtok(':'));
						$mac = $token.":";
						$token = trim(strtok(':'));
						$mac = $mac.$token.":";
						$token = trim(strtok(':'));
						$mac = $mac.$token.":";
						$token = trim(strtok(':'));
						$mac = $mac.$token.":";
						$token = trim(strtok(':'));
						$mac = $mac.$token.":";
						$token = trim(strtok(':'));
						$mac = $mac.$token;
						$i++;
						//break;
					}
					if(strcmp($token, "ESSID") == 0)
					{
						$token = trim(strtok(':'));
						$ssid = $token;
						//break;
					}
					if(strcmp($token, "Frequency") == 0)
					{
						$token = trim(strtok(':'));
						$tmp=trim(strtok($token, 'Channel'));
						$tmp = trim(strtok('Channel'));
						$channel = substr($tmp,0,strlen($tmp)-1);
						//break;
					}
					if(!(strpos($token, "Quality") === FALSE))
					{
						$tmp=trim(strtok($token, '='));
						$tmp=trim(strtok('='));
						$tmp=trim(strtok($tmp, '/'));
						$rssi=$tmp-90;
						
						echo "<tr>\n";
						echo "<td width=\"10%\" align=\"center\" >$i</td>\n";
						echo "<td width=\"40%\" align=\"center\" >".htmlspecialchars($ssid)."</td>\n";
						echo "<td width=\"30%\" align=\"center\" >$mac</td>\n";
						echo "<td width=\"10%\" align=\"center\" >$channel</td>\n";
						echo "<td width=\"10%\" align=\"center\" >$rssi dbm</td>\n";
						echo "</tr>\n";
						//break;
					}
				}
				fclose($fp);
			?>
			</table>
		</div>
		</form>
	</div>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> <br />
	<div><?php echo $str_copy ?></div>
</div>

<form enctype="multipart/form-data" action="chlist.php" id="postform" name="postform" method="post">
	<input type="hidden" name="action" />
</form>
</body>
</html>

