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
?>

<script type="text/javascript">
function validate()
{
	if (checkbox()==true)
	{
		document.mainform.submit();
	}
}

function checkbox()
{
	var txt_command = document.getElementById('txt_command');
	if(txt_command.value=='')
	{
		alert('The command should not be empty.');
		mainform.txt_command.focus();
		return false;
	}
	return true;
}

function keydownevent(event)
{
	if (event.keyCode==13) {validate();}
}

window.onload = function() {
var txt_command = document.getElementById('txt_command');
txt_command.focus();
}
</script>

<?php
$tmp="";
$selected=1;
if(isset($_POST['txt_command']) && isset($_POST['cmb_header']))
{
	if (!(isset($_POST['csrf_token']) && $_POST['csrf_token'] === get_session_token())) {
		header('Location: login.php');
		exit();
	}
	$head=$_POST['cmb_header'];
	$command=$head.$_POST['txt_command'];
	$tmp=shell_exec($command);
	if ($head == "")
	{$selected=0;}
	else
	{$selected=1;}
}
?>
<body class="body" onload="init_menu();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="tools_command.php" id="mainform" name="mainform" method="post">
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<?php echo $curr_mode;?>',privilege);
		</script>
	</div>
	<div class="right">
		<div class="righttop">TOOLS - COMMAND</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr>
					<td width="90%">
						<select name="cmb_header" class="combox" id="cmb_header" style="width:100px;">
							<option value="" <?php if($selected == 0) echo "selected=\"selected\""; ?>>None</option>
							<option value="call_qcsapi " <?php if($selected == 1) echo "selected=\"selected\""; ?>>call_qcsapi</option>
						</select>
					</td>
					<td width="10%"></td>
				</tr>
				<tr>
					<td>
						<input name="txt_command" type="text" id="txt_command" class="textbox" style="width:500px;" onkeydown="keydownevent(event);"/>
					</td>
					<td>
						<button name="btn_send" id="btn_send" type="button" onclick="validate();"  class="button" style="width:55px;">Send</button>
					</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td colspan="2"><?php echo $tmp;?></td>
				</tr>
			</table>
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

