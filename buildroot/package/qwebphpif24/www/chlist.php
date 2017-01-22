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

exec("call_qcsapi get_scs_report wifi0 all | sed 's/  */ /g' > /tmp/ch_list");
?>

<body class="body">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<div style="border:6px solid #9FACB7; width:800px; background-color:#fff;">
	<div class="righttop">CHANNEL INFORMATION LIST</div>
	<form name="mainform" method="post" action="chlist.php">
	<div class="rightmain">
		<table class="tablemain">
			<tr>
				<td width="20%" align="center" bgcolor="#96E0E2" >Channel</td>
				<td width="20%" align="center" bgcolor="#96E0E2">DFS</td>
				<td width="20%" align="center" bgcolor="#96E0E2">Power</td>
				<td width="40%" align="center" bgcolor="#96E0E2">Interference</td>
			</tr>
			<?php
				$file_path="/tmp/ch_list";
				$channel="";
				$isdfs="";
				$power="";
				$cca = "";
				$rows = file($file_path);
				$rows_num = count($rows);
				$tmp = explode("=",$rows[0]);
				$ch_num = trim($tmp[1]);
				if ($ch_num == ""){
					$ch_num = "0";
				}
				if ($ch_num != "0")
				{
					for ($i=2; $i<$rows_num; $i++)
					{
						$tmp = explode(" ",trim($rows[$i]));
						$channel=$tmp[0];
						if ($tmp[1] == "0")
						{
							$isdfs="No";
						}
						else if ($tmp[1] == "1")
						{
							$isdfs="Yes";
						}
						else
						{
							$isdfs="";
						}
						$power=$tmp[2];
						$cca=$tmp[3]/10;
						echo "<tr>\n";
						echo "<td width=\"20%\" align=\"center\" >$channel</td>\n";
						echo "<td width=\"20%\" align=\"center\" >$isdfs</td>\n";
						echo "<td width=\"20%\" align=\"center\" >$power</td>\n";
						echo "<td width=\"40%\" align=\"center\" >$cca%</td>\n";
						echo "</tr>\n";
					}
				}
			?>
			</table>
		</div>
		</form>
	</div>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> <br />
	<div>&copy; 2013 Quantenna Communications, Inc. All Rights Reserved.</div>
</div>

<script type="text/javascript">
setTimeout('location.replace("chlist.php")', 2000);
</script>
<form enctype="multipart/form-data" action="chlist.php" id="postform" name="postform" method="post">
	<input type="hidden" name="action" />
</form>
</body>
</html>

