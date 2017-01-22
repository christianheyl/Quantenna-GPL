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
$privilege = get_privilege(2);

?>


<script type="text/javascript">
var privilege="<?php echo $privilege; ?>";
</script>
<?php
$curr_mode=exec("call_qcsapi get_mode wifi0");
?>

<script type="text/javascript">

function validate(action_name)
{
	var tmp = document.getElementById("action");
	tmp.value = action_name;
	if (action_name==0)
	{
		if (checkbox()==false)
		{
			return false;
		}
		document.mainform.submit();
	}
}

function checkbox()
{
	var oldpsk = document.getElementById('txt_oldpsk');
	var newpsk = document.getElementById('txt_newpsk');
	var newpskagain = document.getElementById('txt_newpskagain');
	if(oldpsk.value=='')
	{
		alert('The old passphrase should not be empty.');
		oldpsk.focus();
		return false;
	}
	if(newpsk.value=='')
	{
		alert('The new password should not be empty.');
		newpsk.focus();
		return false;
	}
	newpsk.value=newpsk.value.replace(/(\")/g, "");
	newpskagain.value=newpskagain.value.replace(/(\")/g, "");
	if (newpsk.value != newpskagain.value)
	{
		alert('The passwords do not match.');
		newpskagain.focus();
		return false;
	}
	return true;
}
</script>

<?php
$res="";
$user="";
if($privilege==0)
{$user="super";}
else if($privilege==1)
{$user="admin";}
else if($privilege==2)
{$user="user";}

function admin_change($newuser,$newpsk)
{
	$file="/mnt/jffs2/admin.conf";
	$fp = fopen($file, 'r');
	$admin_contents = "";
	while(!feof($fp))
	{
		$buffer = stream_get_line($fp, 100, "\n");
		$token = trim(strtok($buffer, ','));
		if((strcmp($token, $newuser)) == 0)
		{
			$token = trim(strtok(','));
			$token = trim(strtok(','));
			$buffer = $newuser.",".$newpsk.",".$token;
		}
		$admin_contents .= "$buffer\n";
	}
	fclose($fp);
	file_put_contents($file, $admin_contents);
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
		$flag=0;
		$file_path = "/mnt/jffs2/admin.conf";
		$fp = fopen($file_path, 'r');
		$psk=md5($_POST['txt_oldpsk']);
		$newpsk=md5($_POST['txt_newpsk']);
		while(!feof($fp))
		{
			$buffer = stream_get_line($fp, 100, "\n");
			$arraylist=split(',',$buffer);
			if($arraylist[0]==$user && $arraylist[1]==$psk)
			{
				$flag=1;
				break;
			}
		}
		fclose($fp);
		if ($flag==1)
		{
			admin_change($user,$newpsk);
			$res="The change has been saved";
		} 
		else
		{$res="Error: Incorrect Password";}
	}
}

?>
<body class="body" onload="init_menu();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="tools_admin.php" id="mainform" name="mainform" method="post">
<input id="action" name="action" type="hidden" >
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php echo $curr_mode;?>',privilege);
		</script>
	</div>
	<div class="right">
		<div class="righttop">TOOLS - ADMIN</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr>
					<td width="45%">User Name:</td>
					<td width="55%">
						<input name="txt_user" type="text" id="txt_user" value="<?php echo $user;?>" class="textbox"/ disabled="disabled">
					</td>
				</tr>
				<tr>
					<td>Old Passphrase:</td>
					<td>
						<input name="txt_oldpsk" type="password" id="txt_oldpsk" value="" class="textbox"/>
					</td>
				</tr>
				<tr>
					<td>New Passphrase:</td>
					<td>
						<input name="txt_newpsk" type="password" id="txt_newpsk" value="" class="textbox">
					</td>
				</tr>
				<tr>
					<td>New Passphrase Again:</td>
					<td>
						<input name="txt_newpskagain" type="password" id="txt_newpskagain" value="" class="textbox"/>
					</td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
				<tr>
					<td colspan="2";>
						<div  style="text-align:left; margin-left:20px; margin-top:20px; font:16px Calibri, Candara, corbel, "Franklin Gothic Book";"><b><?php echo $res;?></b></div>
					</td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_save" id="btn_save" type="button" onclick="validate(0);"  class="button">Save</button>
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

