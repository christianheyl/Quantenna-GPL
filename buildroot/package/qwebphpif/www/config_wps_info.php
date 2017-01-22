#!/usr/lib/cgi-bin/php-cgi

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
	<title>Quantenna Communications</title>

	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<meta http-equiv="expires" content="0" />
	<meta http-equiv="CACHE-CONTROL" content="no-cache" />
</head>


<body>
<center>
<?php
include("common.php");
$privilege = get_privilege(1);

if (isset($_GET['id']))
{
	$page_id=substr($_GET['id'],0,1);
	$interface_id="wifi".$page_id;
}
else
{
	$page_id="0";
	$interface_id="wifi0";
}
$relaod_addr="config_wps_info.php?id=".$page_id;
$wps_state=exec("call_qcsapi get_wps_state $interface_id");
?>
<script>
var WPSState = '<?php echo $wps_state;?>';
var reload_address= '<?php echo $relaod_addr;?>';

document.write(WPSState);
if((WPSState != '2 (WPS_SUCCESS)') && (WPSState != '3 (WPS_ERROR)' && WPSState != '4 (WPS_TIMEOUT)'))
{
	setTimeout('location.replace(reload_address)', 3000);
}

</script>
</center>
</body>
</html>

