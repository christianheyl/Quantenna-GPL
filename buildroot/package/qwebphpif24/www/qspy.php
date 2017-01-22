#!/usr/lib/cgi-bin/php-cgi

<?php
if(isset($_GET['callback']) && $_GET['callback']=="get_info")
{
	$file_path="/tmp/ch_cca";
	exec("call_qcsapi get_scs_report wifi0 all | sed 's/  */ /g' > $file_path");

	$rows = file($file_path);
	$rows_num = count($rows);

	$tmp = explode("=",$rows[0]);
	$ch_num = trim($tmp[1]);

	if ($ch_num == ""){
		$ch_num = "0";
	}

	$json_res="{\"count\":\"".$ch_num."\"";

	if ($ch_num != "0")
	{
		$json_res=$json_res.",\"chinfo\":[";

		for ($i=2; $i<$rows_num; $i++)
		{
			$tmp = explode(" ",trim($rows[$i]));
			//print_r($tmp);
			//echo "</br>";
			$tmp_line="{\"ch\":\"$tmp[0]\",\"dfs\":\"$tmp[1]\",\"txpower\":\"$tmp[2]\",\"cca\":\"$tmp[3]\"},";
			$json_res=$json_res.$tmp_line;
		}
		$json_res=substr($json_res,0,strlen($json_res)-1);
		$json_res=$json_res."]";
	}

	$json_res=$json_res."}";
	echo $_GET['callback']. '(' . $json_res . ');';
}
else if (isset($_GET['callback']) && $_GET['callback']=="init")
{
	$smpl_intv=$_GET['smpl_intv'];
	$dwell_time=$_GET['dwell_time'];

	$tmp=exec("call_qcsapi enable_scs wifi0 1");
	if(!(strpos($tmp, "QCS API error") === FALSE)){
		$json_res=$_GET['callback']."({\"count\":\"0\"});";
		echo $json_res;
		return;
	}
	$tmp=exec("call_qcsapi set_scs_report_only wifi0 1");
	if(!(strpos($tmp, "QCS API error") === FALSE)){
		$json_res=$_GET['callback']."({\"count\":\"0\"});";
		echo $json_res;
		return;
	}
	$tmp=exec("call_qcsapi set_scs_smpl_enable wifi0 1");
	if(!(strpos($tmp, "QCS API error") === FALSE)){
		$json_res=$_GET['callback']."({\"count\":\"0\"});";
		echo $json_res;
		return;
	}
	$tmp=exec("call_qcsapi set_scs_smpl_intv wifi0 $smpl_intv");
	if(!(strpos($tmp, "QCS API error") === FALSE)){
		$json_res=$_GET['callback']."({\"count\":\"0\"});";
		echo $json_res;
		return;
	}
	$tmp=exec("call_qcsapi set_scs_smpl_dwell_time wifi0 $dwell_time");
	if(!(strpos($tmp, "QCS API error") === FALSE)){
		$json_res=$_GET['callback']."({\"count\":\"0\"});";
		echo $json_res;
		return;
	}
	$json_res=$_GET['callback']."({\"count\":\"1\"});";
	echo $json_res;
}
?>
