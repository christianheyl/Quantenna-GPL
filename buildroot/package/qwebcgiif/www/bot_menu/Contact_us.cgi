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
	<div class="righttop" >CONTACT US</div>
		<div class="rightmain" style="color:#0B7682; font:17px Calibri, Arial, Candara, corbel;">
			<h3 style="color:#FF3F00;">North America, Asia Pacific, Europe and Latin America</h3>

				<p><strong>Quantenna Communications</strong><br />
				3450 W. Warren Avenue<br />
				Fremont, CA 94538<br />
				Tel: +1 510-743-2260<br />
				Email: <a href="mailto:sales@quantenna.com" style="color:#2A00FF;">sales@quantenna.com</a></p><br />

				<h3 style="color:#FF3F00;">Asia Pacific Sales Contacts:</h3>

				<p><strong>Quantenna Communications</strong><br />
				7F, No.21, Lane 120, Section 1, NeiHu Road<br />
				NeiHu District<br />
				Taipei City 11492, Taiwan<br />
				Tel: +886 2-2627-5800 (ext. 101)<br />
				Email: <a href="mailto:sales@quantenna.com" style="color:#2A00FF;">sales@quantenna.com</a></p>

				<p><strong>Japan Contact</strong><br />
				3-1-27-2 Roppongi Minato-ku<br />
				Tokyo Japan<br />
				Tel: 03-6311-7161<br />
				Fax: 03-5575-6909<br />
				Email: <a href="mailto:sales@quantenna.com" style="color:#2A00FF;">sales@quantenna.com</a></p><br />

				<h3 style="color:#FF3F00;">Regional Offices:</h3>

				<h4>Australia</h4>

				<p>Quantenna Communications<br />
					Century Plaza Building<br />
					41 Rawson Street<br />
					Epping, NSW 2121, Australia<br />
					Tel: +61 2 8078 2231</p><br />

				<h3 style="color:#FF3F00;">Distributors - Asia Pacific</h3>

				<h4>Japan</h4>

				<p><strong>Altima Corporation</strong><br />
				1-5-5 Shin-Yokohama, Kouhoku-ku<br />
				Yokohama, Kanagawa 222-8563<br />
				Tel: +81(0) 45-476-2045<br />
				Fax: +81(0) 45-476-2046<br />
				Email: <a href="mailto:webmaster@altima.co.jp" style="color:#2A00FF;">webmaster@altima.co.jp</a></p>

				<h4>Korea</h4>

				<p><strong>Uniquest</strong><br />
				Uniquest B/D, 271-2, Seohyeon-Dong<br />
				Bundang-Gu Seongnam-Si, Gyeonggi-Do, 463-824<br />
				Tel: +82-31-776-9868<br />
				Email: <a href="mailto:quantenna@uniquest.co.kr" style="color:#2A00FF;">quantenna@uniquest.co.kr</a></p>

				<h4>Taiwan</h4>

				<p>7F, No.21, Lane 120, Section 1, NeiHu Road<br />
				NeiHu District<br />
				Taipei City 11492, Taiwan<br />
				Tel: +886 2-2627-5800 (ext. 101)</p><br />

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

