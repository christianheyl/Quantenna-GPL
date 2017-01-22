#!/usr/bin/haserl
Content-type: text/html


<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
	<title>Quantenna Communications</title>
	<link rel="stylesheet" type="text/css" href="../themes/style.css" media="screen" />

	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<meta http-equiv="expires" content="0" />
	<meta http-equiv="CACHE-CONTROL" content="no-cache" />
</head>
<script language="javascript" type="text/javascript" src="../js/cookiecontrol.js"></script>
<script language="javascript" type="text/javascript" src="../js/menu.js"></script>
<script language="javascript" type="text/javascript" src="../js/common_js.js"></script>

<%
curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`
%>



<body class="body" onload="focus();">
	<div class="top">
		<script type="text/javascript">
			createTop('<% echo -n $curr_version %>','<% echo -n $curr_mode %>');
		</script>
	</div>

<div style="border:6px solid #9FACB7; width:800px; height:auto; background-color:#fff;">
	<div class="righttop" >VENTURE PARTNERS</div>
		<div class="rightmain" style="color:#0B7682; font:18px Calibri, Arial, Candara, corbel;">
			<br clear="all" />

			<ul class="pr_list_mgmt">

			<li>
			<td ><h3 style="color:#FF3F00;">Bright Capital</h3></td>
			<td >Dr. Sulkhan Davitadze, Investment Director</td></li>

			<br>
			<li>
			<td ><h3 style="color:#FF3F00;">DAG Ventures</td ></h3>
			<td >Nicholas K. Pianim, Managing Director</td></li>

			<br>
			<li>
			<td ><h3 style="color:#FF3F00;">Grazia Equity GmbH</td ></h3>
			<td >Torsten Kreindl, Partner</td></li>

			<br>
			<li>
			<td ><h3 style="color:#FF3F00;">RUSNANO</td ></h3>
			<td >Georgy Kolpachev, Managing Director</td></li>

			<br>
			<li>
			<td ><h3 style="color:#FF3F00;">Sequoia Capital</td ></h3>
			<td >Michael L. Goguen, Venture Capitalist</td></li>

			<br>
			<li>
			<td ><h3 style="color:#FF3F00;">Sigma Partners</h3></td>
			<td >Fahri Diner, Partner</td></li>

			<br>
			<li>
			<td ><h3 style="color:#FF3F00;">Southern Cross Venture Partners</h3></td>
			<td >Dr. Larry Marshall, Managing Director</td></li>

			<br>
			<li>
			<td ><h3 style="color:#FF3F00;">Swisscom</h3></td>
			<td >P&auml;r Lange, Investment Manager</td></li>

			<br>
			<li>
			<td ><h3 style="color:#FF3F00;">Telef&oacute;nica Digital</h3></td>
			<td >Tracy Isacke, Director</td></li>
			
			<br>
			<li>
			<td ><h3 style="color:#FF3F00;">Venrock</h3></td>
			<td >Steve Goldberg, Partner</td></li>
			
			<br>
			</ul>
			
		</div>
	</div>
</div>
<div class="bottom">
<tr>
	<script type="text/javascript">
	createBot();
	</script>
</tr>
</div>


</body>
</html>

