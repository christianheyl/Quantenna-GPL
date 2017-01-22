#!/usr/bin/haserl
Content-type: text/html


<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
	<title>Quantenna Communications</title>
	<link rel="stylesheet" type="text/css" href="/themes/style.css" media="screen" />

	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<meta http-equiv="expires" content="0" />
	<meta http-equiv="CACHE-CONTROL" content="no-cache" />
</head>
<%
curr_mode="`call_qcsapi get_mode wifi0`"
curr_version=`call_qcsapi get_firmware_version`
%>

<script type="text/javascript">

function reload()
{
    window.location.reload();
}

</script>
<!--script part-->
<script language="javascript" type="text/javascript" src="./js/menu.js">
</script>

<body class="body">
<div class="top">
	<script type="text/javascript">
		createTop('<% echo -n $curr_version %>','<% echo -n $curr_mode %>');
	</script>
</div>
<div style="border:6px solid #9FACB7; width:800px; height:643px; background-color:#fff;">
		<div class="righttop">ASSOCIATION TABLE</div>
		<div class="rightmain">
			<table class="tablemain">
			<tr>
				<td width="5%"  align="center" bgcolor="#96E0E2" ></td>
				<td width="20%" align="center" bgcolor="#96E0E2"><% if [ "$curr_mode" = "Access point" ]; then echo "Station"; else echo "Access Point"; fi %></td>
				<td width="8%"  align="center" bgcolor="#96E0E2" <% if [ "$curr_mode" = "Station" ]; then echo "style=\"display:none\""; fi %>>VAP</td>
				<td width="8%"  align="center" bgcolor="#96E0E2">RSSI</td>
				<td width="17%" align="center" bgcolor="#96E0E2">Rx Bytes</td>
				<td width="17%" align="center" bgcolor="#96E0E2">Tx Bytes</td>
				<td width="5%"  align="center" bgcolor="#96E0E2">Bw</td>
				<td width="20%" align="center" bgcolor="#96E0E2">Time Associated</td>
			</tr>
			<pre><%
				interface_index=0;
				api_lost=0;
				while true
				do
					if [ -n "$GET_id" ]
					then
						interface="wifi$GET_id"
						api_lost=1;
					else
						interface=`call_qcsapi get_interface_by_index $interface_index`
						#if(!(strpos($interface, "QCSAPI entry point") === false)) {break;}
						#if(!(strpos($interface, "QCS API error") === false)) {
						#                            if ($interface_index === 0)
						#                                {$interface="wifi0";$api_lost=1;}        //in case the SDK do not have this API
						#
						#                            else
						#                               break;
						#                        }
						if [ $? -ne 0 ]
						then
							break;
						fi
						let interface_index=$interface_index+1;
					fi

					assoc_count=`call_qcsapi get_count_assoc $interface`
					if [ $? -ne 0 ]
					then
						assoc_count=0
					fi

					if [ "$assoc_count" -gt 0 ]
					then
						index=0
						while [ $index -lt $assoc_count ]
						do
							#mac
							station_mac=`call_qcsapi get_associated_device_mac_addr $interface $index`
							if [ $? -ne 0 ]
							then
								current_wifi_mode="`call_qcsapi get_mode wifi0`"
								if [ "$current_wifi_mode" = "Access point" ]
								then
									station_mac="-";
								else
									station_mac="`call_qcsapi get_bssid wifi0`"
								fi
							fi

							rssi="`call_qcsapi get_rssi_dbm $interface $index`"
							if [ $? -ne 0 ]
							then
								rssi="-";
							fi

							rx_bytes="`call_qcsapi get_rx_bytes $interface $index`"
							if [ $? -ne 0 ]
							then
								rx_bytes="-";
							fi

							tx_bytes="`call_qcsapi get_tx_bytes $interface $index`"
							if [ $? -ne 0 ]
							then
								tx_bytes="-";
							fi

							bw_assoc="`call_qcsapi get_assoc_bw $interface $index`"
							if [ $? -ne 0 ]
							then
								bw_assoc="-";
							fi

							time_associated="`call_qcsapi get_time_associated $interface $index`"
							if [ $? -ne 0 ]
							then
								$time_associated = "-";
							fi

							let index=$index+1;

							echo "<tr>";
							echo "<td width=\"5%\"  align=\"center\" >$index</td>";
							echo "<td width=\"20%\" align=\"center\" >$station_mac</td>";
							if [ "$curr_mode" != "Station" ]
							then
								echo "<td width=\"8%\"  align=\"center\" >$interface</td>";
							fi
							echo "<td width=\"8%\"  align=\"center\" >$rssi dbm</td>";
							echo "<td width=\"17%\" align=\"center\" >$rx_bytes</td>";
							echo "<td width=\"17%\" align=\"center\" >$tx_bytes</td>";
							echo "<td width=\"5%\"  align=\"center\" >$bw_assoc</td>";
							echo "<td width=\"20%\" align=\"center\" >$time_associated</td>";
							echo "</tr>";

						done
					fi

					if [ "$api_lost" -eq 1 ]
					then
						break
					fi
				done
			%></pre>
			</table>
			<div class="rightbottom">
				<button name="btn_refresh" id="btn_refresh" type="button" onclick="reload();"  class="button">Refresh</button>
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

