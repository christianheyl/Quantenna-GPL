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

$max_tx_power=exec("cat /proc/bootcfg/env | grep max_tx_power | cut -d \"=\" -f2");
$min_tx_power=exec("cat /proc/bootcfg/env | grep min_tx_power | cut -d \"=\" -f2");
$power_length=$max_tx_power-$min_tx_power-1;

$write_to_file = 0;
$pwr_cal_changed = 0;

$contents = file_get_contents("/mnt/jffs2/wireless_conf.txt");
parse_str($contents);

$old_pwr_cal = $pwr_cal;
$old_pwr = $pwr;

if(isset($_POST['pwr_cal_wifi0']))
{
	if (!(isset($_POST['csrf_token']) && $_POST['csrf_token'] === get_session_token())) {
		header('Location: login.php');
		exit();
	}
	$pwr_cal = htmlspecialchars($_POST['pwr_cal_wifi0']);
	$pwr = htmlspecialchars($_POST['tx_pwr_wifi0']);
}

$str_val = $pwr_cal;
if($old_pwr_cal != "")
{
	//if(($pwr_cal != $old_pwr_cal) && ($pwr_cal != ""))
	if($pwr_cal != "")
	{
		if($pwr_cal != $old_pwr_cal)
		{
			$old_pwr_cal = "pwr_cal=".$old_pwr_cal;
			$pwr_cal = "pwr_cal=".$pwr_cal;

			$contents = str_replace($old_pwr_cal, $pwr_cal, $contents);
			$pwr_cal = $str_val;
			$pwr_cal_changed = 1;
			$write_to_file = 1;
		}
		if($pwr_cal == "on")
		{
			/* If power cal is changed to ENABLED */
			if($pwr_cal_changed == 1)
				exec("set_tx_pow 255");

			if(($pwr != $old_pwr) && ($pwr != ""))
			{
				exec("set_tx_pow $pwr");
				$old_pwr = "pwr=".$old_pwr;
				$pwr = "pwr=".$pwr;

				$contents = str_replace($old_pwr, $pwr, $contents);
				$write_to_file = 1;
			}
		}
		else
		{
			/* If power cal is changed to DISABLED */
			if($pwr_cal_changed == 1)
				exec("set_tx_pow 0");
		}
	}
	if($write_to_file == 1)
	{file_put_contents("/mnt/jffs2/wireless_conf.txt", $contents);}
}
else /* Flash may not have this parameter */
{
	/* Add parameter */
	if ($pwr_cal != "")
	{
		$newpwr_cal = "pwr_cal=".$pwr_cal."&";
		$contents = str_pad($contents, (strlen($newpwr_cal)+strlen($contents)), $newpwr_cal, STR_PAD_LEFT);

		if($old_pwr == "")
		{
			$newpwr = "pwr=".$pwr."&";
			$contents = str_pad($contents, (strlen($newpwr)+strlen($contents)), $newpwr, STR_PAD_LEFT);
		}
		else if(($pwr != $old_pwr) && ($pwr != ""))
		{

			$old_pwr = "pwr=".$old_pwr;
			$pwr = "pwr=".$pwr;

			$contents = str_replace($old_pwr, $pwr, $contents);
		}

		file_put_contents("/mnt/jffs2/wireless_conf.txt", $contents);

		if($pwr_cal == "on")
		{
			exec("set_tx_pow $pwr");
		}
	}
}

$contents = file_get_contents("/mnt/jffs2/wireless_conf.txt");
parse_str($contents);

$curr_pwr_cal = $pwr_cal;
if($curr_pwr_cal=="")
	$curr_pwr_cal = "off";
$curr_pwr = $pwr;
?>

<script type="text/javascript">
function validate()
{
	document.mainform.submit();
}

function reload()
{
	window.location.href="tools_power.php";
}

function modechange(obj)
{
	if(obj.value == "off")
	{
		set_disabled("tx_pwr_wifi0",true);
	}
	else if (obj.value == "on")
	{
		set_disabled("tx_pwr_wifi0",false);
	}
}

function populate_powerlist()
{
	var tmp1 = "+";
	var tmp2 = "-";
	powerlist = document.getElementById("tx_pwr_wifi0");
	if(!powerlist) return;
	powerlist.options.length = <?php echo $power_length ?>;
	for(i=1;i<=powerlist.options.length;i++)
	{
		{powerlist.options[i-1].text = eval(i+tmp1+10); powerlist.options[i-1].value = eval(i+tmp1+10);}
	}
	power_value = '<?php echo $pwr ?>' ;
	power_index = eval(power_value+tmp2+11);
	powerlist.options[power_index].selected = true;
}

function onload_event()
{
	init_menu();
	populate_powerlist();
}
</script>

<body class="body" onload="onload_event();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="tools_power.php" id="mainform" name="mainform" method="post">
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php echo $curr_mode;?>',privilege);
		</script>
	</div>
	<div class="right">
		<div class="righttop">TOOLS - POWER</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr>
					<td width="40%">Tx Power</td>
					<td width="60%">
					<input id="action" type="radio" name="pwr_cal_wifi0" value="on"  <?php if($curr_pwr_cal == "on") echo "checked=\"checked\""; ?> onclick="modechange(this);"/> On &nbsp; &nbsp;
					<input type="radio" name="pwr_cal_wifi0" value="off" <?php if($curr_pwr_cal == "off") echo "checked=\"checked\""; ?> onclick="modechange(this);"/> Off
					</td>
				</tr>
				<tr>
					<td>Tx Power</td>
					<td>
					<select id="tx_pwr_wifi0" name="tx_pwr_wifi0" <?php if($curr_pwr_cal == "off") echo "disabled=\"disabled\""; ?>>

					</select>
					</td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
			</table>
			<div class="rightbottom">
				<input type="submit" name="btn_save" class="button" value="Save" />
				<button name="btn_cancel" id="btn_cancel" type="button" onclick="reload();"  class="button">Cancel</button>
			</div>
		</div>
	</div>
</div>
<input type="hidden" name="csrf_token" value="<?php echo get_session_token(); ?>" />
</form>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> | <a href="help/h_power.php">Help</a><br />
	<div><?php echo $str_copy ?></div>
</div>

</body>
</html>

