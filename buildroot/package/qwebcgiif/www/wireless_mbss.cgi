#!/usr/bin/haserl
Content-type: text/html


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
<script language="javascript" type="text/javascript" src="./js/common_js.js"></script>

<script language="javascript" type="text/javascript">
var privilege=get_privilege(2);
</script>

<%
source ./common_sh.sh

curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`

if [ -n "$curr_mode" -a "$curr_mode" = "Station" ]
then
	echo "<script langauge=\"javascript\">alert(\"Don\`t support in the Station mode.\");</script>"
	echo "<script language='javascript'>location.href='status_device.cgi'</script>"
	return
fi

#Check the revision number, only available for v32XX<br />
#To do
#Add code here


file="/mnt/jffs2/vlan_config.txt"
chk1=1
chk2=1
chk3=1
chk4=1
chk5=1
chk6=1
chk7=1
ssid1=""
ssid2=""
ssid3=""
ssid4=""
ssid5=""
ssid6=""
ssid7=""
proto1="NONE"
proto2="NONE"
proto3="NONE"
proto4="NONE"
proto5="NONE"
proto6="NONE"
proto7="NONE"
psk1=""
psk2=""
psk3=""
psk4=""
psk5=""
psk6=""
psk7=""
br1=0
br2=0
br3=0
br4=0
br5=0
br6=0
br7=0
vlan1=""
vlan2=""
vlan3=""
vlan4=""
vlan5=""
vlan6=""
vlan7=""

func_generate_vlan_config_file(){
	if [ ! -f "$file" ]
	then
		echo "#wifi0:" > $file
		echo "#wifi1:" >> $file
		echo "#wifi2:" >> $file
		echo "#wifi3:" >> $file
		echo "#wifi4:" >> $file
		echo "#wifi5:" >> $file
		echo "#wifi6:" >> $file
		echo "#wifi7:" >> $file
		echo "#wds0:" >> $file
		echo "#wds1:" >> $file
		echo "#wds2:" >> $file
		echo "#wds3:" >> $file
		echo "#wds4:" >> $file
		echo "#wds5:" >> $file
		echo "#wds6:" >> $file
		echo "#wds7:" >> $file
		chmod 777 $file
	fi
}

func_generate_vlan_config_file

func_escape_any_characters(){
	echo $1 | sed 's/\\/\\\\/g' | sed 's/\x60/\\\x60/g'| sed 's/\$/\\\$/g' | sed 's/\"/\\\"/g' > /tmp/123test
	rval_func_escape_any_characters=`cat /tmp/123test`
}


#only $1 for func_get_proto <device>
func_get_proto(){
	tmp_for_func_get_proto=`call_qcsapi get_ssid $1`
	if [ $? -ne 0 ]
	then
		rval_func_get_proto="NONE"
	else
		beacon=`call_qcsapi get_beacon $1`
		if [ "$beacon" = "Basic" ]
		then
			rval_func_get_proto="NONE"
		else
			encryption=`call_qcsapi get_WPA_encryption_modes $1`
			if [ "$beacon" = "WPA" -a "$encryption" = "TKIPEncryption" ]
			then
				rval_func_get_proto="TKIP"
			elif [ "$beacon" = "11i" -a "$encryption" = "AESEncryption" ]
			then
				rval_func_get_proto="AES"
			elif [ "$beacon" = "WPAand11i" -a "$encryption" = "TKIPandAESEncryption" ]
			then
				rval_func_get_proto="TKIPandAES"
			else
				rval_func_get_proto="error"
			fi
		fi
	fi
}

#$1 device $2 proto
func_set_proto(){
	if [ "$2" = "NONE" ]
	then
		res=`call_qcsapi set_beacon $1 Basic`
		res=`call_qcsapi set_WPA_encryption_modes $1 AESEncryption`
	elif [ "$2" = "TKIP" ]
	then
		res=`call_qcsapi set_beacon $1 WPA`
		res=`call_qcsapi set_WPA_encryption_modes $1 TKIPEncryption`
	elif [ "$2" = "AES" ]
	then
		res=`call_qcsapi set_beacon $1 11i`
		res=`call_qcsapi set_WPA_encryption_modes $1 AESEncryption`
	elif [ "$2" = "TKIPandAES" ]
	then
		res=`call_qcsapi set_beacon $1 WPAand11i`
		res=`call_qcsapi set_WPA_encryption_modes $1 TKIPandAESEncryption`
	else
		return 1
	fi
}

#$1 device
func_get_psk(){
	tmp_for_func_get_psk=`call_qcsapi get_ssid $1`
	if [ $? -ne 0 ]
	then
		rval_func_get_psk=""
	else
		rval_func_get_psk=`call_qcsapi get_passphrase $1 0`
		if [ $? -ne 0 ]
		then
			rval_func_get_psk=`call_qcsapi get_pre_shared_key $1 0`
		fi
	fi
}
#$1 device
func_get_broad(){
	tmp_for_func_get_broad=`call_qcsapi get_ssid $1`
	if [ $? -ne 0 ]
	then
		rval_func_get_broad=0
	else
		tmp_for_func_get_broad=`call_qcsapi get_option $1 SSID_broadcast`
		if [ "$tmp_for_func_get_broad" = "TRUE" ]
		then
			rval_func_get_broad=1
		else
			rval_func_get_broad=0
		fi
	fi
}


#================Load Value======================
func_get_value(){

	ssid1=`call_qcsapi get_ssid wifi1`
	if [ $? -ne 0 ]
	then
		ssid1="N/A"
		chk1=0
	else
		func_get_proto wifi1
		proto1=$rval_func_get_proto
		func_get_psk wifi1
		psk1=$rval_func_get_psk
		func_get_broad wifi1
		br1=$rval_func_get_broad
		vlan1=`cat $file | grep -v "#" | grep wifi1 | awk -F ':' '{print $2}'`
	fi

	ssid2=`call_qcsapi get_ssid wifi2`
	if [ $? -ne 0 ]
	then
		ssid2="N/A"
		chk2=0
	else
		func_get_proto wifi2
		proto2=$rval_func_get_proto
		func_get_psk wifi2
		psk2=$rval_func_get_psk
		func_get_broad wifi2
		br2=$rval_func_get_broad
		vlan2=`cat $file | grep -v "#" | grep wifi2 | awk -F ':' '{print $2}'`
	fi

	ssid3=`call_qcsapi get_ssid wifi3`
	if [ $? -ne 0 ]
	then
		ssid3="N/A"
		chk3=0
	else
		func_get_proto wifi3
		proto3=$rval_func_get_proto
		func_get_psk wifi3
		psk3=$rval_func_get_psk
		func_get_broad wifi3
		br3=$rval_func_get_broad
		vlan3=`cat $file | grep -v "#" | grep wifi3 | awk -F ':' '{print $2}'`
	fi

	ssid4=`call_qcsapi get_ssid wifi4`
	if [ $? -ne 0 ]
	then
		ssid4="N/A"
		chk4=0
	else
		func_get_proto wifi4
		proto4=$rval_func_get_proto
		func_get_psk wifi4
		psk4=$rval_func_get_psk
		func_get_broad wifi4
		br4=$rval_func_get_broad
		vlan4=`cat $file | grep -v "#" | grep wifi4 | awk -F ':' '{print $2}'`
	fi

	ssid5=`call_qcsapi get_ssid wifi5`
	if [ $? -ne 0 ]
	then
		ssid5="N/A"
		chk5=0
	else
		func_get_proto wifi5
		proto5=$rval_func_get_proto
		func_get_psk wifi5
		psk5=$rval_func_get_psk
		func_get_broad wifi5
		br5=$rval_func_get_broad
		vlan5=`cat $file | grep -v "#" | grep wifi5 | awk -F ':' '{print $2}'`
	fi

	ssid6=`call_qcsapi get_ssid wifi6`
	if [ $? -ne 0 ]
	then
		ssid6="N/A"
		chk6=0
	else
		func_get_proto wifi6
		proto6=$rval_func_get_proto
		func_get_psk wifi6
		psk6=$rval_func_get_psk
		func_get_broad wifi6
		br6=$rval_func_get_broad
		vlan6=`cat $file | grep -v "#" | grep wifi6 | awk -F ':' '{print $2}'`
	fi

	ssid7=`call_qcsapi get_ssid wifi7`
	if [ $? -ne 0 ]
	then
		ssid7="N/A"
		chk7=0
	else
		func_get_proto wifi7
		proto7=$rval_func_get_proto
		func_get_psk wifi7
		psk7=$rval_func_get_psk
		func_get_broad wifi7
		br7=$rval_func_get_broad
		vlan7=`cat $file | grep -v "#" | grep wifi7 | awk -F ':' '{print $2}'`
	fi

	vlan_id0=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n1 | tail -n1 `
	vlan_id1=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n2 | tail -n1 `
	vlan_id2=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n3 | tail -n1 `
	vlan_id3=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n4 | tail -n1 `
	vlan_id4=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n5 | tail -n1 `
	vlan_id5=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n6 | tail -n1 `
	vlan_id6=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n7 | tail -n1 `
	vlan_id7=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n8 | tail -n1 `
	vlan_id8=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n9 | tail -n1 `
	vlan_id9=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n10 | tail -n1 `
	vlan_id10=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n11 | tail -n1 `
	vlan_id11=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n12 | tail -n1 `
	vlan_id12=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n13 | tail -n1 `
	vlan_id13=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n14 | tail -n1 `
	vlan_id14=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n15 | tail -n1 `
	vlan_id15=`cat /mnt/jffs2/vlan_config.txt | cut -d ":" -f2 | head -n16 | tail -n1 `
	
}
###################set value#############################
#$device,$old_chk,$new_chk,$old_ssid,$new_ssid,$old_proto,$new_proto,$old_psk,$new_psk,$old_br,$new_br,$old_vlan,$new_vlan
#$1		 $2		  $3		$4		  $5        $6			$7			$8		$9		${10}	${11}	${12}		${13}
func_set_value(){

#confirm "$# --- !!!! 1:$1 2:$2 3:$3 4;$4 5:$5 6;$6 7;$7 8;$8 9;$9 10;${10} 11;${11}"
	if [ "$3" != "$2" ]
	then
		if [ $2 -eq 1 ]
		then
			if [ -n "${12}" ]
			then
				res=`call_qcsapi vlan_config $1 unbind ${12}`
				res=`cat $file | sed "s/$1:${12}/#$1:/g" > /tmp/vlan_txt `
				res=`cat /tmp/vlan_txt > $file`
			fi
			res=`call_qcsapi wifi_remove_bss $1`
		else
			res=`call_qcsapi wifi_create_bss $1`
			res=`call_qcsapi set_SSID $1 "$5"`
			func_set_proto $1 $7
			res=`call_qcsapi set_passphrase $1 0 "$9"`
			if [ $? -ne 0 ]
			then
				res=`call_qcsapi set_pre_shared_key $1 0 "$9"`
			fi
			res=`call_qcsapi set_option $1 SSID_broadcast ${11}`
			if [ -n "${13}" ]
			then
				res=`call_qcsapi vlan_config $1 bind ${13}`
				res=`cat $file | sed "s/#$1:/$1:${13}/g" > /tmp/vlan_txt `
				res=`cat /tmp/vlan_txt > $file`
			fi
		fi
	elif [ "$3" -eq "$2" -a "$3" -eq 1 ]
	then
		if [ "$4" != "$5" ]
		then
			res=`call_qcsapi set_SSID $1 "$5"`
		fi

		if [ "$6" != "$7" ]
		then
			func_set_proto $1 $7
		fi

		if [ "$8" != "$9" ]
		then
			res=`call_qcsapi set_passphrase $1 0 "$9"`
			if [ $? -ne 0 ]
			then
				res=`call_qcsapi set_pre_shared_key $1 0 "$9"`
			fi
		fi

		if [ "${10}" != "${11}" ]
		then
			res=`call_qcsapi set_option $1 SSID_broadcast ${11}`
		fi

		if [ "${12}" != "${13}" ]
		then
			if [ ! -n "${13}" ]
			then
				res=`call_qcsapi vlan_config $1 unbind ${12}`
				res=`cat $file | sed "s/$1:${12}/#$1:/g" > /tmp/vlan_txt `
				res=`cat /tmp/vlan_txt > $file`
			else
				if [ ! -n "${12}" ]
				then
					res=`call_qcsapi vlan_config $1 bind ${13}`
					res=`cat $file | sed "s/#$1:/$1:${13}/g" > /tmp/vlan_txt `
					res=`cat /tmp/vlan_txt > $file`
				else
					res=`qvlan unbind $1 ${12}`
					res=`qvlan bind $1 ${13}`
					res=`cat $file | sed "s/$1:${12}/$1:${13}/g" > /tmp/vlan_txt `
					res=`cat /tmp/vlan_txt > $file`
				fi
			fi
		fi
	fi
}
#=====================================================

func_get_value

if [ -n "$POST_action" ]
then
	new_chk1=$POST_p_chk_bss1
	new_ssid1=$POST_p_txt_ssid1
	new_proto1=$POST_p_cmb_proto1
	new_psk1=$POST_p_txt_psk1
	new_br1=$POST_p_chk_br1
	new_vlan1=$POST_p_txt_vlan1
	new_chk2=$POST_p_chk_bss2
	new_ssid2=$POST_p_txt_ssid2
	new_proto2=$POST_p_cmb_proto2
	new_psk2=$POST_p_txt_psk2
	new_br2=$POST_p_chk_br2
	new_vlan2=$POST_p_txt_vlan2
	new_chk3=$POST_p_chk_bss3
	new_ssid3=$POST_p_txt_ssid3
	new_proto3=$POST_p_cmb_proto3
	new_psk3=$POST_p_txt_psk3
	new_br3=$POST_p_chk_br3
	new_vlan3=$POST_p_txt_vlan3
	new_chk4=$POST_p_chk_bss4
	new_ssid4=$POST_p_txt_ssid4
	new_proto4=$POST_p_cmb_proto4
	new_psk4=$POST_p_txt_psk4
	new_br4=$POST_p_chk_br4
	new_vlan4=$POST_p_txt_vlan4
	new_chk5=$POST_p_chk_bss5
	new_ssid5=$POST_p_txt_ssid5
	new_proto5=$POST_p_cmb_proto5
	new_psk5=$POST_p_txt_psk5
	new_br5=$POST_p_chk_br5
	new_vlan5=$POST_p_txt_vlan4
	new_chk6=$POST_p_chk_bss6
	new_ssid6=$POST_p_txt_ssid6
	new_proto6=$POST_p_cmb_proto6
	new_psk6=$POST_p_txt_psk6
	new_br6=$POST_p_chk_br6
	new_vlan6=$POST_p_txt_vlan6
	new_chk7=$POST_p_chk_bss7
	new_ssid7=$POST_p_txt_ssid7
	new_proto7=$POST_p_cmb_proto7
	new_psk7=$POST_p_txt_psk7
	new_br7=$POST_p_chk_br7
	new_vlan7=$POST_p_txt_vlan7

	#confirm "chk1:$chk1--new_chk1:$new_chk1--ssid1:$ssid1--new_ssid1:$new_ssid1--proto1:$proto1--new_proto1:$new_proto1--psk1:$psk1--new_psk1:$new_psk1--br1:$br1--new_br1:$new_br1"

	func_set_value "wifi1" "$chk1" "$new_chk1" "$ssid1" "$new_ssid1" "$proto1" "$new_proto1" "$psk1" "$new_psk1" "$br1" "$new_br1" "$vlan1" "$new_vlan1"
	func_set_value "wifi2" "$chk2" "$new_chk2" "$ssid2" "$new_ssid2" "$proto2" "$new_proto2" "$psk2" "$new_psk2" "$br2" "$new_br2" "$vlan2" "$new_vlan2"
	func_set_value "wifi3" "$chk3" "$new_chk3" "$ssid3" "$new_ssid3" "$proto3" "$new_proto3" "$psk3" "$new_psk3" "$br3" "$new_br3" "$vlan3" "$new_vlan3"
	func_set_value "wifi4" "$chk4" "$new_chk4" "$ssid4" "$new_ssid4" "$proto4" "$new_proto4" "$psk4" "$new_psk4" "$br4" "$new_br4" "$vlan4" "$new_vlan4"
	func_set_value "wifi5" "$chk5" "$new_chk5" "$ssid5" "$new_ssid5" "$proto5" "$new_proto5" "$psk5" "$new_psk5" "$br5" "$new_br5" "$vlan5" "$new_vlan5"
	func_set_value "wifi6" "$chk6" "$new_chk6" "$ssid6" "$new_ssid6" "$proto6" "$new_proto6" "$psk6" "$new_psk6" "$br6" "$new_br6" "$vlan6" "$new_vlan6"
	func_set_value "wifi7" "$chk7" "$new_chk7" "$ssid7" "$new_ssid7" "$proto7" "$new_proto7" "$psk7" "$new_psk7" "$br7" "$new_br7" "$vlan7" "$new_vlan7"

	echo "<script language='javascript'>location.href='wireless_mbss.cgi'</script>"
fi

%>
<script type="text/javascript">

function reload()
{
	window.location.href="config_mbssid.php";
}

function disablePsk(bssid)
{
	var cmbproto="cmb_proto"+bssid;
	var txtpsk="txt_psk"+bssid;
	var cmbproto1=document.getElementById(cmbproto);
	var txtpsk1=document.getElementById(txtpsk);
	if (cmbproto1.selectedIndex==0)
	{
		set_disabled(txtpsk,true);
	}
	else
	{
		set_disabled(txtpsk,false);
	}
}

function test(bssid)
{
	nonhex = /[^A-Fa-f0-9]/g;
	nonascii = /[^\x20-\x7E]/;
	var chk="chk_bss"+bssid;
	var txtssid="txt_ssid"+bssid;
	var cmbproto="cmb_proto"+bssid;
	var txtpsk="txt_psk"+bssid;
	var chk1=document.getElementById(chk);
	var txtssid1=document.getElementById(txtssid);
	txtssid1.value=txtssid1.value.replace(/(\")/g, '\"');
	var cmbproto1=document.getElementById(cmbproto);
	var txtpsk1=document.getElementById(txtpsk);
	txtpsk1.value=txtpsk1.value.replace(/(\")/g, '\"');
	if (chk1.checked==true)
	{
		if (txtssid1.value.length < 1 || txtssid1.value.length > 32)
		{
			txtssid1.focus();
			alert("SSID must contain between 1 and 32 ASCII characters");
			return false;
		}
		if ((nonascii.test(txtssid1.value)))
		{
			txtssid1.focus();
			alert("Only ASCII characters allowed in SSID");
			return false;
		}
		if (cmbproto1.selectedIndex > 0)
		{
			if (txtpsk1.value.length < 8 || txtpsk1.value.length > 64)
			{
				txtpsk1.focus();
				alert("Passphrase must contain between 8 and 64 ASCII characters");
				return false;
			}
			if ((nonascii.test(txtpsk1.value)))
			{
				txtpsk1.focus();
				alert("Only ASCII characters allowed");
				return false;
			}
			if(txtpsk1.value.length == 64 && (nonhex.test(txtpsk1.value)))
			{
				txtpsk1.focus();
				alert("Passkey must contain 64 hexadecimal (0-9, A-F) characters");
				return false;
			}
		}
	}
	return true;
}

function test_vlan(bssid)
{
	var no_num= /[^0-9]/ ;
	var txtssid_id="txt_ssid"+bssid;
	var txt_vlan_id="txt_vlan"+bssid;
	var txt_vlan=document.getElementById(txt_vlan_id);
	var txtssid=document.getElementById(txtssid_id);
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

	vlan_id[0]="<% echo -n $vlan_id0 %>";
	vlan_id[1]="<% echo -n $vlan_id1 %>";
	vlan_id[2]="<% echo -n $vlan_id2 %>";
	vlan_id[3]="<% echo -n $vlan_id3 %>";
	vlan_id[4]="<% echo -n $vlan_id4 %>";
	vlan_id[5]="<% echo -n $vlan_id5 %>";
	vlan_id[6]="<% echo -n $vlan_id6 %>";
	vlan_id[7]="<% echo -n $vlan_id7 %>";
	vlan_id[8]="<% echo -n $vlan_id8 %>";
	vlan_id[9]="<% echo -n $vlan_id9 %>";
	vlan_id[10]="<% echo -n $vlan_id10 %>";
	vlan_id[11]="<% echo -n $vlan_id11 %>";
	vlan_id[12]="<% echo -n $vlan_id12 %>";
	vlan_id[13]="<% echo -n $vlan_id13 %>";
	vlan_id[14]="<% echo -n $vlan_id14 %>";
	vlan_id[15]="<% echo -n $vlan_id15 %>";


	for (x=1; x<bssid;x++)
	{
		if(vlan_id[x]!=""&&txt_vlan.value==vlan_id[x])
		{
			txt_vlan.focus();
			res.innerHTML="Vlan ID has been used. Please enter a different Vlan ID";
			res.style.visibility="visible";
			return false;
		}
	}
	for (x=bssid+1; x<16;x++)
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
	if (test(1)==false||test_vlan(1)==false) 
	{return false;}
	if (test(2)==false||test_vlan(2)==false) 
	{return false;}
	if (test(3)==false||test_vlan(3)==false) 
	{return false;}
	if (test(4)==false||test_vlan(4)==false) 
	{return false;}
	if (test(5)==false||test_vlan(5)==false) 
	{return false;}
	if (test(6)==false||test_vlan(6)==false) 
	{return false;}
	if (test(7)==false||test_vlan(7)==false) 
	{return false;}

	var pf=document.wireless_mbss;
	var mf=document.mainform;

	if (document.getElementById('chk_bss1').checked) {
		pf.p_chk_bss1.value=1
	}
	else{
		pf.p_chk_bss1.value=0
	}

	if (document.getElementById('chk_br1').checked) {
		pf.p_chk_br1.value=1
	}
	else{
		pf.p_chk_br1.value=0
	}
	pf.p_txt_ssid1.value=mf.txt_ssid1.value;
	pf.p_cmb_proto1.value=mf.cmb_proto1.value;
	pf.p_txt_psk1.value=mf.txt_psk1.value;
	pf.p_txt_vlan1.value=mf.txt_vlan1.value;

	if (document.getElementById('chk_bss2').checked) {
		pf.p_chk_bss2.value=1
	}
	else{
		pf.p_chk_bss2.value=0
	}

	if (document.getElementById('chk_br2').checked) {
		pf.p_chk_br2.value=1
	}
	else{
		pf.p_chk_br2.value=0
	}
	pf.p_txt_ssid2.value=mf.txt_ssid2.value;
	pf.p_cmb_proto2.value=mf.cmb_proto2.value;
	pf.p_txt_psk2.value=mf.txt_psk2.value;
	pf.p_txt_vlan2.value=mf.txt_vlan2.value;

	if (document.getElementById('chk_bss3').checked) {
		pf.p_chk_bss3.value=1
	}
	else{
		pf.p_chk_bss3.value=0
	}

	if (document.getElementById('chk_br3').checked) {
		pf.p_chk_br3.value=1
	}
	else{
		pf.p_chk_br3.value=0
	}
	pf.p_txt_ssid3.value=mf.txt_ssid3.value;
	pf.p_cmb_proto3.value=mf.cmb_proto3.value;
	pf.p_txt_psk3.value=mf.txt_psk3.value;
	pf.p_txt_vlan3.value=mf.txt_vlan3.value;

	if (document.getElementById('chk_bss4').checked) {
		pf.p_chk_bss4.value=1
	}
	else{
		pf.p_chk_bss4.value=0
	}

	if (document.getElementById('chk_br4').checked) {
		pf.p_chk_br4.value=1
	}
	else{
		pf.p_chk_br4.value=0
	}
	pf.p_txt_ssid4.value=mf.txt_ssid4.value;
	pf.p_cmb_proto4.value=mf.cmb_proto4.value;
	pf.p_txt_psk4.value=mf.txt_psk4.value;
	pf.p_txt_vlan4.value=mf.txt_vlan4.value;

	if (document.getElementById('chk_bss5').checked) {
		pf.p_chk_bss5.value=1
	}
	else{
		pf.p_chk_bss5.value=0
	}

	if (document.getElementById('chk_br5').checked) {
		pf.p_chk_br5.value=1
	}
	else{
		pf.p_chk_br5.value=0
	}
	pf.p_txt_ssid5.value=mf.txt_ssid5.value;
	pf.p_cmb_proto5.value=mf.cmb_proto5.value;
	pf.p_txt_psk5.value=mf.txt_psk5.value;
	pf.p_txt_vlan5.value=mf.txt_vlan5.value;

	if (document.getElementById('chk_bss6').checked) {
		pf.p_chk_bss6.value=1
	}
	else{
		pf.p_chk_bss6.value=0
	}

	if (document.getElementById('chk_br6').checked) {
		pf.p_chk_br6.value=1
	}
	else{
		pf.p_chk_br6.value=0
	}
	pf.p_txt_ssid6.value=mf.txt_ssid6.value;
	pf.p_cmb_proto6.value=mf.cmb_proto6.value;
	pf.p_txt_psk6.value=mf.txt_psk6.value;
	pf.p_txt_vlan6.value=mf.txt_vlan6.value;

	if (document.getElementById('chk_bss7').checked) {
		pf.p_chk_bss7.value=1
	}
	else{
		pf.p_chk_bss7.value=0
	}

	if (document.getElementById('chk_br7').checked) {
		pf.p_chk_br7.value=1
	}
	else{
		pf.p_chk_br7.value=0
	}
	pf.p_txt_ssid7.value=mf.txt_ssid7.value;
	pf.p_cmb_proto7.value=mf.cmb_proto7.value;
	pf.p_txt_psk7.value=mf.txt_psk7.value;
	pf.p_txt_vlan7.value=mf.txt_vlan7.value;

	pf.submit();
}

function load_value()
{
	init_menu();
	set_control_value('chk_bss1','<% echo -n $chk1 %>', 'checkbox');
	set_control_value('cmb_proto1','<% echo -n $proto1 %>', 'combox');
	set_control_value('chk_br1','<% echo -n $br1 %>', 'checkbox');
	var test_for_proto="<% echo -n $proto1 %>"
	if (test_for_proto=="NONE") {
		document.getElementById('txt_psk1').disabled="disabled"
	}


	set_control_value('chk_bss2','<% echo -n $chk2 %>', 'checkbox');
	set_control_value('cmb_proto2','<% echo -n $proto2 %>', 'combox');
	set_control_value('chk_br2','<% echo -n $br2 %>', 'checkbox');
	var test_for_proto="<% echo -n $proto2 %>"
	if (test_for_proto=="NONE") {
		document.getElementById('txt_psk2').disabled="disabled"
	}

	set_control_value('chk_bss3','<% echo -n $chk3 %>', 'checkbox');
	set_control_value('cmb_proto3','<% echo -n $proto3 %>', 'combox');
	set_control_value('chk_br3','<% echo -n $br3 %>', 'checkbox');
	var test_for_proto="<% echo -n $proto3 %>"
	if (test_for_proto=="NONE") {
		document.getElementById('txt_psk3').disabled="disabled"
	}

	set_control_value('chk_bss4','<% echo -n $chk4 %>', 'checkbox');
	set_control_value('cmb_proto4','<% echo -n $proto4 %>', 'combox');
	set_control_value('chk_br4','<% echo -n $br4 %>', 'checkbox');
	var test_for_proto="<% echo -n $proto4 %>"
	if (test_for_proto=="NONE") {
		document.getElementById('txt_psk4').disabled="disabled"
	}

	set_control_value('chk_bss5','<% echo -n $chk5 %>', 'checkbox');
	set_control_value('cmb_proto5','<% echo -n $proto5 %>', 'combox');
	set_control_value('chk_br5','<% echo -n $br5 %>', 'checkbox');
	var test_for_proto="<% echo -n $proto5 %>"
	if (test_for_proto=="NONE") {
		document.getElementById('txt_psk5').disabled="disabled"
	}

	set_control_value('chk_bss6','<% echo -n $chk6 %>', 'checkbox');
	set_control_value('cmb_proto6','<% echo -n $proto6 %>', 'combox');
	set_control_value('chk_br6','<% echo -n $br6 %>', 'checkbox');
	var test_for_proto="<% echo -n $proto6 %>"
	if (test_for_proto=="NONE") {
		document.getElementById('txt_psk6').disabled="disabled"
	}

	set_control_value('chk_bss7','<% echo -n $chk7 %>', 'checkbox');
	set_control_value('cmb_proto7','<% echo -n $proto7 %>', 'combox');
	set_control_value('chk_br7','<% echo -n $br7 %>', 'checkbox');
	var test_for_proto="<% echo -n $proto7 %>"
	if (test_for_proto=="NONE") {
		document.getElementById('txt_psk7').disabled="disabled"
	}
}

function reload()
{
	window.location.href="wireless_mbss.cgi";
}
</script>
<!--script part-->

<body class="body" onload="load_value();">
	<div class="top">
		<script type="text/javascript">
			createTop('<% echo -n $curr_version %>','<% echo -n $curr_mode %>');
		</script>
	</div>
<div class="container">
	<div class="left">
		<script type="text/javascript">
			createMenu('<% echo -n $curr_mode %>',privilege);
		</script>
	</div>
	<div class="right">
		<div class="righttop">WIRELESS - MBSS</div>
		<div class="rightmain">
		<form name="mainform" >
			<table class="tablemain">
				<tr>
					<td width="5%"></td>
					<td width="4%">BSS</td>
					<td width="25%">&nbsp;&nbsp;SSID</td>
					<td width="25%">Encryption</td>
					<td width="20%">Passphrase</td>
					<td width="5%">Broadcast</td>
					<td width="16%">VLAN</td>
				</tr>
				<tr>
				<td><input name="chk_bss1" id="chk_bss1" type="checkbox"  class="checkbox" /></td>
					<td>&nbsp;&nbsp;1:</td>
					<td><input name="txt_ssid1" type="text" id="txt_ssid1" class="textbox" value="<% if [ "$chk1" -eq 1 ]; then echo -n "$ssid1" | sed -e 's/\"/\&\#34;/g' ; fi %>"/></td>
					<td>
					<select name="cmb_proto1" class="combox" id="cmb_proto1" onchange="disablePsk(1);">
					<option  value="NONE"> NONE-OPEN </option>
					<option  value="AES"> WPA2-AES </option>
					<option  value="TKIPandAES"> WPA2 + WPA (mixed mode) </option>
					</select>
					</td>
					<td><input name="txt_psk1" type="text" id="txt_psk1" class="textbox" style="width:120px" value="<% if [ "$chk1" -eq 1 -a "$proto1" != "NONE" ]; then echo -n "$psk1" | sed -e 's/\"/\&\#34;/g' ; fi %>" /></td>
					<td align="center"><input name="chk_br1" id="chk_br1" type="checkbox"  class="chk_br1" /></td>
					<td><input name="txt_vlan1" style="width:42px" type="text" id="txt_vlan1" class="textbox"  value="<% if [ "$chk1" -eq 1 ]; then echo -n "$vlan1" ; fi %>"/></td>
				</tr>
				<tr>
					<td><input name="chk_bss2" id="chk_bss2" type="checkbox"  class="checkbox" /></td>
					<td>&nbsp;&nbsp;2:</td>
					<td><input name="txt_ssid2" type="text" id="txt_ssid2" class="textbox" value="<% if [ "$chk2" -eq 1 ]; then echo -n "$ssid2" | sed -e 's/\"/\&\#34;/g' ; fi %>" /></td>
					<td>
					<select name="cmb_proto2" class="combox" id="cmb_proto2" onchange="disablePsk(2);">
						<option value="NONE"> NONE-OPEN </option>
						<option value="AES"> WPA2-AES </option>
						<option value="TKIPandAES"> WPA2 + WPA (mixed mode) </option>
					</select>
					</td>
					<td><input name="txt_psk2" type="text" id="txt_psk2" class="textbox" style="width:120px" value="<% if [ "$chk2" -eq 1 -a "$proto2" != "NONE" ]; then echo -n "$psk2" | sed -e 's/\"/\&\#34;/g' ; fi %>"/></td>
					<td align="center"><input name="chk_br2" id="chk_br2" type="checkbox"  class="chk_br2" /></td>
					<td><input name="txt_vlan2" style="width:42px" type="text" id="txt_vlan2" class="textbox" value="<% if [ "$chk2" -eq 1 ]; then echo -n "$vlan2" ; fi %>"/></td>
				</tr>
				<tr>
					<td><input name="chk_bss3" id="chk_bss3" type="checkbox"  class="checkbox" /></td>
					<td>&nbsp;&nbsp;3:</td>
					<td><input name="txt_ssid3" type="text" id="txt_ssid3" class="textbox" value="<% if [ "$chk3" -eq 1 ]; then echo -n "$ssid3" | sed -e 's/\"/\&\#34;/g' ; fi %>" /></td>
					<td>
					<select name="cmb_proto3" class="combox" id="cmb_proto3" onchange="disablePsk(3)">
					<option value="NONE"> NONE-OPEN </option>
					<option value="AES"> WPA2-AES </option>
					<option value="TKIPandAES"> WPA2 + WPA (mixed mode) </option>
					</select>
					</td>
					<td><input name="txt_psk3" type="text" id="txt_psk3" class="textbox" style="width:120px" value="<% if [ "$chk3" -eq 1 -a "$proto3" != "NONE" ]; then echo -n "$psk3" | sed -e 's/\"/\&\#34;/g' ; fi %>"/></td>
					<td align="center"><input name="chk_br3" id="chk_br3" type="checkbox"  class="chk_br3" /></td>
					<td><input name="txt_vlan3" style="width:42px" type="text" id="txt_vlan3" class="textbox" value="<% if [ "$chk3" -eq 1 ]; then echo -n "$vlan3" ; fi %>"/></td>
				</tr>
				<tr>
					<td><input name="chk_bss4" id="chk_bss4" type="checkbox"  class="checkbox" /></td>
					<td>&nbsp;&nbsp;4:</td>
					<td><input name="txt_ssid4" type="text" id="txt_ssid4" class="textbox" value="<% if [ "$chk4" -eq 1 ]; then echo -n "$ssid4" | sed -e 's/\"/\&\#34;/g' ; fi %>" /></td>
					<td>
					<select name="cmb_proto4" class="combox" id="cmb_proto4" onchange="disablePsk(4);">
					<option value="NONE"> NONE-OPEN </option>
					<option value="AES"> WPA2-AES </option>
					<option value="TKIPandAES"> WPA2 + WPA (mixed mode) </option>
					</select>
					</td>
					<td><input name="txt_psk4" type="text" id="txt_psk4" class="textbox" style="width:120px" value="<% if [ "$chk4" -eq 1 -a "$proto4" != "NONE" ]; then echo -n "$psk4" | sed -e 's/\"/\&\#34;/g' ; fi %>"/></td>
					<td align="center"><input name="chk_br4" id="chk_br4" type="checkbox"  class="chk_br4" /></td>
					<td><input name="txt_vlan4" style="width:42px" type="text" id="txt_vlan4" class="textbox" value="<% if [ "$chk4" -eq 1 ]; then echo -n "$vlan4" ; fi %>"/></td>
				</tr>
				<tr>
					<td><input name="chk_bss5" id="chk_bss5" type="checkbox"  class="checkbox" /></td>
					<td>&nbsp;&nbsp;5:</td>
					<td><input name="txt_ssid5" type="text" id="txt_ssid5" class="textbox" value="<% if [ "$chk5" -eq 1 ]; then echo -n "$ssid5" | sed -e 's/\"/\&\#34;/g' ; fi %>" /></td>
					<td>
					<select name="cmb_proto5" class="combox" id="cmb_proto5" onchange="disablePsk(5);">
					<option value="NONE"> NONE-OPEN </option>
					<option value="AES"> WPA2-AES </option>
					<option value="TKIPandAES"> WPA2 + WPA (mixed mode) </option>
					</select>
					</td>
					<td><input name="txt_psk5" type="text" id="txt_psk5" class="textbox" style="width:120px" value="<% if [ "$chk5" -eq 1 -a "$proto5" != "NONE" ]; then echo -n "$psk5" | sed -e 's/\"/\&\#34;/g' ; fi %>"/></td>
					<td align="center"><input name="chk_br5" id="chk_br5" type="checkbox"  class="chk_br5" /></td>
					<td><input name="txt_vlan5" style="width:42px" type="text" id="txt_vlan5" class="textbox" value="<% if [ "$chk5" -eq 1 ]; then echo -n "$vlan5" ; fi %>"/></td>
				</tr>
				<tr>
					<td><input name="chk_bss6" id="chk_bss6" type="checkbox"  class="checkbox"/></td>
					<td>&nbsp;&nbsp;6:</td>
					<td><input name="txt_ssid6" type="text" id="txt_ssid6" class="textbox" value="<% if [ "$chk6" -eq 1 ]; then echo -n "$ssid6" | sed -e 's/\"/\&\#34;/g' ; fi %>" /></td>
					<td>
					<select name="cmb_proto6" class="combox" id="cmb_proto6" onchange="disablePsk(6);">
					<option  value="NONE"> NONE-OPEN </option>
					<option  value="AES"> WPA2-AES </option>
					<option value="TKIPandAES"> WPA2 + WPA (mixed mode) </option>
					</select>
					</td>
					<td><input name="txt_psk6" type="text" id="txt_psk6" class="textbox" style="width:120px" value="<% if [ "$chk6" -eq 1 -a "$proto6" != "NONE" ]; then echo -n "$psk6" | sed -e 's/\"/\&\#34;/g' ; fi %>"/></td>
					<td align="center"><input name="chk_br6" id="chk_br6" type="checkbox"  class="chk_br6" /></td>
					<td><input name="txt_vlan6" style="width:42px" type="text" id="txt_vlan6" class="textbox" value="<% if [ "$chk6" -eq 1 ]; then echo -n "$vlan6" ; fi %>"/></td>
				</tr>
				<tr>
					<td><input name="chk_bss7" id="chk_bss7" type="checkbox"  class="checkbox" /></td>
					<td>&nbsp;&nbsp;7:</td>
					<td><input name="txt_ssid7" type="text" id="txt_ssid7" class="textbox" value="<% if [ "$chk7" -eq 1 ]; then echo -n "$ssid7" | sed -e 's/\"/\&\#34;/g' ; fi %>"/></td>
					<td>
					<select name="cmb_proto7" class="combox" id="cmb_proto7" onchange="disablePsk(7);">
					<option  value="NONE"> NONE-OPEN </option>
					<option  value="AES"> WPA2-AES </option>
					<option  value="TKIPandAES"> WPA2 + WPA (mixed mode) </option>
					</select>
					</td>
					<td><input name="txt_psk7" type="text" id="txt_psk7" class="textbox" style="width:120px" value="<% if [ "$chk7" -eq 1 -a "$proto7" != "NONE" ]; then echo -n "$psk7" | sed -e 's/\"/\&\#34;/g' ; fi %>"/></td>
					<td align="center"><input name="chk_br7" id="chk_br7" type="checkbox"  class="chk_br7" /></td>
					<td><input name="txt_vlan7" style="width:42px" type="text" id="txt_vlan7" class="textbox" value="<% if [ "$chk7" -eq 1 ]; then echo -n "$vlan7" ; fi %>"/></td>
				</tr>
				<tr>
					<td class="divline" colspan="7";></td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_save" id="btn_save" type="button" onclick="validate();"  class="button">Save</button>
				<button name="btn_cancel" id="btn_cancel" type="button" onclick="reload();"  class="button">Cancel</button>
			</div>
		</div>
	</div>
</div>
</form>
<div class="bottom">
<tr>
	<script type="text/javascript">
	createBot();
	</script>
</tr>
</div>
<form name="wireless_mbss" method="POST" action="wireless_mbss.cgi">
	<input type="hidden" name="action" value="action" />
	<input type="hidden" name="p_chk_bss1" />
	<input type="hidden" name="p_txt_ssid1" />
	<input type="hidden" name="p_cmb_proto1" />
	<input type="hidden" name="p_txt_psk1" />
	<input type="hidden" name="p_chk_br1" />
	<input type="hidden" name="p_txt_vlan1" />

	<input type="hidden" name="p_chk_bss2" />
	<input type="hidden" name="p_txt_ssid2" />
	<input type="hidden" name="p_cmb_proto2" />
	<input type="hidden" name="p_txt_psk2" />
	<input type="hidden" name="p_chk_br2" />
	<input type="hidden" name="p_txt_vlan2" />

	<input type="hidden" name="p_chk_bss3" />
	<input type="hidden" name="p_txt_ssid3" />
	<input type="hidden" name="p_cmb_proto3" />
	<input type="hidden" name="p_txt_psk3" />
	<input type="hidden" name="p_chk_br3" />
	<input type="hidden" name="p_txt_vlan3" />

	<input type="hidden" name="p_chk_bss4" />
	<input type="hidden" name="p_txt_ssid4" />
	<input type="hidden" name="p_cmb_proto4" />
	<input type="hidden" name="p_txt_psk4" />
	<input type="hidden" name="p_chk_br4" />
	<input type="hidden" name="p_txt_vlan4" />

	<input type="hidden" name="p_chk_bss5" />
	<input type="hidden" name="p_txt_ssid5" />
	<input type="hidden" name="p_cmb_proto5" />
	<input type="hidden" name="p_txt_psk5" />
	<input type="hidden" name="p_chk_br5" />
	<input type="hidden" name="p_txt_vlan5" />

	<input type="hidden" name="p_chk_bss6" />
	<input type="hidden" name="p_txt_ssid6" />
	<input type="hidden" name="p_cmb_proto6" />
	<input type="hidden" name="p_txt_psk6" />
	<input type="hidden" name="p_chk_br6" />
	<input type="hidden" name="p_txt_vlan6" />

	<input type="hidden" name="p_chk_bss7" />
	<input type="hidden" name="p_txt_ssid7" />
	<input type="hidden" name="p_cmb_proto7" />
	<input type="hidden" name="p_txt_psk7" />
	<input type="hidden" name="p_chk_br7" />
	<input type="hidden" name="p_txt_vlan7" />
</form>

</body>
</html>
