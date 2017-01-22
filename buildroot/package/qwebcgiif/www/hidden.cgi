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
var privilege=get_privilege(0);
</script>
<%
source ./common_sh.sh
curr_mode=`call_qcsapi get_mode wifi0`
curr_version=`call_qcsapi get_firmware_version`
curr_pp_enbl=0
curr_pp_name=""
curr_mspp_enbl=0
curr_mspp_name=""
curr_region=""
curr_regionlist=""
curr_scs=0
curr_vsp=0
curr_bf=0
curr_coc=0

get_value()
{
	if [ "$curr_mode" = "Access point" ]
	then
		curr_pp_enbl=`call_qcsapi get_wps_access_control wifi0`
		if [ $? -ne 0 ]
		then
			curr_pp_enbl=0
		fi
	
		curr_pp_name=`call_qcsapi registrar_get_pp_devname wifi0`
		tmp=`echo -n $curr_pp_name | grep error`
		if [ -n "$tmp" ]
		then
			curr_pp_name=""
		fi
		
		curr_mspp_enbl=`call_qcsapi get_pairing_enable wifi0`
		if [ $? -ne 0 ]
		then
			curr_mspp_enbl=0
		fi
	
		curr_mspp_name=`call_qcsapi get_pairing_id wifi0`
		tmp=`echo -n $curr_mspp_name | grep error`
		if [ -n "$tmp" ]
		then
			curr_mspp_name=""
		fi
	fi
	
	func_wr_wireless_conf region
	curr_region=$rval_func_wr_wireless_conf
	
	curr_regionlist=`call_qcsapi get_regulatory_regions wifi0`
	
	tmp=`call_qcsapi get_scs_status wifi0`
	if [ $? -ne 0 ]
	then
		curr_scs=0
	else
		if [ "$tmp" = "Enabled (1)" ]
		then
			curr_scs=1
		else
			curr_scs=0
		fi
	fi
	
	tmp=`call_qcsapi vsp wifi0 show`
	if [ $? -ne 0 ]
	then
		curr_vsp=0
	else
		if [ "$tmp" = "VSP is not enabled" ]
		then
			curr_vsp=0
		else
			curr_vsp=1
		fi
	fi
	
	func_wr_wireless_conf bf
	curr_bf=$rval_func_wr_wireless_conf	
	
	tmp=`qpm show | grep ^level | awk '{print $2}'`
	if [ $? -ne 0 ]
	then
		curr_coc=0
	else
		if [ "$tmp" = "0" ]
		then
			curr_coc=0
		else
			curr_coc=1
		fi
	fi
	
	func_wr_wireless_conf pwr_cal
	tmp_pwr_cal=$rval_func_wr_wireless_conf
	if [ -z "$tmp_pwr_cal" -o "$tmp_pwr_cal" = "off" ]
	then
		pwr_cal=0
	else
		pwr_cal=1
	fi
	func_wr_wireless_conf pwr
	pwr=$rval_func_wr_wireless_conf

}

set_value()
{
	#Set WPS PP
	if [ "$POST_p_enblpp" != "$curr_pp_enbl" ]
	then
		if [ "$POST_p_enblpp" = "1" ]
		then
			if [ "$POST_p_txt_pp_name" != "$curr_pp_name" ]
			then
				res=`call_qcsapi registrar_set_pp_devname wifi0 "$POST_p_txt_pp_name"`
			fi
		fi

		if [ "$POST_p_enblpp" -eq 1 ]
		then
			states_wps=`call_qcsapi get_wps_configured_state wifi0`
			if [ "$states_wps" == "disabled" ]
			then
				confirm "Please enable WPS at WPS page"
			else
				res=`call_qcsapi set_wps_access_control wifi0 $POST_p_enblpp`
			fi
		else
			res=`call_qcsapi set_wps_access_control wifi0 $POST_p_enblpp`
		fi
	else
		if [ "$POST_p_enblpp" = "1" ]
		then
			if [ "$POST_p_txt_pp_name" != "$curr_pp_name" ]
			then
				res=`call_qcsapi registrar_set_pp_devname wifi0 "$POST_p_txt_pp_name"`
			fi
		fi
	fi
	
	#Set MS PP
	if [ "$POST_p_enblmspp" != "$curr_mspp_enbl" ]
	then
		if [ "$POST_p_enblmspp" = "1" ]
		then
			if [ "$POST_p_txt_mspp_name" != "$curr_mspp_name" ]
			then
				res=`call_qcsapi set_pairing_id wifi0 "$POST_p_txt_mspp_name"`
			fi
		fi
		res=`call_qcsapi set_pairing_enable wifi0 $POST_p_enblmspp`
	else
		if [ "$POST_p_enblmspp" = "1" ]
		then
			if [ "$POST_p_txt_mspp_name" != "$curr_mspp_name" ]
			then
				res=`call_qcsapi set_pairing_id wifi0 "$POST_p_txt_mspp_name"`
			fi
		fi
	fi

	#Set SCS
	if [ "$POST_p_enblscs" != "$curr_scs" ]
	then
		res=`call_qcsapi enable_scs wifi0 $POST_p_enblscs`
		func_wr_wireless_conf scs $POST_p_enblscs
	fi

	#Set VSP
	if [ "$POST_p_enblvsp" != "$curr_vsp" ]
	then
		res=`call_qcsapi vsp wifi0 set enabled $POST_p_enblvsp`
		func_wr_wireless_conf vsp $POST_p_enblvsp
	fi

	#Set BF
	if [ "$POST_p_enblbf" != "$curr_bf" ]
	then
		if [ "$POST_p_enblbf" -eq 1 ]
		then
			bfon
		else
			bfoff
		fi
		func_wr_wireless_conf bf $POST_p_enblbf
	fi
	
	#Set CoC
	if [ "$POST_p_enblcoc" != "$curr_coc" ]
	then
		res=`qpm level $POST_p_enblcoc`
	fi

	#Set power_cal
	if [ -n "$POST_p_pwr_cal_wifi0" -a "$pwr_cal" -ne "$POST_p_pwr_cal_wifi0" ]
	then
		if [ "$POST_p_pwr_cal_wifi0" -eq 1 ]
		then
			func_wr_wireless_conf pwr_cal on
		else
			func_wr_wireless_conf pwr_cal off
		fi
		if [ "$POST_p_pwr_cal_wifi0" -eq 1 ]
		then
			set_tx_pow 255
		else
			set_tx_pow 0
		fi
	fi

	if [ "$POST_p_pwr_cal_wifi0" -eq 1 -a -n "$POST_p_tx_pwr_wifi0" -a "$pwr" != "$POST_p_tx_pwr_wifi0" ]
	then
		func_wr_wireless_conf pwr $POST_p_tx_pwr_wifi0
		res=`set_tx_pow $POST_p_tx_pwr_wifi0`
	fi
	
	#Set region and redirect to reboot page
	if [ "$curr_region" != "$POST_p_cmb_region" ]
	then
		func_wr_wireless_conf region $POST_p_cmb_region
		confirm "Region changing needs reboot. Please reboot the system."
		echo "<script language='javascript'>location.href='system_reboot.cgi'</script>"
	fi
}

get_value

if [ -n "$POST_action" ]
then
	set_value
	get_value
fi
%>

<script language="javascript" type="text/javascript">
function modechange(obj)
{
	if(obj.name == 'enblpp')
	{
		if(obj.value == "0")
			set_disabled("txt_pp_name",true);
		else if (obj.value == "1")
			set_disabled("txt_pp_name",false);
	}
	if(obj.name == 'enblmspp')
	{
		if(obj.value == "0")
			set_disabled("txt_mspp_name",true);
		else if (obj.value == "1")
			set_disabled("txt_mspp_name",false);
	}
	if(obj.name == 'pwr_cal_wifi0')
	{
		if(obj.value == "0")
			set_disabled("tx_pwr_wifi0",true);
		else if (obj.value == "1")
			set_disabled("tx_pwr_wifi0",false);
	}
}

function populate_powerlist()
{
	var tmp1 = "+";
	var tmp2 = "-";
	powerlist = document.getElementById("tx_pwr_wifi0");
	if(!powerlist) return;
	powerlist.options.length = 14;
	for(i=1;i<=14;i++)
	{
		{powerlist.options[i-1].text = i; powerlist.options[i-1].value = eval(i+tmp1+10);}
	}
	power_value = '<% echo -n $pwr %>' ;
	power_cal_value = '<% echo -n $pwr_cal %>' ;
	power_index = eval(power_value+tmp2+11);
	powerlist.options[power_index].selected = true;
	if (power_cal_value == 0) {
		set_disabled("tx_pwr_wifi0",true);
	}
	else {
		set_disabled("tx_pwr_wifi0",false);
	}
}

function populate_regionlist()
{
	var cmb_region = document.getElementById("cmb_region");

	var regionlist='<% echo -n "$curr_regionlist" %>';
	var curr_region='<% echo -n "$curr_region" %>';
	var tmp=regionlist.split(",");
	for(var i=0;i<tmp.length;i++)
	{
		cmb_region.options.add(new Option(tmp[i],tmp[i]));
		if( curr_region==tmp[i] )
			cmb_region.options[i].selected = true;
	}
}

function validate(act)
{
	if (act==0)//Save Click
	{
		var pf=document.hidden;
		var mf=document.mainform;
		//WPS PP
		if (mf.enblpp[0].checked==true)
		{
			pf.p_enblpp.value=0;
		}
		else if (mf.enblpp[1].checked==true)
		{
			pf.p_enblpp.value=1;
			if (mf.txt_pp_name.value.length > 128)
			{
				alert("The Device IDs are a comma separated list 1 to 128 characters in length with commas as delimiters");
				return;
			}
			if (mf.txt_pp_name.value.length < 1)
			{
				alert("The Device IDs are a comma separated list 1 to 128 characters in length with commas as delimiters");
				return;
			}
		}
		pf.p_txt_pp_name.value=mf.txt_pp_name.value;

		//MS PP
		if (mf.enblmspp[0].checked==true)
		{
			pf.p_enblmspp.value=0;
		}
		else if (mf.enblmspp[1].checked==true)
		{
			pf.p_enblmspp.value=1;
			if (mf.txt_mspp_name.value.length > 32)
			{
				alert("The length of the pairing ID is 1 to 32 characters");
				return;
			}
			if (mf.txt_mspp_name.value.length < 1)
			{
				alert("The length of the pairing ID is 1 to 32 characters");
				return;
			}
		}
		pf.p_txt_mspp_name.value=mf.txt_mspp_name.value;
		
		//Region
		pf.p_cmb_region.value=mf.cmb_region.value;
		
		//SCS
		if (mf.enblscs[0].checked==true)
		{
			pf.p_enblscs.value=0;
		}
		else if (mf.enblscs[1].checked==true)
		{
			pf.p_enblscs.value=1;
		}

		//VSP
		if (mf.enblvsp[0].checked==true)
		{
			pf.p_enblvsp.value=0;
		}
		else if (mf.enblvsp[1].checked==true)
		{
			pf.p_enblvsp.value=1;
		}
		
		//BF
		if (mf.enblbf[0].checked==true)
		{
			pf.p_enblbf.value=0;
		}
		else if (mf.enblbf[1].checked==true)
		{
			pf.p_enblbf.value=1;
		}
		
		//COC
		if (mf.enblcoc[0].checked==true)
		{
			pf.p_enblcoc.value=0;
		}
		else if (mf.enblcoc[1].checked==true)
		{
			pf.p_enblcoc.value=1;
		}

		//TX Power
		if (mf.pwr_cal_wifi0[0].checked==true)
		{
			pf.p_pwr_cal_wifi0.value=0;
		}
		else if (mf.pwr_cal_wifi0[1].checked==true)
		{
			pf.p_pwr_cal_wifi0.value=1;
		}
		pf.p_tx_pwr_wifi0.value=mf.tx_pwr_wifi0.value;

		pf.submit();
	}
}

function load_value()
{
	init_menu();
	set_control_value('enblpp','<% echo -n $curr_pp_enbl %>', 'radio');
	set_control_value('txt_pp_name','<% echo -n "$curr_pp_name" %>', 'text');
	set_control_value('enblmspp','<% echo -n $curr_mspp_enbl %>', 'radio');
	set_control_value('txt_mspp_name','<% echo -n "$curr_mspp_name" %>', 'text');
	
	populate_regionlist();
	set_control_value('enblscs','<% echo -n $curr_scs %>', 'radio');
	set_control_value('enblvsp','<% echo -n $curr_vsp %>', 'radio');
	set_control_value('enblbf','<% echo -n $curr_bf %>', 'radio');
	set_control_value('enblcoc','<% echo -n $curr_coc %>', 'radio');
	
	set_control_value('pwr_cal_wifi0','<% echo -n $pwr_cal %>', 'radio');
	populate_powerlist();
	
	var mode = '<% echo -n "$curr_mode" %>';
	if (mode == "Station")
	{
		set_visible('tr_enblpp', false);
		set_visible('tr_txt_pp_name', false);
		set_visible('tr_enblmspp', false);
		set_visible('tr_txt_mspp_name', false);
		set_visible('tr_pp', false);
	}
	else
	{
		curr_pp='<% echo -n $curr_pp_enbl %>'
		if (curr_pp == 0)
			set_disabled("txt_pp_name",true);
		else
			set_disabled("txt_pp_name",false);
			
		curr_mspp='<% echo -n $curr_mspp_enbl %>'
		if (curr_mspp == 0)
			set_disabled("txt_mspp_name",true);
		else
			set_disabled("txt_mspp_name",false);
	}
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
		<div class="righttop">HIDDEN</div>
		<div class="rightmain">
			<form name="mainform">
			<table class="tablemain">
				<tr id="tr_enblpp">
					<td width="40%">WPS Pairing Protection</td>
					<td width="60%">
						<input type="radio" name="enblpp" id="enblpp" onclick="modechange(this);" value="0"> Disable&nbsp;&nbsp;
						<input type="radio" name="enblpp" id="enblpp" onclick="modechange(this);" value="1"> Enable
					</td>
				</tr>
				<tr id="tr_txt_pp_name">
					<td>WPS Pairing String</td>
					<td>
						<input name="txt_pp_name" type="text" id="txt_pp_name" class="textbox" style="width:167px;" />
					</td>
				</tr>
				<tr id="tr_enblmspp">
					<td>MS Pairing Protection</td>
					<td>
						<input type="radio" name="enblmspp" id="enblmspp" onclick="modechange(this);" value="0"> Disable&nbsp;&nbsp;
						<input type="radio" name="enblmspp" id="enblmspp" onclick="modechange(this);" value="1"> Enable
					</td>
				</tr>
				<tr id="tr_txt_mspp_name">
					<td>MS Pairing String</td>
					<td>
						<input name="txt_mspp_name" type="text" id="txt_mspp_name" class="textbox" style="width:167px;" />
					</td>
				</tr>
				<tr id="tr_pp">
					<td class="divline" colspan="2";></td>
				</tr>
				<tr>
					<td>Region:</td>
					<td>
						<select name="cmb_region" class="combox" id="cmb_region">
						</select>
					</td>
				</tr>
				<tr>
					<td>SCS:</td>
					<td>
						<input type="radio" name="enblscs" id="enblscs" value="0"> Disable&nbsp;&nbsp;
						<input type="radio" name="enblscs" id="enblscs" value="1"> Enable
					</td>
				</tr>
				<tr>
					<td>VSP:</td>
					<td>
						<input type="radio" name="enblvsp" id="enblvsp" value="0"> Disable&nbsp;&nbsp;
						<input type="radio" name="enblvsp" id="enblvsp" value="1"> Enable
					</td>
				</tr>

				<tr>
					<td>BF:</td>
					<td>
						<input type="radio" name="enblbf" id="enblbf" value="0"> Disable&nbsp;&nbsp;
						<input type="radio" name="enblbf" id="enblbf" value="1"> Enable
					</td>
				</tr>
				<tr>
					<td>CoC:</td>
					<td>
						<input type="radio" name="enblcoc" id="enblcoc" value="0"> Disable&nbsp;&nbsp;
						<input type="radio" name="enblcoc" id="enblcoc" value="1"> Enable
					</td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
				<tr>
					<td>Tx Power</td>
					<td>
					<input type="radio" name="pwr_cal_wifi0" value="0" onclick="modechange(this);"/> Off &nbsp; &nbsp;
					<input type="radio" name="pwr_cal_wifi0" value="1" onclick="modechange(this);"/> On
					</td>
				</tr>
				<tr>
					<td>Tx Power Selection</td>
					<td>
						<select name="tx_pwr_wifi0" class="combox" id="tx_pwr_wifi0" >
						</select>
					</td>
				</tr>
				<tr>
					<td class="divline" colspan="2";></td>
				</tr>
			</table>
			<div class="rightbottom">
				<button name="btn_save_basic" id="btn_save_basic" type="button" onclick="validate(0);"  class="button">Save</button>
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

<form name="hidden" id="hidden" method="POST" action="hidden.cgi">
	<input type="hidden" name="action" value="action" />
	<input type="hidden" name="p_enblpp" />
	<input type="hidden" name="p_txt_pp_name" />
	<input type="hidden" name="p_enblmspp" />
	<input type="hidden" name="p_txt_mspp_name" />
	<input type="hidden" name="p_cmb_region" />
	<input type="hidden" name="p_enblscs" />
	<input type="hidden" name="p_enblvsp" />
	<input type="hidden" name="p_enblbf" />
	<input type="hidden" name="p_enblcoc" />
	<input type="hidden" name="p_pwr_cal_wifi0" />
	<input type="hidden" name="p_tx_pwr_wifi0" />
</form>

</body>
</html>

