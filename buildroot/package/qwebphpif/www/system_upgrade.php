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
?>

<script language="javascript" type="text/javascript">
function validate()
{
	var res = document.getElementById("result");
	var res1 = document.getElementById("result1");
        var file = document.getElementById('uploaded').files[0];
        if(!file )
	{
                alert("No file select. Please select the file");
		return false;
        }
	if (file.size > 8388608)
	{
                alert("Upload file size should be less than 8MB");
		return false;
        }

	res.style.visibility="visible";
	res1.style.visibility="hidden";
	set_disabled('btn_upgrade', true);
	document.mainform.submit();
}

var res1 = document.getElementById("result1");
res1.style.visibility="visible";
</script>
<body class="body" onload="init_menu();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="system_upgrade.php" id="mainform" name="mainform" method="post">
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php echo $curr_mode;?>',privilege);
		</script>
	</div>
	<div class="right">
		<div class="righttop">SYSTEM - UPGRADE</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr>
					<td width="20%">Choose a file:</td>
					<td width="80%">
						<input name="uploaded" id="uploaded" type="file" style="width:400px;"/>
					</td>
				</tr>
			</table>
			<div id="result" style="visibility:hidden; text-align:left; margin-left:20px; margin-top:10px; font:16px Calibri, Candara, corbel;">
				Loading the image file......Please wait.
			</div>
			<div id="result1" style="text-align:left; margin-left:20px; margin-top:0px; font:16px Calibri, Candara, corbel;">
			<?php
			if (isset($_FILES['uploaded']))
			{
				if (!(isset($_POST['csrf_token']) && $_POST['csrf_token'] === get_session_token())) {
					header('Location: login.php');
					exit();
				}

				$target = "/tmp/";
				$mtd = "/dev/mtd3";
				$target = $target."topaz-linux.lzma.img";
				$ok=0;
				$output = "";
				$token = "";
				$retval = 1;
				$bootcmd_ub=exec("call_qcsapi get_bootcfg_param bootcmd");
				$hw_config_id=exec("call_qcsapi get_bootcfg_param hw_config_id");
				$is_pcie=exec("ifconfig | grep pcie")==""?0:1 ;

				if($bootcmd_ub=="bootselect")
				{
					$bootselect_ub=exec("call_qcsapi get_bootcfg_param bootselect");
				}
				else
				{
					if($bootcmd_ub!="qtnboot")
					{
						$bootcmd_ub=exec("call_qcsapi get_bootcfg_param bootcmd | awk '{print $3}'");
					}
				}

				do {
					if($hw_config_id == 1217||$is_pcie == 1){
						echo "Error: PCIE and RGMII can't be upgraded through WebUI";
						if(file_exists($_FILES['uploaded']['tmp_name']))
						{
							unlink($_FILES['uploaded']['tmp_name']);
						}
						break;
					}

					if(!move_uploaded_file($_FILES['uploaded']['tmp_name'], $target)) {
						echo "Error: could not move uploaded file!<br>";
						if(file_exists($_FILES['uploaded']['tmp_name']))
						{
							unlink($_FILES['uploaded']['tmp_name']);
						}
						break;
					}

					/* Upgrade the firmware */
					if($bootcmd_ub!="bootselect"){
						exec("call_qcsapi flash_image_update $target live", $output, $retval);
						if ($output[0] != "complete") {
							$token = trim(strtok($output[0], ':'));
							if (strcmp($token, "Wrong image") == 0) {
								echo "$output[0]<br>";
							}
							echo "Could not write new firmware to flash!<br>";
							if(file_exists($target))
							{
								unlink($target);
							}
							break;
						}
					}
					else{
						if($bootselect_ub=="0")
						{
							exec("call_qcsapi flash_image_update $target safety", $output, $retval);
							if ($output[0] != "complete"){
								echo "Could not write new firmware to flash!<br>";
								if(file_exists($target))
								{
									unlink($target);
								}
								break;
							}
							exec("call_qcsapi update_bootcfg_param bootselect 1");
						}
						else if($bootselect_ub=="1")
						{
							exec("call_qcsapi flash_image_update $target live", $output, $retval);
							if ($output[0] != "complete"){
								echo "Could not write new firmware to flash!<br>";
								if(file_exists($target))
								{
									unlink($target);
								}
								break;
							}
							exec("call_qcsapi update_bootcfg_param bootselect 0");
						}
						else
						{
							echo "bootselect has invalid value";
							if(file_exists($target))
							{
								unlink($target);
							}
							break;
						}
					}
					$ok = 1;
					echo "Firmware upgraded successfully.<br>";
					$_SESSION['qtn_can_reboot']=TRUE;
					echo "<br>Click <a href=\"system_rebooted.php\" style=\"font-weight:bold\"> here </a>to reboot the platform.";
				} while(0);

				if ($ok == 0) {
					echo "Firmware upgrade failed.<br>";
				}
			}
			?>
			</div>
			<div class="rightbottom" style="text-align:left; margin-left:20px; margin-top:20px;">
				<button name="btn_upgrade" id="btn_upgrade" type="button" onclick="validate();"  class="button">Upgrade</button>
			</div>
		</div>
	</div>
</div>
<input type="hidden" name="csrf_token" value="<?php echo get_session_token(); ?>" />
</form>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> <br />
	<div><?php echo $str_copy ?></div>
</div>
</body>
</html>

