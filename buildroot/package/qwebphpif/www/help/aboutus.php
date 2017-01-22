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

<?php
include("../common.php");
?>

<body class="body" onload="focus();">
	<div class="top">
		<a class="logo" href="../status_device.php">
			<img src="../images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="tools_logs.php" id="mainform" name="mainform" method="post">

<div style="border:6px solid #9FACB7; width:800px; height:auto; background-color:#fff;">
	<div class="righttop">About Us</div>
			<table class="tablemain" style=" height:auto">
				<div class="tablemain" style=" height:auto; width:750px">
					<br/><br/>
					<p>Quantenna Communications, Inc. is a leading developer of 802.11ac and 802.11n semiconductor solutions for the next generation of ultra-reliable,  Wi-Fi networks. The first company to introduce a commercially available, standards-based 802.11ac and 802.11n 4x4 Multiple Input Multiple Output (MIMO) chipset, Quantenna is focused on enabling a new wave of wireless multimedia entertainment solutions that require extremely predictable performance, rock-solid reliability, and easy service deployment in order to support whole-home, full HD video distribution and networking services over standard Wi-Fi networks. The company's industry-leading 802.11ac and 802.11n MIMO technology offers carrier-grade features which are essential for distributing multiple HD video streams with wire-like quality throughout the home at full, 1080p resolution.</p><br />
					<p>Quantenna's technology is targeted at devices such as wireless set-top boxes, residential gateways video bridges, and other devices that deliver highly reliable broadband multimedia video and data services over Wi-Fi anywhere in the home.</p><br />

					<p><strong>HD Video Wi-Fi Technology</strong><br />
					Quantenna fully integrated 4x4 MIMO 802.11ac and 802.11n Wi-Fi chipsets incorporate the full complement of next-generation features required for video and multimedia networking, including:</p><br />
					<ul>
					<li>4x4 MIMO technology with support for up to four spatial streams that are capable of unequal modulation to optimize SNR</li>
					<li>Dynamic digital beamforming, low density parity check (LDPC), mesh networking, and channel monitoring and optimization. These features enable dramatically higher levels of Wi-Fi througput, quality, reliability and coverage for whole-home networking and HD video entertainment.
					</li>
					</ul>
					<br /><br />

					<p><strong>Highest Performance for Whole Home Video & Data Distribution</strong><br />
					Quantenna's unique combination of technologies effectively achieves improvements including up to five times the coverage and as much as twice the throughput of existing, alternative solutions. Networks based on Quantenna's  technology will deliver the same quality as wired Ethernet, and deliver unmatched wireless networking and multimedia entertainment by delivering up to four HD video streams at more than 200 Mbps data rates, over 300 feet, with near-zero packet error rate (PER) data transfers, regardless of signal impairments and other interferences in the home.</p>

					<br /><p> <strong>Founded <u></u><br />
					</strong>Quantenna  was founded in 2006 by leading experts in the semiconductor, wireless and networking industries. The team's shared vision is to create wireless semiconductor solutions that solve the whole home wireless networking challenge and how to reliably move HD video and data for multimedia entertainment ubiquitous Wi-Fi technology. <u></u><u></u></p>

					<br /><p> <strong>Company</strong><u></u><br />
					  Quantenna boasts strong technical team, many of them with PhDs. It is headquartered in Fremont, California with offices in Australia, China, and Taiwan.</p>
		
					<br /><p><strong>Customers</strong><br />
					Quantenna's technology is deployed by major service providers including: AT&T, DirecTV, Swisscom, Telefonica, France Telecom and Rostelecom. It has also been selected by other major NA and European service providers for their future video and data networks.</p><br /><br />
				</div>
		</table>
	</div>
</div>
</form>
</div>
<div class="bottom"><?php echo $str_copy ?></div>

</body>
</html>

