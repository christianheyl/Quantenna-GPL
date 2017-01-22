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
var privilege=get_privilege(1);
</script>

<%
source ./common_sh.sh

curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`
curr_bandwidth=40
curr_mcs=0

func_get_value(){
	#get bandwidth
	func_wr_wireless_conf bw
	curr_bandwidth=$rval_func_wr_wireless_conf

	#get txrate
	test_for_mcs=`call_qcsapi get_option wifi0 autorate`
	if [ "$test_for_mcs" = "TRUE" ]
	then
		curr_mcs=0
	else
		curr_mcs=`call_qcsapi get_mcs_rate wifi0`
	fi

	if [ "$curr_mode" = "Access point" ]
	then
		#get beacon interval
		bintval=`call_qcsapi get_beacon_interval wifi0`

		#get dtim
		dtimperiod=`call_qcsapi get_dtim wifi0`

		#get shortGI
		tmp_short_gi=`call_qcsapi get_option wifi0 short_GI`
		if [ "$tmp_short_gi" = "TRUE" ]
		then
			short_gi=1
		else
			short_gi=0
		fi
	fi
}

func_set_value(){
	#change mcs rate
	if [ "$curr_mcs" != "$POST_p_cmb_txrate" ]
	then
		if [ "$POST_p_cmb_txrate" != "0" ]
		then
			res=`call_qcsapi set_mcs_rate wifi0 $POST_p_cmb_txrate`
		else
			res=`iwconfig wifi0 rate auto`
		fi
	fi

	if [ "$curr_mode" = "Access point" ]
	then
		#change beacon interval
		if [ -n "$POST_p_txt_beaconinterval" -a "$bintval" -ne "$POST_p_txt_beaconinterval" ]
		then
			res=`call_qcsapi set_beacon_interval wifi0 $POST_p_txt_beaconinterval`
		fi

		#change dtim
		if [ -n "$POST_p_txt_dtimperiod" -a "$dtimperiod" -ne "$POST_p_txt_dtimperiod" ]
		then
			res=`call_qcsapi set_dtim wifi0 $POST_p_txt_dtimperiod`
		fi

		#set short GI
		if [ -n "$POST_p_chb_shortgi" -a "$short_gi" -ne "$POST_p_chb_shortgi" ]
		then
			res=`call_qcsapi set_option wifi0 short_GI $POST_p_chb_shortgi`
		fi
	fi
}


func_get_value
if [ -n "$POST_action" ]
then
	func_set_value
	func_get_value
fi
%>

<script language="javascript" type="text/javascript">

function populate_mcslist()
{
	mcslist = document.getElementById("cmb_txrate");
	if(!mcslist) return;
	mcslist.options.length = 77;
	mcslist.options[0].text = "Auto"; mcslist.options[0].value = "0";
	for(i=0;i<=31;i++)
	{
		{mcslist.options[i+1].text = "MCS"+i; mcslist.options[i+1].value = "MCS"+i;}
	}

	for(i=33;i<=76;i++)
	{
		{mcslist.options[i].text = "MCS"+i; mcslist.options[i].value = "MCS"+i;}
	}

	mcs_value ='<% echo -n "$curr_mcs"; %>' ;
	mcs_index = 0;
	if(mcs_value.length >= 4&&mcs_value.length<=6)
	{
		mcs_index = mcs_value.substr(3);
		var tmp = "+";
		if(31>=mcs_index)
		mcs_index = eval(mcs_index+tmp+1);
	}

	mcslist.options[mcs_index].selected = true;
}


function arrange_display()
{
	var mode='<% echo -n $curr_mode %>'
	if (mode!="Access point") {
		set_visible('tr_bi',false);
		set_visible('tr_dtim',false);
		set_visible('tr_sg',false);
	}
}

function load_value()
{
	arrange_display();
	populate_mcslist();
	init_menu();
	set_control_value('cmb_bandwidth','<% echo -n $curr_bandwidth %>', 'combox');
	set_control_value('txt_beaconinterval','<% echo -n $bintval %>', 'text');
	set_control_value('txt_dtimperiod','<% echo -n $dtimperiod %>', 'text');
	set_control_value('chb_shortgi','<% echo -n $short_gi %>', 'checkbox');
	set_control_value('cmb_bandwidth','<% echo -n $curr_bandwidth %>', 'combox');
}

function validate()
{
	var pf=document.wireless_advanced;
	var mf=document.mainform;
	var bint = document.getElementById("txt_beaconinterval");
	var dtim = document.getElementById("txt_dtimperiod");
	var mode='<% echo -n $curr_mode %>'
	if(mode=="Access point" && bint.value == "")
	{
		alert("Please enter beacon interval");
		return false;
	}

	if((bint.value != "") && (!(/^\d+$/.test(bint.value))))
	{
		alert("The beacon interval value should be natural numbers");
		return false;
	}
	if(bint.value != "" && (bint.value < 25 || bint.value > 5000))
	{
		alert("Beacon interval needs to be between 25 and 5000");
		return false;
	}

	if(mode=="Access point" && dtim.value == "")
	{
		alert("Please enter DTIM period value");
		return false;
	}

	if((dtim.value != "") && (!(/^\d+$/.test(dtim.value))))
	{
		alert("The DTIM period value should be natural numbers");
		return false;
	}
	if((dtim.value != "" )&& ((dtim.value > 15) || (dtim.value < 1)))
	{
		alert("DTIM period needs to be between 1 and 15");
		return false;
	}

	pf.p_cmb_txrate.value=mf.cmb_txrate.value;
	pf.p_txt_beaconinterval.value=mf.txt_beaconinterval.value;
	pf.p_txt_dtimperiod.value=mf.txt_dtimperiod.value;
	if (document.getElementById('chb_shortgi').checked) {
		pf.p_chb_shortgi.value=1
	}
	else{
		pf.p_chb_shortgi.value=0
	}

	pf.submit();
}

</script>

<body onload="load_value();" class="body" >
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
		<div class="righttop">WIRELESS - ADVANCED</div>
		<div class="rightmain">
			<form id="mainform" name="mainform">
				<table class="tablemain">
					<tr>
						<td width="40%">Bandwidth:</td>
						<td width="60%">
							<select name="cmb_bandwidth" class="combox" id="cmb_bandwidth" disabled="disabled">
								<option value="40">40 Mhz</option>
								<option value="20">20 Mhz</option>
							</select>
						</td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
					<tr>
						<td>TX Rate:</td>
						<td>
							<select name="cmb_txrate" class="combox" id="cmb_txrate">
							</select>
						</td>
					</tr>
					<tr name="tr_bi" id="tr_bi">
						<td>Beacon Interval (in ms):</td>
						<td>
							<input name="txt_beaconinterval" type="text" id="txt_beaconinterval" value="" class="textbox"/>
						</td>
					</tr>
					<tr name="tr_dtim" id="tr_dtim">
						<td>DTIM Period:</td>
						<td>
							<input name="txt_dtimperiod" type="text" id="txt_dtimperiod" value="" class="textbox"/>
						</td>
					</tr>
					<tr name="tr_sg" id="tr_sg">
						<td>Short GI:</td>
						<td>
							<input name="chb_shortgi" type="checkbox" id="chb_shortgi" value="1" />
						</td>
					</tr>
					<tr>
						<td class="divline" colspan="2";></td>
					</tr>
				</table>
				<div class="rightbottom">
					<button name="btn_save_adv" id="btn_save_basic" type="button" onclick="validate();" class="button">Save</button>
					<button name="btn_cancel_adv" id="btn_cancel_basic" type="button" onclick="load_value();" class="button">Cancel</button>
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

<form name="wireless_advanced" method="POST" action="wireless_advanced.cgi">
	<input type="hidden" name="action" value="action" />
	<input type="hidden" name="p_cmb_txrate" />
	<input type="hidden" name="p_txt_beaconinterval" />
	<input type="hidden" name="p_txt_dtimperiod" />
	<input type="hidden" name="p_chb_shortgi" />
</form>
</body>
</html>
