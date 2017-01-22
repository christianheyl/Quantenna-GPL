/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2013 Quantenna Communications, Inc.          **
**                                                                           **
**  File        : menu.js                                                    **
**  Description :                                                            **
**                                                                           **
*******************************************************************************
**                                                                           **
**  Redistribution and use in source and binary forms, with or without       **
**  modification, are permitted provided that the following conditions       **
**  are met:                                                                 **
**  1. Redistributions of source code must retain the above copyright        **
**     notice, this list of conditions and the following disclaimer.         **
**  2. Redistributions in binary form must reproduce the above copyright     **
**     notice, this list of conditions and the following disclaimer in the   **
**     documentation and/or other materials provided with the distribution.  **
**  3. The name of the author may not be used to endorse or promote products **
**     derived from this software without specific prior written permission. **
**                                                                           **
**  Alternatively, this software may be distributed under the terms of the   **
**  GNU General Public License ("GPL") version 2, or (at your option) any    **
**  later version as published by the Free Software Foundation.              **
**                                                                           **
**  In the case this software is distributed under the GPL license,          **
**  you should have received a copy of the GNU General Public License        **
**  along with this software; if not, write to the Free Software             **
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA  **
**                                                                           **
**  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR       **
**  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES**
**  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  **
**  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,         **
**  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT **
**  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,**
**  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY    **
**  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT      **
**  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF **
**  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.        **
**                                                                           **
*******************************************************************************
EH0*/

function createMenuItem(index,MenuName,arrItem,arrLink,arrActive)
{
	var strHtml;
	strHtml='<li class="item"><a href="javascript:void(0)" class="title" id="sub_menu'+index+'">'+MenuName+'</a>'+
			'<ul id="opt_'+index+'" class="option">';
	document.write(strHtml);

	for(i=0;i<arrItem.length;i++)
	{
		if (arrActive[i]==true)
		{
			if (arrItem[i]=="Log")
			{
				strHtml='<li><b><font color="#216BA4">-&nbsp&nbsp</font><a href=void();" onclick="javascript:window.open(\''+arrLink[i]+'\'); return false;"><font color="#216BA4">'+arrItem[i]+'</font></a></b></li>\n';
			}
			else
			{
				strHtml='<li><b><font color="#216BA4">-&nbsp&nbsp</font><a href="'+arrLink[i]+'"><font color="#216BA4">'+arrItem[i]+'</font></a></b></li>\n';
			}
			document.write(strHtml);

			if(arrItem[i+1]=='NULL')
			{
				break;
			}
		}
	}
	strHtml='</ul>'+
			'</li>';
	document.write(strHtml);
}

function createMenu(mode,previlege)
{
	var strHtml;
	var arrMenuItem = new Array("Status","Wireless","Ethernet","Tools","System");

	var arrStatusItem = new Array("Device","Wireless","Networking","WDS","MBSS","NULL");
	var arrStatusLink = new Array("status_device.cgi","status_wireless.cgi","status_networking.cgi","status_wds.cgi","status_mbss.cgi","NULL");

	var arrWirelessItem = new Array("DeviceMode","Basic","WPS","MAC Filter","WDS","MBSS","Advanced","NULL");
	var arrWirelessLink = new Array("wireless_mode.cgi","wireless_basic.cgi","wireless_wps.cgi","wireless_macfilter.cgi","wireless_wds.cgi","wireless_mbss.cgi","wireless_advanced.cgi","NULL");

	var arrEthernetItem = new Array("Networking","NULL");
	var arrEthernetLink = new Array("ethernet_networking.cgi","NULL");

	var arrToolsItem = new Array("Log","Command","Admin","NULL");
	var arrToolsLink = new Array("tools_log.cgi","tools_command.cgi","tools_admin.cgi","NULL");

	var arrSystemItem = new Array("Restore","Upgrade","Reboot","NULL");
	var arrSystemLink = new Array("system_restore.cgi","system_upgrade.cgi","system_reboot.cgi","NULL");

	//Super
	if (previlege=="0")
	{
		if (mode == "Station")
		{
			var arrStatusActive = new Array(true,true,true,false,false);
			var arrWirelessActive = new Array(true,true,true,false,false,false,true);
		}
		else
		{
			var arrStatusActive = new Array(true,true,true,true,true);
			var arrWirelessActive = new Array(true,true,true,true,true,true,true);
		}
		var arrEthernetActive = new Array(true);
		var arrToolsActive = new Array(true,true,true);
		var arrSystemActive = new Array(true,true,true);
	}
	//Admin
	else if (previlege=="1")
	{
		if (mode == "Station")
		{
			var arrStatusActive = new Array(true,true,true,false,false);
			var arrWirelessActive = new Array(true,true,true,false,false,false,true);
		}
		else
		{
			var arrStatusActive = new Array(true,true,true,true,true);
			var arrWirelessActive = new Array(true,true,true,true,true,true,true);
		}
		var arrEthernetActive = new Array(true);
		var arrToolsActive = new Array(false,false,true);
		var arrSystemActive = new Array(false,true,true);
	}
	//User
	else
	{
		if (mode == "Station")
		{
			var arrStatusActive = new Array(true,true,true,false,false);
			var arrWirelessActive = new Array(true,true,true,false,false,false,false);
		}
		else
		{
			var arrStatusActive = new Array(true,true,true,true,true);
			var arrWirelessActive = new Array(true,true,true,false,true,true,false);
		}
		var arrEthernetActive = new Array(true);
		var arrToolsActive = new Array(false,false,true);
		var arrSystemActive = new Array(false,false,true);
	}
	strHtml='<ul id="menu">';
	document.write(strHtml);
	createMenuItem("1",arrMenuItem[0],arrStatusItem,arrStatusLink,arrStatusActive);
	createMenuItem("2",arrMenuItem[1],arrWirelessItem,arrWirelessLink,arrWirelessActive);
	createMenuItem("3",arrMenuItem[2],arrEthernetItem,arrEthernetLink,arrEthernetActive);
	createMenuItem("4",arrMenuItem[3],arrToolsItem,arrToolsLink,arrToolsActive);
	createMenuItem("5",arrMenuItem[4],arrSystemItem,arrSystemLink,arrSystemActive);
	strHtml='</ul>';
	document.write(strHtml);
}

function createTop(version,mode)
{
	var strHtml;
	strHtml='<table style="height:100%; width:100%; padding-top:30px; padding-right:5px; text-align:right; font-weight:bold;">';
	document.write(strHtml);
	strHtml='<tr>';
	document.write(strHtml);
	strHtml='<td>'+version+'<br>'+mode+'</td>';
	document.write(strHtml);
	strHtml='</tr>';
	document.write(strHtml);
	strHtml='</table>';
	document.write(strHtml);
}
 
function createBot()
{
	var strHtml;
	strHtml='<td><a href=void();" onclick="javascript:window.open(\'/bot_menu/About_quantenna.cgi\'); return false;" style="_width:50px;width:50px\9; ">About Quantenna</a></td>'
	document.write(strHtml);
	strHtml='<td>|</td>'
	document.write(strHtml);
	strHtml='<td><a href=void();" onclick="javascript:window.open(\'/bot_menu/Investors.cgi\'); return false;" style="_width:80px;width:80px\9; ">Investors</a></td>'
	document.write(strHtml);
	strHtml='<td>|</td>'
	document.write(strHtml);
	strHtml='<td><a href=void();" onclick="javascript:window.open(\'/bot_menu/Contact_us.cgi\'); return false;" style="_width:80px;width:80px\9; ">Contact Us</a></td>'
	document.write(strHtml);
	strHtml='<td>|</td>'
	document.write(strHtml);
	strHtml='<td><a href=void();" onclick="javascript:window.open(\'/bot_menu/Privacy_policy.cgi\'); return false;" style="_width:80px;width:80px\9; ">Privacy Policy</a></td>'
	document.write(strHtml);
	strHtml='<td>|</td>'
	document.write(strHtml);
	strHtml='<td><a href=void();" onclick="javascript:window.open(\'/bot_menu/Terms_of_use.cgi\'); return false;" style="_width:80px;width:80px\9; ">Terms of Use</a></td>'
	document.write(strHtml);
	strHtml='<td><p>©2012 Quantenna Communications Inc. All Rights Reserved.</p></td>'
	document.write(strHtml);
}

function init_menu()
{
	var submenu;
	var opt;
	document.getElementById("opt_1").style.display = "block";
	document.getElementById("opt_2").style.display = "block";
	document.getElementById("opt_3").style.display = "block";
	document.getElementById("opt_4").style.display = "block";
	document.getElementById("opt_5").style.display = "block";
	submenu = document.getElementById("sub_menu1");
	submenu.onclick = function()
	{
		opt = document.getElementById("opt_1");
		if (opt.style.display != "block")
		{
			opt.style.display = "block";
		}
		else
		{
			opt.style.display = "none";
		}
	}
	submenu = document.getElementById("sub_menu2");
	submenu.onclick = function()
	{
		opt = document.getElementById("opt_2");
		if (opt.style.display != "block")
		{
			opt.style.display = "block";
		}
		else
		{
			opt.style.display = "none";
		}
	}
	submenu = document.getElementById("sub_menu3");
	submenu.onclick = function()
	{
		opt = document.getElementById("opt_3");
		if (opt.style.display != "block")
		{
			opt.style.display = "block";
		}
		else
		{
			opt.style.display = "none";
		}
	}
	submenu = document.getElementById("sub_menu4");
	submenu.onclick = function()
	{
		opt = document.getElementById("opt_4");
		if (opt.style.display != "block")
		{
			opt.style.display = "block";
		}
		else
		{
			opt.style.display = "none";
		}
	}
	submenu = document.getElementById("sub_menu5");
	submenu.onclick = function()
	{
		opt = document.getElementById("opt_5");
		if (opt.style.display != "block")
		{
			opt.style.display = "block";
		}
		else
		{
			opt.style.display = "none";
		}
	}
}
