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
chk0=""
chk1=""
chk2=""
chk3=""
chk4=""
chk5=""
chk6=""
chk7=""
wds0=""
wds1=""
wds2=""
wds3=""
wds4=""
wds5=""
wds6=""
wds7=""
psk0=""
psk1=""
psk2=""
psk3=""
psk4=""
psk5=""
psk6=""
psk7=""
d_psk0=""
d_psk1=""
d_psk2=""
d_psk3=""
d_psk4=""
d_psk5=""
d_psk6=""
d_psk7=""
config_content=""

vlan0=""
vlan1=""
vlan2=""
vlan3=""
vlan4=""
vlan5=""
vlan6=""
vlan7=""
file_vlan="/mnt/jffs2/vlan_config.txt"

if [ -n "$curr_mode" -a "$curr_mode" = "Station" ]
then
	echo "<script langauge=\"javascript\">alert(\"Don\`t support in the Station mode.\");</script>"
	echo "<script language='javascript'>location.href='status_device.cgi'</script>"
	return
fi

func_generate_vlan_config_file(){
	if [ ! -f "$file_vlan" ]
	then
		echo "#wifi0:" > $file_vlan
		echo "#wifi1:" >> $file_vlan
		echo "#wifi2:" >> $file_vlan
		echo "#wifi3:" >> $file_vlan
		echo "#wifi4:" >> $file_vlan
		echo "#wifi5:" >> $file_vlan
		echo "#wifi6:" >> $file_vlan
		echo "#wifi7:" >> $file_vlan
		echo "#wds0:" >> $file_vlan
		echo "#wds1:" >> $file_vlan
		echo "#wds2:" >> $file_vlan
		echo "#wds3:" >> $file_vlan
		echo "#wds4:" >> $file_vlan
		echo "#wds5:" >> $file_vlan
		echo "#wds6:" >> $file_vlan
		echo "#wds7:" >> $file_vlan
		chmod 777 $file_vlan
	fi
}

func_generate_vlan_config_file

get_value()
{
	wds0=`call_qcsapi wds_get_peer_address wifi0 0`
	if [ $? -ne 0 ]
	then
		chk0="0"
		wds0=""
		psk0=""
		d_psk0=""
	else
		chk0="1"
		psk0=`cat /mnt/jffs2/wds_config.txt | grep $wds0 -A 1 | grep psk | awk -F\= '{print $2}'`
		if [ "$psk0" != "NULL" ]
		then
			d_psk0="********"
		else
			d_psk0=""
		fi
		vlan0=`cat $file_vlan | grep -v "#" |grep wds0 | awk -F ':' '{print $2}'`
	fi

	wds1=`call_qcsapi wds_get_peer_address wifi0 1`
	if [ $? -ne 0 ]
	then
		chk1="0"
		wds1=""
		psk1=""
		d_psk1=""
	else
		chk1="1"
		psk1=`cat /mnt/jffs2/wds_config.txt | grep $wds1 -A 1 | grep psk | awk -F\= '{print $2}'`
		if [ "$psk1" != "NULL" ]
		then
			d_psk1="********"
		else
			d_psk1=""
		fi
		vlan1=`cat $file_vlan | grep -v "#" |grep wds1 | awk -F ':' '{print $2}'`
	fi

	wds2=`call_qcsapi wds_get_peer_address wifi0 2`
	if [ $? -ne 0 ]
	then
		chk2="0"
		wds2=""
		psk2=""
		d_psk2=""
	else
		chk2="1"
		psk2=`cat /mnt/jffs2/wds_config.txt | grep $wds2 -A 1 | grep psk | awk -F\= '{print $2}'`
		if [ "$psk2" != "NULL" ]
		then
			d_psk2="********"
		else
			d_psk2=""
		fi
		vlan2=`cat $file_vlan | grep -v "#" |grep wds2 | awk -F ':' '{print $2}'`
	fi

	wds3=`call_qcsapi wds_get_peer_address wifi0 3`
	if [ $? -ne 0 ]
	then
		chk3="0"
		wds3=""
		psk3=""
		d_psk3=""
	else
		chk3="1"
		psk3=`cat /mnt/jffs2/wds_config.txt | grep $wds3 -A 1 | grep psk | awk -F\= '{print $2}'`
		if [ "$psk3" != "NULL" ]
		then
			d_psk3="********"
		else
			d_psk3=""
		fi
		vlan3=`cat $file_vlan | grep -v "#" |grep wds3 | awk -F ':' '{print $2}'`
	fi

	wds4=`call_qcsapi wds_get_peer_address wifi0 4`
	if [ $? -ne 0 ]
	then
		chk4="0"
		wds4=""
		psk4=""
		d_psk4=""
	else
		chk4="1"
		psk4=`cat /mnt/jffs2/wds_config.txt | grep $wds4 -A 1 | grep psk | awk -F\= '{print $2}'`
		if [ "$psk4" != "NULL" ]
		then
			d_psk4="********"
		else
			d_psk4=""
		fi
		vlan4=`cat $file_vlan | grep -v "#" |grep wds4 | awk -F ':' '{print $2}'`
	fi

	wds5=`call_qcsapi wds_get_peer_address wifi0 5`
	if [ $? -ne 0 ]
	then
		chk5="0"
		wds5=""
		psk5=""
		d_psk5=""
	else
		chk5="1"
		psk5=`cat /mnt/jffs2/wds_config.txt | grep $wds5 -A 1 | grep psk | awk -F\= '{print $2}'`
		if [ "$psk5" != "NULL" ]
		then
			d_psk5="********"
		else
			d_psk5=""
		fi
		vlan5=`cat $file_vlan | grep -v "#" |grep wds5 | awk -F ':' '{print $2}'`
	fi

	wds6=`call_qcsapi wds_get_peer_address wifi0 6`
	if [ $? -ne 0 ]
	then
		chk6="0"
		wds6=""
		psk6=""
		d_psk6=""
	else
		chk6="1"
		psk6=`cat /mnt/jffs2/wds_config.txt | grep $wds6 -A 1 | grep psk | awk -F\= '{print $2}'`
		if [ "$psk6" != "NULL" ]
		then
			d_psk6="********"
		else
			d_psk6=""
		fi
		vlan6=`cat $file_vlan | grep -v "#" |grep wds6 | awk -F ':' '{print $2}'`
	fi

	wds7=`call_qcsapi wds_get_peer_address wifi0 7`
	if [ $? -ne 0 ]
	then
		chk7="0"
		wds7=""
		psk7=""
		d_psk7=""
	else
		chk7="1"
		psk7=`cat /mnt/jffs2/wds_config.txt | grep $wds7 -A 1 | grep psk | awk -F\= '{print $2}'`
		if [ "$psk7" != "NULL" ]
		then
			d_psk7="********"
		else
			d_psk7=""
		fi
		vlan7=`cat $file_vlan | grep -v "#" |grep wds7 | awk -F ':' '{print $2}'`
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

#$1		 ,$2      ,$3      ,$4      ,$5      ,$6,		$7			$8		$9
#$old_chk,$new_chk,$old_wds,$new_wds,$new_psk,$old_psk,$old_vlan,$new_vlan,$wds_if
set_single_value()
{
	if [ "$2" != "$1" ]
	then
		if [ "$1" = "1" ]
		then
			if [ -n "$7" ]
			then
				res=`qvlan unbind $9 $7 2>/dev/null`
				res=`cat $file_vlan | sed "s/$9:$7/#$9:/g" > /tmp/vlan_txt `
				res=`cat /tmp/vlan_txt > $file_vlan`
			fi
			res=`call_qcsapi wds_remove_peer wifi0 $3`
		else
			res=`call_qcsapi wds_add_peer wifi0 $4`
			if [ $res != "complete" ]
				then
					confirm "Local Mac address can't be used as wds peer address. Please enter valid Mac address."
					return;
				fi
			if [ "$5" != "" ]
			then
				res=`call_qcsapi wds_set_psk wifi0 $4 $5`
			else
				res=`call_qcsapi wds_set_psk wifi0 $4 NULL`
			fi

			if [ -n "$8" ]
			then
				res=`qvlan bind $9 $8 2>/dev/null`
				res=`cat $file_vlan | sed "s/#$9:/$9:$8/g" > /tmp/vlan_txt `
				res=`cat /tmp/vlan_txt > $file_vlan`
			fi
		fi
	elif [ "$1" = "$2" -a "$2" = "1" ]
	then
		if [ "$4" != "$3" ]
		then
			res=`call_qcsapi wds_remove_peer wifi0 $3`
			res=`call_qcsapi wds_add_peer wifi0 $4`
		fi
		if [ "$5" != "" -a "$5" != "********" ]
		then
			res=`call_qcsapi wds_set_psk wifi0 $4 $5`
		fi
		if [ ! -n "$5" ]
		then
			res=`call_qcsapi wds_set_psk wifi0 $4 NULL`
		fi

		if [ "$7" != "$8" ]
		then
			#Remove vlan tag for the device
			if [ ! -n "$8" ]
			then
				res=`qvlan unbind $9 $7 2>/dev/null`
				res=`cat $file_vlan | sed "s/$9:$7/#$9:/g" > /tmp/vlan_txt `
				res=`cat /tmp/vlan_txt > $file_vlan`
			#Change a vlan tag for the device
			else
				if [ ! -n "$7" ]
				then
					res=`qvlan bind $9 $8 2>/dev/null `
					res=`cat $file_vlan | sed "s/#$9:/$9:$8/g" > /tmp/vlan_txt `
					res=`cat /tmp/vlan_txt > $file_vlan`
				else
					res=`qvlan unbind $9 $7 2>/dev/null`
					res=`qvlan bind $9 $8 2>/dev/null`
					res=`cat $file_vlan | sed "s/$9:$7/$9:$8/g" > /tmp/vlan_txt `
					res=`cat /tmp/vlan_txt > $file_vlan`
				fi
			fi
		fi
	fi
	tnull="NULL"
	if [ "$2" = "1" ]
	then
		tmp="wds={\n mac=$4\n psk="
		if [ "$5" = "" ]
		then
			tmp="$tmp$tnull\n}\n\n"
		elif [ "$5" == "********" ]
		then
			tmp="$tmp$6\n}\n\n"
		else
			tmp="$tmp$5\n}\n\n"
		fi
		config_content="$config_content$tmp"
	fi
}

set_value()
{
	set_single_value "$chk0" "$POST_wds0" "$wds0" "$POST_mac0" "$POST_psk0" "$psk0" "$vlan0" "$POST_vlan0" "wds0"
	set_single_value "$chk1" "$POST_wds1" "$wds1" "$POST_mac1" "$POST_psk1" "$psk1" "$vlan1" "$POST_vlan1" "wds1"
	set_single_value "$chk2" "$POST_wds2" "$wds2" "$POST_mac2" "$POST_psk2" "$psk2" "$vlan2" "$POST_vlan2" "wds2"
	set_single_value "$chk3" "$POST_wds3" "$wds3" "$POST_mac3" "$POST_psk3" "$psk3" "$vlan3" "$POST_vlan3" "wds3"
	set_single_value "$chk4" "$POST_wds4" "$wds4" "$POST_mac4" "$POST_psk4" "$psk4" "$vlan4" "$POST_vlan4" "wds4"
	set_single_value "$chk5" "$POST_wds5" "$wds5" "$POST_mac5" "$POST_psk5" "$psk5" "$vlan5" "$POST_vlan5" "wds5"
	set_single_value "$chk6" "$POST_wds6" "$wds6" "$POST_mac6" "$POST_psk6" "$psk6" "$vlan6" "$POST_vlan6" "wds6"
	set_single_value "$chk7" "$POST_wds7" "$wds7" "$POST_mac7" "$POST_psk7" "$psk7" "$vlan7" "$POST_vlan7" "wds7"
	res=`printf "$config_content" > /mnt/jffs2/wds_config.txt`
}

get_value

if [ -n "$POST_action" ]
then
	set_value
	get_value
fi
%>

<script language="javascript" type="text/javascript">
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
	var chk0=document.getElementById(chk);
	var txtmac0=document.getElementById(txtmac);
	var txtpass0=document.getElementById(txtpass);
	if(chk0.checked==true)
	{
		if(CheckMac(txtmac0.value)==false)
		{
			alert("Invalid MAC address. Please Check.");
			return false;
		}
		if(txtpass0.value.length != 64 && txtpass0.value.length != 0 && txtpass0.value != "********")
		{
			alert("Passphrase must contain 64 ASCII characters");
			txtpass0.focus();
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
	var mform = document.mainform;
	var pform = document.wireless_wds;
	var mMac = new Array(mform.txt_mac0,mform.txt_mac1,mform.txt_mac2,mform.txt_mac3,mform.txt_mac4,mform.txt_mac5,mform.txt_mac6,mform.txt_mac7);

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
		var macvalue = mMac[i].value.toUpperCase();
		if (macvalue != "")
		{
			for(var j=i+1; j<8; j++)
			{
				if( macvalue==mMac[j].value.toUpperCase() )
				{
					alert("Invalid MAC address. Please Check.");
					return;
				}
			}
		}
	}
	if (mform.chk_wds0.checked==true)
	{
		pform.wds0.value="1";
	}
	else
	{
		pform.wds0.value="0";
	}
	pform.mac0.value=mform.txt_mac0.value;
	pform.psk0.value=mform.txt_pass0.value;
	pform.vlan0.value=mform.txt_vlan0.value;

	if (mform.chk_wds1.checked==true)
	{
		pform.wds1.value="1";
	}
	else
	{
		pform.wds1.value="0";
	}
	pform.mac1.value=mform.txt_mac1.value;
	pform.psk1.value=mform.txt_pass1.value;
	pform.vlan1.value=mform.txt_vlan1.value;

	if (mform.chk_wds2.checked==true)
	{
		pform.wds2.value="1";
	}
	else
	{
		pform.wds2.value="0";
	}
	pform.mac2.value=mform.txt_mac2.value;
	pform.psk2.value=mform.txt_pass2.value;
	pform.vlan2.value=mform.txt_vlan2.value;

	if (mform.chk_wds3.checked==true)
	{
		pform.wds3.value="1";
	}
	else
	{
		pform.wds3.value="0";
	}
	pform.mac3.value=mform.txt_mac3.value;
	pform.psk3.value=mform.txt_pass3.value;
	pform.vlan3.value=mform.txt_vlan3.value;

	if (mform.chk_wds4.checked==true)
	{
		pform.wds4.value="1";
	}
	else
	{
		pform.wds4.value="0";
	}
	pform.mac4.value=mform.txt_mac4.value;
	pform.psk4.value=mform.txt_pass4.value;
	pform.vlan4.value=mform.txt_vlan4.value;

	if (mform.chk_wds5.checked==true)
	{
		pform.wds5.value="1";
	}
	else
	{
		pform.wds5.value="0";
	}
	pform.mac5.value=mform.txt_mac5.value;
	pform.psk5.value=mform.txt_pass5.value;
	pform.vlan5.value=mform.txt_vlan5.value;

	if (mform.chk_wds6.checked==true)
	{
		pform.wds6.value="1";
	}
	else
	{
		pform.wds6.value="0";
	}
	pform.mac6.value=mform.txt_mac6.value;
	pform.psk6.value=mform.txt_pass6.value;
	pform.vlan6.value=mform.txt_vlan6.value;

	if (mform.chk_wds7.checked==true)
	{
		pform.wds7.value="1";
	}
	else
	{
		pform.wds7.value="0";
	}
	pform.mac7.value=mform.txt_mac7.value;
	pform.psk7.value=mform.txt_pass7.value;
	pform.vlan7.value=mform.txt_vlan7.value;
	pform.submit();
}

function load_value()
{
	init_menu();
	set_control_value('chk_wds0','<% echo -n "$chk0" %>', 'checkbox');
	set_control_value('txt_mac0','<% echo -n "$wds0" %>', 'text');
	set_control_value('txt_pass0','<% echo -n "$d_psk0" %>', 'text');
	set_control_value('txt_vlan0','<% if [ "$chk0" -eq 1 ]; then echo -n "$vlan0"; fi %>', 'text');
	set_control_value('chk_wds1','<% echo -n "$chk1" %>', 'checkbox');
	set_control_value('txt_mac1','<% echo -n "$wds1" %>', 'text');
	set_control_value('txt_pass1','<% echo -n "$d_psk1" %>', 'text');
	set_control_value('txt_vlan1','<% if [ "$chk1" -eq 1 ]; then echo -n "$vlan1"; fi %>', 'text');
	set_control_value('chk_wds2','<% echo -n "$chk2" %>', 'checkbox');
	set_control_value('txt_mac2','<% echo -n "$wds2" %>', 'text');
	set_control_value('txt_pass2','<% echo -n "$d_psk2" %>', 'text');
	set_control_value('txt_vlan2','<% if [ "$chk2" -eq 1 ]; then echo -n "$vlan2"; fi %>', 'text');
	set_control_value('chk_wds3','<% echo -n "$chk3" %>', 'checkbox');
	set_control_value('txt_mac3','<% echo -n "$wds3" %>', 'text');
	set_control_value('txt_pass3','<% echo -n "$d_psk3" %>', 'text');
	set_control_value('txt_vlan3','<% if [ "$chk3" -eq 1 ]; then echo -n "$vlan3"; fi %>', 'text');
	set_control_value('chk_wds4','<% echo -n "$chk4" %>', 'checkbox');
	set_control_value('txt_mac4','<% echo -n "$wds4" %>', 'text');
	set_control_value('txt_pass4','<% echo -n "$d_psk4" %>', 'text');
	set_control_value('txt_vlan4','<% if [ "$chk4" -eq 1 ]; then echo -n "$vlan4"; fi %>', 'text');
	set_control_value('chk_wds5','<% echo -n "$chk5" %>', 'checkbox');
	set_control_value('txt_mac5','<% echo -n "$wds5" %>', 'text');
	set_control_value('txt_pass5','<% echo -n "$d_psk5" %>', 'text');
	set_control_value('txt_vlan5','<% if [ "$chk5" -eq 1 ]; then echo -n "$vlan5"; fi %>', 'text');
	set_control_value('chk_wds6','<% echo -n "$chk6" %>', 'checkbox');
	set_control_value('txt_mac6','<% echo -n "$wds6" %>', 'text');
	set_control_value('txt_pass6','<% echo -n "$d_psk6" %>', 'text');
	set_control_value('txt_vlan6','<% if [ "$chk6" -eq 1 ]; then echo -n "$vlan6"; fi %>', 'text');
	set_control_value('chk_wds7','<% echo -n "$chk7" %>', 'checkbox');
	set_control_value('txt_mac7','<% echo -n "$wds7" %>', 'text');
	set_control_value('txt_pass7','<% echo -n "$d_psk7" %>', 'text');
	set_control_value('txt_vlan7','<% if [ "$chk7" -eq 1 ]; then echo -n "$vlan7"; fi %>', 'text');
}
</script>

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
		<div class="righttop">WIRELESS - WDS</div>
		<div class="rightmain">
			<form name="mainform">
			<table class="tablemain">
				<tr>
					<td width="5%"></td>
					<td width="10%">WDS</td>
					<td width="25%">MAC Address</td>
					<td width="25%">Passphrase</td>
					<td>VLAN</td>
				</tr>
				<tr>
					<td><input name="chk_wds0" id="chk_wds0" type="checkbox"  class="checkbox"/></td>
					<td>WDS0:</td>
					<td><input name="txt_mac0" type="text" id="txt_mac0" class="textbox"/></td>
					<td><input name="txt_pass0" type="text" id="txt_pass0" class="textbox"/></td>
					<td><input name="txt_vlan0" style="width:42px" type="text" id="txt_vlan0" class="textbox"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds1" id="chk_wds1" type="checkbox"  class="checkbox"/></td>
					<td>WDS1:</td>
					<td><input name="txt_mac1" type="text" id="txt_mac1" class="textbox"/></td>
					<td><input name="txt_pass1" type="text" id="txt_pass1" class="textbox"/></td>
					<td><input name="txt_vlan1" style="width:42px" type="text" id="txt_vlan1" class="textbox"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds2" id="chk_wds2" type="checkbox"  class="checkbox"/></td>
					<td>WDS2:</td>
					<td><input name="txt_mac2" type="text" id="txt_mac2" class="textbox"/></td>
					<td><input name="txt_pass2" type="text" id="txt_pass2" class="textbox"/></td>
					<td><input name="txt_vlan2" style="width:42px" type="text" id="txt_vlan2" class="textbox"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds3" id="chk_wds3" type="checkbox"  class="checkbox"/></td>
					<td>WDS3:</td>
					<td><input name="txt_mac3" type="text" id="txt_mac3" class="textbox"/></td>
					<td><input name="txt_pass3" type="text" id="txt_pass3" class="textbox"/></td>
					<td><input name="txt_vlan3" style="width:42px" type="text" id="txt_vlan3" class="textbox"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds4" id="chk_wds4" type="checkbox"  class="checkbox"/></td>
					<td>WDS4:</td>
					<td><input name="txt_mac4" type="text" id="txt_mac4" class="textbox"/></td>
					<td><input name="txt_pass4" type="text" id="txt_pass4" class="textbox"/></td>
					<td><input name="txt_vlan4" style="width:42px" type="text" id="txt_vlan4" class="textbox"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds5" id="chk_wds5" type="checkbox"  class="checkbox"/></td>
					<td>WDS5:</td>
					<td><input name="txt_mac5" type="text" id="txt_mac5" class="textbox"/></td>
					<td><input name="txt_pass5" type="text" id="txt_pass5" class="textbox"/></td>
					<td><input name="txt_vlan5" style="width:42px" type="text" id="txt_vlan5" class="textbox"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds6" id="chk_wds6" type="checkbox"  class="checkbox"/></td>
					<td>WDS6:</td>
					<td><input name="txt_mac6" type="text" id="txt_mac6" class="textbox"/></td>
					<td><input name="txt_pass6" type="text" id="txt_pass6" class="textbox"/></td>
					<td><input name="txt_vlan6" style="width:42px" type="text" id="txt_vlan6" class="textbox"/></td>
				</tr>
				<tr>
					<td><input name="chk_wds7" id="chk_wds7" type="checkbox"  class="checkbox"/></td>
					<td>WDS7:</td>
					<td><input name="txt_mac7" type="text" id="txt_mac7" class="textbox"/></td>
					<td><input name="txt_pass7" type="text" id="txt_pass7" class="textbox"/></td>
					<td><input name="txt_vlan7" style="width:42px" type="text" id="txt_vlan7" class="textbox"/></td>
				</tr>
				<tr>
					<td class="divline" colspan="5";></td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_save_basic" id="btn_save_basic" type="button" onclick="validate();"  class="button">Save</button>
				<button name="btn_cancel_basic" id="btn_cancel_basic" type="button" class="button" onclick="load_value();">Cancel</button>
			</div>
			</form>
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

<form name="wireless_wds" method="POST" action="wireless_wds.cgi">
	<input type="hidden" name="action" value="action" />
	<input type="hidden" name="wds0" />
	<input type="hidden" name="mac0" />
	<input type="hidden" name="psk0" />
	<input type="hidden" name="wds1" />
	<input type="hidden" name="mac1" />
	<input type="hidden" name="psk1" />
	<input type="hidden" name="wds2" />
	<input type="hidden" name="mac2" />
	<input type="hidden" name="psk2" />
	<input type="hidden" name="wds3" />
	<input type="hidden" name="mac3" />
	<input type="hidden" name="psk3" />
	<input type="hidden" name="wds4" />
	<input type="hidden" name="mac4" />
	<input type="hidden" name="psk4" />
	<input type="hidden" name="wds5" />
	<input type="hidden" name="mac5" />
	<input type="hidden" name="psk5" />
	<input type="hidden" name="wds6" />
	<input type="hidden" name="mac6" />
	<input type="hidden" name="psk6" />
	<input type="hidden" name="wds7" />
	<input type="hidden" name="mac7" />
	<input type="hidden" name="psk7" />
	<input type="hidden" name="vlan0" />
	<input type="hidden" name="vlan1" />
	<input type="hidden" name="vlan2" />
	<input type="hidden" name="vlan3" />
	<input type="hidden" name="vlan4" />
	<input type="hidden" name="vlan5" />
	<input type="hidden" name="vlan6" />
	<input type="hidden" name="vlan7" />
</form>

</body>
</html>

