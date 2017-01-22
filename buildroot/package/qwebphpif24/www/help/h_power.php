#!/usr/lib/cgi-bin/php-cgi

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
	<title>Quantenna Communications</title>
	<link rel="stylesheet" type="text/css" href="../themes/style.css" media="screen" />

	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<meta http-equiv="expires" content="0" />
	<meta http-equiv="CACHE-CONTROL" content="no-cache" />
</head>

<body class="body" onload="focus();">
	<div class="top">
		<a class="logo" href="../status_device.php">
			<img src="../images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="tools_logs.php" id="mainform" name="mainform" method="post">

<div style="border:6px solid #9FACB7; width:800px; height:auto; background-color:#fff;">
	<div class="righttop">Power</div>
			<table class="tablemain" style=" height:auto">
				<tr>
					<td width="30%">Tx Power</td>
					<td width="70%">
					table should be created once in the golden image for pThe purpose of power table is to comply with emission regulation of different regions such as US , EU , Russia, Japan and etc.
					The power table should be generated based on the final test results from approved EMI certification labs.
					Each board design will have its own power table which is due to different layout, RF front end and antenna designs. However, same product or design should have identical power table on each unit.
					The power articular design and pre-programmed to all flash devices for manufacturing.
				</td>
				</tr>
				<tr>
					<td class="divline" style="background:url(../images/divline.png);" colspan="2";></td>
				</tr>
				<tr>
					<td class="divline" style="background:url(../images/divline.png);" colspan="2";></td>
				</tr>
		</table>
	</div>
</div>
</form>
</div>
<div class="bottom">&copy; 2013 Quantenna Communications, Inc. All Rights Reserved.</div>

</body>
</html>

