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
$tmp=exec("qweaction wlan1 status wps");
switch ($tmp)
{
	case "0":
	  $wps_state="0 (WPS_INITAL)";
	  break;
	case "1":
	  $wps_state="1 (WPS_START)";
	  break;
	case "2":
	  $wps_state="2 (WPS_SUCCESS)";
	  break;
	case "3":
	  $wps_state="3 (WPS_ERROR)";
	  break;
	case "4":
	  $wps_state="4 (WPS_TIMEOUT)";
	  break;
	case "5":
	  $wps_state="5 (WPS_OVERLAP)";
	  break;
	case "6":
	  $wps_state="6 (WPS_CANCEL)";
	  break;
	case "7":
	  $wps_state="7 (WPS_UNKNOW)";
	  break;
	default:
	  $wps_state="7 (WPS_UNKNOW)";
	  break;
}
?>
<script>
var WPSState = '<?php echo $wps_state;?>';
var reload_address= 'config_wps_info24.php';

document.write(WPSState);
if((WPSState == '0 (WPS_INITAL)') || (WPSState =='1 (WPS_START)'))
{
	setTimeout('location.replace(reload_address)', 3000);
}

</script>
</center>
</body>
</html>

