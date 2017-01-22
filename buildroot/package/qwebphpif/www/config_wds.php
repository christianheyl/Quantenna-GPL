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
$privilege = get_privilege(2);
?>


<script type="text/javascript">
var privilege="<?php echo $privilege; ?>";
</script>

<?php
$curr_mode=exec("call_qcsapi get_mode wifi0");
if($curr_mode=="Station")
{
	echo "<script langauge=\"javascript\">alert(\"Don`t support in the Station mode.\");</script>";
	echo "<script language='javascript'>location.href='status_device.php'</script>";
	return;
}
//=================Load Value======================
$chk0=1;
$chk1=1;
$chk2=1;
$chk3=1;
$chk4=1;
$chk5=1;
$chk6=1;
$chk7=1;
$wds0="";
$wds1="";
$wds2="";
$wds3="";
$wds4="";
$wds5="";
$wds6="";
$wds7="";
$psk0="";
$psk1="";
$psk2="";
$psk3="";
$psk4="";
$psk5="";
$psk6="";
$psk7="";
$count_for_wds=-1;
$vlan0="";
$vlan1="";
$vlan2="";
$vlan3="";
$vlan4="";
$vlan5="";
$vlan6="";
$vlan7="";
$file_vlan="/mnt/jffs2/vlan_config.txt";

function find_wds_passphrase($wds, &$passphrase)
{
	$file="/mnt/jffs2/wds_config.txt";
	if(file_exists($file))
	{
		$fp = fopen($file, 'r');
		$result="";
		if($fp != NULL)
		{
			while(!feof($fp))
			{
				$buffer = stream_get_line($fp, 100, "\n");
				$token = trim(strtok($buffer, '='));
				if((strcmp($token, "mac") == 0))
				{
					$token = strtok(" ");
					if((strcmp($token, $wds) == 0))
					{
						$buffer = stream_get_line($fp, 100, "\n");
						$token = trim(strtok($buffer, '='));
						$token = strtok(" ");
						$result=$token;
						break;
					}
				}
			}
			fclose($fp);
		}
		$passphrase=$result;
	}
}

function getValue()
{
	global $chk0,$chk1,$chk2,$chk3,$chk4,$chk5,$chk6,$chk7,$wds0,$wds1,$wds2,$wds3,$wds4,$wds5,$wds6,$wds7,$psk0,$psk1,$psk2,$psk3,$psk4,$psk5,$psk6,$psk7,$vlan0,$vlan1,$vlan2,$vlan3,$vlan4,$vlan5,$vlan6,$vlan7,$vlan_num,$vlan_id,$file_vlan;
	//==============Get WDS Peer=======================
	$wds0=exec("call_qcsapi wds_get_peer_address wifi0 0");
	if(is_qcsapi_error($wds0))
	{
		$chk0=0;
		$wds0="N/A";
	}
	else
	{
		find_wds_passphrase($wds0,$psk0);
		$vlan0=exec("cat $file_vlan | grep wds0 | awk -F ':' '{print $2}'");
	}
	$wds1=exec("call_qcsapi wds_get_peer_address wifi0 1");
	if(is_qcsapi_error($wds1))
	{
		$wds1="N/A";
		$chk1=0;
	}
	else
	{
		find_wds_passphrase($wds1,$psk1);
		$vlan1=exec("cat $file_vlan | grep wds1 | awk -F ':' '{print $2}'");
	}
	$wds2=exec("call_qcsapi wds_get_peer_address wifi0 2");
	if(is_qcsapi_error($wds2))
	{
		$wds2="N/A";
		$chk2=0;
	}
	else
	{
		find_wds_passphrase($wds2,$psk2);
		$vlan2=exec("cat $file_vlan | grep wds2 | awk -F ':' '{print $2}'");
	}
	$wds3=exec("call_qcsapi wds_get_peer_address wifi0 3");
	if(is_qcsapi_error($wds3))
	{
		$wds3="N/A";
		$chk3=0;
	}
	else
	{
		find_wds_passphrase($wds3,$psk3);
		$vlan3=exec("cat $file_vlan | grep wds3 | awk -F ':' '{print $2}'");
	}
	$wds4=exec("call_qcsapi wds_get_peer_address wifi0 4");
	if(is_qcsapi_error($wds4))
	{
		$wds4="N/A";
		$chk4=0;
	}
	else
	{
		find_wds_passphrase($wds4,$psk4);
		$vlan4=exec("cat $file_vlan | grep wds4 | awk -F ':' '{print $2}'");
	}
	$wds5=exec("call_qcsapi wds_get_peer_address wifi0 5");
	if(is_qcsapi_error($wds5))
	{
		$wds5="N/A";
		$chk5=0;
	}
	else
	{
		find_wds_passphrase($wds5,$psk5);
		$vlan5=exec("cat $file_vlan | grep wds5 | awk -F ':' '{print $2}'");
	}
	$wds6=exec("call_qcsapi wds_get_peer_address wifi0 6");
	if(is_qcsapi_error($wds6))
	{
		$wds6="N/A";
		$chk6=0;
	}
	else
	{
		find_wds_passphrase($wds6,$psk6);
		$vlan6=exec("cat $file_vlan | grep wds6 | awk -F ':' '{print $2}'");
	}
	$wds7=exec("call_qcsapi wds_get_peer_address wifi0 7");
	if(is_qcsapi_error($wds7))
	{
		$wds7="N/A";
		$chk7=0;
	}
	else
	{
		find_wds_passphrase($wds7,$psk7);
		$vlan7=exec("cat $file_vlan | grep wds7 | awk -F ':' '{print $2}'");
	}
	//=================================================
		exec("cat /mnt/jffs2/vlan_config.txt | cut -d ':' -f2 > /tmp/vlan_id");
	$vlan_num=exec("cat /tmp/vlan_id | wc -l");
	for($i=0;$i<$vlan_num;$i++)
	{
		$j=$i+1;
		$vlan_id[$i]=exec("cat /tmp/vlan_id | head -n$j | tail -n1");
	}
}
function setValue($old_chk,$new_chk,$old_wds,$new_wds,$new_psk,$old_vlan,$new_vlan,$wds_if)
{
	global $count_for_wds,$file_vlan,$content_vlan;

	$new_psk_esc=escapeshellarg(escape_any_characters($new_psk));
	$new_wds_esc=escapeshellarg($new_wds);
	$new_vlan_esc=escapeshellarg($new_vlan);

	$count_for_wds=$count_for_wds+1;
	if($new_chk=="on")
	{$new_chk=1;}
	else
	{$new_chk=0;}
	//confirm("old chk: $old_chk, new chk: $new_chk");
	if($new_chk!=$old_chk)
	{
		if ($old_chk==1)
		{
			//Remove the vlan tag for the device
			if($old_vlan!="")
			{
				exec("qvlan unbind $wds_if $old_vlan");
				$content_vlan=str_replace("$wds_if:$old_vlan","#$wds_if:",$content_vlan);
			}
			exec("call_qcsapi wds_remove_peer wifi0 $old_wds");
		}
		else
		{
			$tmp=exec("call_qcsapi wds_add_peer wifi0 $new_wds_esc");
			if($tmp!="complete")
			{
				confirm("WDS$count_for_wds address is a Local Mac address which can't be used as wds peer address. Please enter valid Mac address.");
				return;
			}
			//sleep(3);
			if($new_psk!="")
			{exec("call_qcsapi wds_set_psk wifi0 $new_wds_esc $new_psk_esc");}
			//add vlan
			if($new_vlan!="")
			{
				exec("qvlan bind $wds_if $new_vlan");
				$content_vlan=str_replace("#$wds_if:","$wds_if:$new_vlan",$content_vlan);
			}
		}
	}
	else if ($new_chk==$old_chk && $new_chk==1)
	{
		if ($new_wds!=$old_wds)
		{
			if($old_vlan!="")
			exec("qvlan unbind $wds_if $old_vlan");
			exec("call_qcsapi wds_remove_peer wifi0 $old_wds");
			exec("call_qcsapi wds_add_peer wifi0 $new_wds_esc");
			if($old_vlan!="")
			exec("qvlan bind $wds_if $old_vlan");
			//sleep(3);
		}
		if($new_psk!="" && $new_psk!="********")
		{exec("call_qcsapi wds_set_psk wifi0 $new_wds_esc $new_psk_esc");}
		else if ($new_psk=="")
		{exec("call_qcsapi wds_set_psk wifi0 $new_wds_esc NULL");}

		if ($old_vlan!=$new_vlan)
		{
			//Remove vlan tag for the device
			if ($new_vlan=="")
			{
				exec("qvlan unbind $wds_if $old_vlan");
				$content_vlan=str_replace("$wds_if:$old_vlan","#$wds_if:",$content_vlan);
			}
			//Change a vlan tag for the device
			else
			{
				if($old_vlan=="")
				{

					$tmp=exec("qvlan bind $wds_if $new_vlan");
					$content_vlan=str_replace("#$wds_if:","$wds_if:$new_vlan",$content_vlan);
				}
				else
				{
					exec("qvlan unbind $wds_if $old_vlan");
					exec("qvlan bind $wds_if $new_vlan");
					$content_vlan=str_replace("$wds_if:$old_vlan","$wds_if:$new_vlan",$content_vlan);
				}
			}
		}
	}
}

function generate_vlan_config_file($file_path)
{
	if(!file_exists($file_path))
	{
		//confirm("create new vlan file");
		$config_file_content="#wifi0:\n#wifi1:\n#wifi2:\n#wifi3:\n#wifi4:\n#wifi5:\n#wifi6:\n#wifi7:\n#wds0:\n#wds1:\n#wds2:\n#wds3:\n#wds4:\n#wds5:\n#wds6:\n#wds7:\n#wds8:\n";
		file_put_contents($file_path, $config_file_content);
		exec("chmod 777 /mnt/jffs2/vlan_config.txt");
	}
}
//=================Load Value======================
//=================Set Value=======================
generate_vlan_config_file($file_vlan);
$content_vlan=file_get_contents($file_vlan);
if(isset($_POST['action']))
{
	if (!(isset($_POST['csrf_token']) && $_POST['csrf_token'] === get_session_token())) {
		header('Location: login.php');
		exit();
	}

	getValue();
	if (isset($_POST['chk_wds0']))
	{
		$new_chk0=$_POST['chk_wds0'];
		$new_wds0=$_POST['txt_mac0'];
		$new_pass0=$_POST['txt_pass0'];
		$new_vlan0=$_POST['txt_vlan0'];
	}
	else
	{
		$new_chk0="";
		$new_wds0="";
		$new_pass0="";
		$new_vlan0="";
	}

	if (isset($_POST['chk_wds1']))
	{
		$new_chk1=$_POST['chk_wds1'];
		$new_wds1=$_POST['txt_mac1'];
		$new_pass1=$_POST['txt_pass1'];
		$new_vlan1=$_POST['txt_vlan1'];
	}
	else
	{
		$new_chk1="";
		$new_wds1="";
		$new_pass1="";
		$new_vlan1="";
	}
	if (isset($_POST['chk_wds2']))
	{
		$new_chk2=$_POST['chk_wds2'];
		$new_wds2=$_POST['txt_mac2'];
		$new_pass2=$_POST['txt_pass2'];
		$new_vlan2=$_POST['txt_vlan2'];
	}
	else
	{
		$new_chk2="";
		$new_wds2="";
		$new_pass2="";
		$new_vlan2="";
	}
	if (isset($_POST['chk_wds3']))
	{
		$new_chk3=$_POST['chk_wds3'];
		$new_wds3=$_POST['txt_mac3'];
		$new_pass3=$_POST['txt_pass3'];
		$new_vlan3=$_POST['txt_vlan3'];
	}
	else
	{
		$new_chk3="";
		$new_wds3="";
		$new_pass3="";
		$new_vlan3="";
	}
	if (isset($_POST['chk_wds4']))
	{
		$new_chk4=$_POST['chk_wds4'];
		$new_wds4=$_POST['txt_mac4'];
		$new_pass4=$_POST['txt_pass4'];
		$new_vlan4=$_POST['txt_vlan4'];
	}
	else
	{
		$new_chk4="";
		$new_wds4="";
		$new_pass4="";
		$new_vlan4="";
	}
	if (isset($_POST['chk_wds5']))
	{
		$new_chk5=$_POST['chk_wds5'];
		$new_wds5=$_POST['txt_mac5'];
		$new_pass5=$_POST['txt_pass5'];
		$new_vlan5=$_POST['txt_vlan5'];
	}
	else
	{
		$new_chk5="";
		$new_wds5="";
		$new_pass5="";
		$new_vlan5="";
	}
	if (isset($_POST['chk_wds6']))
	{
		$new_chk6=$_POST['chk_wds6'];
		$new_wds6=$_POST['txt_mac6'];
		$new_pass6=$_POST['txt_pass6'];
		$new_vlan6=$_POST['txt_vlan6'];
	}
	else
	{
		$new_chk6="";
		$new_wds6="";
		$new_pass6="";
		$new_vlan6="";
	}
	if (isset($_POST['chk_wds7']))
	{
		$new_chk7=$_POST['chk_wds7'];
		$new_wds7=$_POST['txt_mac7'];
		$new_pass7=$_POST['txt_pass7'];
		$new_vlan7=$_POST['txt_vlan7'];
	}
	else
	{
		$new_chk7="";
		$new_wds7="";
		$new_pass7="";
		$new_vlan7="";
	}
	setValue($chk0,$new_chk0,$wds0,$new_wds0,$new_pass0,$vlan0,$new_vlan0,"wds0");
	setValue($chk1,$new_chk1,$wds1,$new_wds1,$new_pass1,$vlan1,$new_vlan1,"wds1");
	setValue($chk2,$new_chk2,$wds2,$new_wds2,$new_pass2,$vlan2,$new_vlan2,"wds2");
	setValue($chk3,$new_chk3,$wds3,$new_wds3,$new_pass3,$vlan3,$new_vlan3,"wds3");
	setValue($chk4,$new_chk4,$wds4,$new_wds4,$new_pass4,$vlan4,$new_vlan4,"wds4");
	setValue($chk5,$new_chk5,$wds5,$new_wds5,$new_pass5,$vlan5,$new_vlan5,"wds5");
	setValue($chk6,$new_chk6,$wds6,$new_wds6,$new_pass6,$vlan6,$new_vlan6,"wds6");
	setValue($chk7,$new_chk7,$wds7,$new_wds7,$new_pass7,$vlan7,$new_vlan7,"wds7");


	$file="/mnt/jffs2/wds_config.txt";
	$config_contents="";
	if($new_chk0=="on")
	{
		$config_contents .= "wds={\n mac=$new_wds0\n psk=";
		if($new_pass0=="")
		{$config_contents .="NULL\n}\n\n";}
		else if($new_pass0=="********")
		{$config_contents .="$psk0\n}\n\n";}
		else
		{$config_contents .="$new_pass0\n}\n\n";}
	}
	if($new_chk1=="on")
	{
		$config_contents .= "wds={\n mac=$new_wds1\n psk=";
		if($new_pass1=="")
		{$config_contents .="NULL\n}\n\n";}
		else if($new_pass1=="********")
		{$config_contents .="$psk1\n}\n\n";}
		else
		{$config_contents .="$new_pass1\n}\n\n";}
	}
	if($new_chk2=="on")
	{
		$config_contents .= "wds={\n mac=$new_wds2\n psk=";
		if($new_pass2=="")
		{$config_contents .="NULL\n}\n\n";}
		else if($new_pass2=="********")
		{$config_contents .="$psk2\n}\n\n";}
		else
		{$config_contents .="$new_pass2\n}\n\n";}
	}
	if($new_chk3=="on")
	{
		$config_contents .= "wds={\n mac=$new_wds3\n psk=";
		if($new_pass3=="")
		{$config_contents .="NULL\n}\n\n";}
		else if($new_pass3=="********")
		{$config_contents .="$psk3\n}\n\n";}
		else
		{$config_contents .="$new_pass3\n}\n\n";}
	}
	if($new_chk4=="on")
	{
		$config_contents .= "wds={\n mac=$new_wds4\n psk=";
		if($new_pass4=="")
		{$config_contents .="NULL\n}\n\n";}
		else if($new_pass4=="********")
		{$config_contents .="$psk4\n}\n\n";}
		else
		{$config_contents .="$new_pass4\n}\n\n";}
	}
	if($new_chk5=="on")
	{
		$config_contents .= "wds={\n mac=$new_wds5\n psk=";
		if($new_pass5=="")
		{$config_contents .="NULL\n}\n\n";}
		else if($new_pass5=="********")
		{$config_contents .="$psk5\n}\n\n";}
		else
		{$config_contents .="$new_pass5\n}\n\n";}
	}
	if($new_chk6=="on")
	{
		$config_contents .= "wds={\n mac=$new_wds6\n psk=";
		if($new_pass6=="")
		{$config_contents .="NULL\n}\n\n";}
		else if($new_pass6=="********")
		{$config_contents .="$psk6\n}\n\n";}
		else
		{$config_contents .="$new_pass6\n}\n\n";}
	}
	if($new_chk7=="on")
	{
		$config_contents .= "wds={\n mac=$new_wds7\n psk=";
		if($new_pass7=="")
		{$config_contents .="NULL\n}\n\n";}
		else if($new_pass7=="********")
		{$config_contents .="$psk7\n}\n\n";}
		else
		{$config_contents .="$new_pass7\n}\n\n";}
	}
	file_put_contents($file, $config_contents);
	file_put_contents($file_vlan, $content_vlan);

	echo "<script language='javascript'>setTimeout('location.replace(\"config_wds.php\")', 2000); </script>";
}

getValue();

//=================Set Value=======================
?>
<script type="text/javascript">

function reload()
{
	window.location.href="config_wds.php";
}

function CheckMac(address)
{
	//mac address regular expression
	var reg_test=/^([0-9a-fA-F]{2})(([//\s:-][0-9a-fA-F]{2}){5})$/;
	var tmp_test_mac=address.substring(0,2);
	var tmp_output=parseInt(tmp_test_mac,16).toString(2)
	String.prototype.Right = function(i) {return this.slice(this.length - i,this.length);};
	tmp_output = tmp_output.Right(1);
	if(!reg_test.test(address))
	{return false;}
	else
	{
		if(1 == tmp_output)
		return false;
	}
}

function test(wdsid)
{
	var chk="chk_wds"+wdsid;
	var txtmac="txt_mac"+wdsid;
	var txtpass="txt_pass"+wdsid;
	var res = document.getElementById("result");
	var chk0=document.getElementById(chk);
	var txtmac0=document.getElementById(txtmac);
	var txtpass0=document.getElementById(txtpass);
	if(chk0.checked==true)
	{
		if(CheckMac(txtmac0.value)==false)
		{
			txtmac0.focus();
			res.innerHTML="Invalid MAC address. Please Check.";
			res.style.visibility="visible";
			return false;
		}
		if(txtpass0.value.length != 64 && txtpass0.value.length != 0 && txtpass0.value != "********")
		{
			txtpass0.focus();
			res.innerHTML="Passphrase must contain 64 ASCII characters";
			res.style.visibility="visible";
			return false;
		}
	}
	return true;
}

function test_vlan(wssid)
{
	var no_num= /[^0-9]/ ;
	var txt_vlan_id="txt_vlan"+wssid;
	var txt_vlan=document.getElementById(txt_vlan_id);
	var res = document.getElementById("result");
	var vlan_id=new Array();
	var x;

	if(txt_vlan.value!="")
	{
		if ((no_num.test(txt_vlan.value)))
		{
			txt_vlan.focus();
			res.innerHTML="Only Numbers are allowed in VLANID";
			res.style.visibility="visible";
			return false;
		}
		if (txt_vlan.value <1 || txt_vlan.value > 4095)
		{
			txt_vlan.focus();
			res.innerHTML="Vlan ID is only allowed between 1-4095";
			res.style.visibility="visible";
			return false;
		}
	}

	vlan_id[0]="<?php echo $vlan_id[0]; ?>";
	vlan_id[1]="<?php echo $vlan_id[1]; ?>";
	vlan_id[2]="<?php echo $vlan_id[2]; ?>";
	vlan_id[3]="<?php echo $vlan_id[3]; ?>";
	vlan_id[4]="<?php echo $vlan_id[4]; ?>";
	vlan_id[5]="<?php echo $vlan_id[5]; ?>";
	vlan_id[6]="<?php echo $vlan_id[6]; ?>";
	vlan_id[7]="<?php echo $vlan_id[7]; ?>";
	vlan_id[8]="<?php echo $vlan_id[8]; ?>";
	vlan_id[9]="<?php echo $vlan_id[9]; ?>";
	vlan_id[10]="<?php echo $vlan_id[10]; ?>";
	vlan_id[11]="<?php echo $vlan_id[11]; ?>";
	vlan_id[12]="<?php echo $vlan_id[12]; ?>";
	vlan_id[13]="<?php echo $vlan_id[13]; ?>";
	vlan_id[14]="<?php echo $vlan_id[14]; ?>";
	vlan_id[15]="<?php echo $vlan_id[15]; ?>";
	vlan_id[16]="<?php echo $vlan_id[16]; ?>";
	vlan_id[17]="<?php echo $vlan_id[17]; ?>";

	for (x=1; x<wssid+8;x++ )
	{
		if(vlan_id[x]!=""&&txt_vlan.value==vlan_id[x])
		{
			txt_vlan.focus();
			res.innerHTML="Vlan ID has been used. Please enter a different Vlan ID";
			res.style.visibility="visible";
			return false;
		}
	}
	for (x=wssid+9; x<16;x++ )
	{
		if(vlan_id[x]!=""&&txt_vlan.value==vlan_id[x])
		{
			txt_vlan.focus();
			res.innerHTML="Vlan ID has been used. Please enter a different Vlan ID";
			res.style.visibility="visible";
			return false;
		}
	}
	return true;
}


function validate()
{
	var cf = document.forms[0];
	var MacAddr = new Array(cf.txt_mac0,cf.txt_mac1,cf.txt_mac2,cf.txt_mac3,cf.txt_mac4,cf.txt_mac5,cf.txt_mac6,cf.txt_mac7);
	var res = document.getElementById("result");

	var chk=new Array();
	var x;
	var j;
	var z;
	var count_for_check=0;
	for(x=0;x<=7;x++)
	{
		var chk_id="chk_wds"+x;
		chk[x]=document.getElementById(chk_id);
	}

	for(j=0;j<=7;j++)
	{
		if(chk[j].checked==true)
		{count_for_check++;}
	}

	for(z=0;z<count_for_check;z++)
	{
		if(chk[z].checked==false)
		{
			//alert("count_for_check="+count_for_check+"z="+z+"chk[z]="+chk[z]);
			res.innerHTML="Please keep all wds together from top";
			res.style.visibility="visible";
			return false;
		}
	}

	if(test(0)==false||test_vlan(0)==false)
	{return;}
	if(test(1)==false||test_vlan(1)==false)
	{return;}
	if(test(2)==false||test_vlan(2)==false)
	{return;}
	if(test(3)==false||test_vlan(3)==false)
	{return;}
	if(test(4)==false||test_vlan(4)==false)
	{return;}
	if(test(5)==false||test_vlan(5)==false)
	{return;}
	if(test(6)==false||test_vlan(6)==false)
	{return;}
	if(test(7)==false||test_vlan(7)==false)
	{return;}

	for(var i=0; i<8; i++)
	{
		var macvalue = MacAddr[i].value.toUpperCase();
		if (macvalue != "")
		{
			for(var j=i+1; j<8; j++)
			{
				if( macvalue==MacAddr[j].value.toUpperCase() )
				{
					res.innerHTML="Invalid MAC address. Please Check.";
					res.style.visibility="visible";
					MacAddr[j].focus();
					return;
				}
			}
		}
	}
	document.mainform.submit();
}
</script>

<body class="body" onload="init_menu();">
	<div class="top">
		<a class="logo" href="./status_device.php">
			<img src="./images/logo.png"/>
		</a>
	</div>
<form enctype="multipart/form-data" action="config_wds.php" id="mainform" name="mainform" method="post">
<div class="container">
	<div class="left">
				<script type="text/javascript">
			createMenu('<?php echo $curr_mode;?>',privilege);
		</script>
	</div>

	<div class="right">
		<div class="righttop">CONFIG - WDS</div>
		<div class="rightmain">
			<table class="tablemain">
				<tr>
					<td width="5%"></td>
					<td width="10%">WDS</td>
					<td width="25%">MAC Address</td>
					<td width="25%">Passphrase</td>
					<td>VLAN</td>
				</tr>
				<tr>
					<td><input name="chk_wds0" id="chk_wds0" type="checkbox"  class="checkbox" <?php if($chk0==1) echo "checked=\"checked\""?>/></td>
					<td>WDS0:</td>
					<td><input name="txt_mac0" type="text" id="txt_mac0" class="textbox" value="<?php if($chk0==1) echo $wds0; ?>"/></td>
					<td><input name="txt_pass0" type="text" id="txt_pass0" class="textbox" value="<?php if($chk0==1&&$psk0!="NULL") echo "********";?>"/></td>
					<td><input name="txt_vlan0" style="width:42px" type="text" id="txt_vlan0" class="textbox" value="<?php if($chk0==1) echo htmlspecialchars($vlan0);?>"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds1" id="chk_wds1" type="checkbox"  class="checkbox" <?php if($chk1==1) echo "checked=\"checked\""?>/></td>
					<td>WDS1:</td>
					<td><input name="txt_mac1" type="text" id="txt_mac1" class="textbox" value="<?php if($chk1==1) echo $wds1;?>"/></td>
					<td><input name="txt_pass1" type="text" id="txt_pass1" class="textbox" value="<?php if($chk1==1&&$psk1!="NULL") echo "********";?>"/></td>
					<td><input name="txt_vlan1" style="width:42px" type="text" id="txt_vlan1" class="textbox" value="<?php if($chk1==1) echo htmlspecialchars($vlan1);?>"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds2" id="chk_wds2" type="checkbox"  class="checkbox" <?php if($chk2==1) echo "checked=\"checked\""?>/></td>
					<td>WDS2:</td>
					<td><input name="txt_mac2" type="text" id="txt_mac2" class="textbox"  value="<?php if($chk2==1) echo $wds2;?>"/></td>
					<td><input name="txt_pass2" type="text" id="txt_pass2" class="textbox" value="<?php if($chk2==1&&$psk2!="NULL") echo "********";?>"/></td>
					<td><input name="txt_vlan2" style="width:42px" type="text" id="txt_vlan2" class="textbox" value="<?php if($chk2==1) echo htmlspecialchars($vlan2);?>"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds3" id="chk_wds3" type="checkbox"  class="checkbox" <?php if($chk3==1) echo "checked=\"checked\""?>/></td>
					<td>WDS3:</td>
					<td><input name="txt_mac3" type="text" id="txt_mac3" class="textbox" value="<?php if($chk3==1) echo $wds3;?>"/></td>
					<td><input name="txt_pass3" type="text" id="txt_pass3" class="textbox" value="<?php if($chk3==1&&$psk3!="NULL") echo "********";?>"/></td>
					<td><input name="txt_vlan3" style="width:42px" type="text" id="txt_vlan3" class="textbox" value="<?php if($chk3==1) echo htmlspecialchars($vlan3);?>"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds4" id="chk_wds4" type="checkbox"  class="checkbox" <?php if($chk4==1) echo "checked=\"checked\""?>/></td>
					<td>WDS4:</td>
					<td><input name="txt_mac4" type="text" id="txt_mac4" class="textbox" value="<?php if($chk4==1) echo $wds4;?>"/></td>
					<td><input name="txt_pass4" type="text" id="txt_pass4" class="textbox" value="<?php if($chk4==1&&$psk4!="NULL") echo "********";?>"/></td>
					<td><input name="txt_vlan4" style="width:42px" type="text" id="txt_vlan4" class="textbox" value="<?php if($chk4==1) echo htmlspecialchars($vlan4);?>"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds5" id="chk_wds5" type="checkbox"  class="checkbox" <?php if($chk5==1) echo "checked=\"checked\""?>/></td>
					<td>WDS5:</td>
					<td><input name="txt_mac5" type="text" id="txt_mac5" class="textbox" value="<?php if($chk5==1) echo $wds5;?>"/></td>
					<td><input name="txt_pass5" type="text" id="txt_pass5" class="textbox" value="<?php if($chk5==1&&$psk5!="NULL") echo "********";?>"/></td>
					<td><input name="txt_vlan5" style="width:42px" type="text" id="txt_vlan5" class="textbox" value="<?php if($chk5==1) echo htmlspecialchars($vlan5);?>"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds6" id="chk_wds6" type="checkbox"  class="checkbox" <?php if($chk6==1) echo "checked=\"checked\""?>/></td>
					<td>WDS6:</td>
					<td><input name="txt_mac6" type="text" id="txt_mac6" class="textbox" value="<?php if($chk6==1) echo $wds6;?>"/></td>
					<td><input name="txt_pass6" type="text" id="txt_pass6" class="textbox" value="<?php if($chk6==1&&$psk6!="NULL") echo "********";?>"/></td>
					<td><input name="txt_vlan6" style="width:42px" type="text" id="txt_vlan6" class="textbox" value="<?php if($chk6==1) echo htmlspecialchars($vlan6);?>"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds7" id="chk_wds7" type="checkbox"  class="checkbox" <?php if($chk7==1) echo "checked=\"checked\""?>/></td>
					<td>WDS7:</td>
					<td><input name="txt_mac7" type="text" id="txt_mac7" class="textbox" value="<?php if($chk7==1) echo $wds7;?>"/></td>
					<td><input name="txt_pass7" type="text" id="txt_pass7" class="textbox" value="<?php if($chk7==1&&$psk7!="NULL") echo "********";?>"/></td>
					<td><input name="txt_vlan7" style="width:42px" type="text" id="txt_vlan7" class="textbox" value="<?php if($chk7==1) echo htmlspecialchars($vlan7);?>"/></td>
				</tr>
				<tr>
					<td class="divline" style="background:url(/images/divline.png);" colspan="5";></td>
				</tr>
			</table>
			<div id="result" style="visibility:hidden; text-align:left; margin-left:20px; margin-top:20px; font:16px Calibri, Candara, corbel, "Franklin Gothic Book";">
			</div>
			<div class="rightbottom">
				<button name="btn_save" id="btn_save" type="button" onclick="validate();"  class="button">Save</button>
				<button name="btn_cancel" id="btn_cancel" type="button" onclick="reload();"  class="button">Cancel</button>
			</div>
			<input id="action" name="action" type="hidden" value="1">
		</div>
	</div>
</div>
<input type="hidden" name="csrf_token" value="<?php echo get_session_token(); ?>" />
</form>
<div class="bottom">
	<a href="help/aboutus.php">About Quantenna</a> |  <a href="help/contactus.php">Contact Us</a> | <a href="help/privacypolicy.php">Privacy Policy</a> | <a href="help/terms.php">Terms of Use</a> | <a href="help/h_wds.php">Help</a><br />
	<div><?php echo $str_copy ?></div>
</div>

</body>
</html>

