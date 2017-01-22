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
				strHtml='<li><a href=void();" onclick="javascript:window.open(\''+arrLink[i]+'\'); return false;">'+arrItem[i]+'</b></li>\n';
			}
			else
			{
				strHtml='<li><a href="'+arrLink[i]+'">'+arrItem[i]+'</a></li>\n';
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
	var arrMenuItem = new Array("Status","Config","Tools","System");

	var arrStatusItem = new Array("Device","Wireless","Networking","WDS","MBSS","NULL");
	var arrStatusLink = new Array("status_device.php","status_wireless.php","status_networking.php","status_wds.php","status_mbss.php","NULL");

	var arrConfigItem = new Array("Wireless","WPS","MAC Filter","Networking","WDS","MBSS","802.11u","Hotspot","NULL");
	var arrConfigLink = new Array("config_wireless.php","config_wps.php","config_macfilter.php","config_networking.php","config_wds.php","config_mbss.php","config_11u.php","config_hotspot.php","NULL");

	var arrToolsItem = new Array("Log","Command","Admin","Ubootinfo","Restore","Power","NULL");
	var arrToolsLink = new Array("tools_log.php","tools_command.php","tools_admin.php","tools_ubootinfo.php","tools_restore.php","tools_power.php","NULL");

	var arrSystemItem = new Array("Upgrade","Reboot","NULL");
	var arrSystemLink = new Array("system_upgrade.php","system_reboot.php","NULL");

	//Super
	if (previlege=="0")
	{
		if (mode == "Station")
		{
			var arrStatusActive = new Array(true,true,true,false,false);
			var arrConfigActive = new Array(true,true,false,true,false,false,false,false);
		}
		else
		{
			var arrStatusActive = new Array(true,true,true,true,true);
			var arrConfigActive = new Array(true,true,true,true,true,true,true,true);
		}
		var arrToolsActive = new Array(true,true,true,true,true,true);
		var arrSystemActive = new Array(true,true);
	}
	//Admin
	else if (previlege=="1")
	{
		if (mode == "Station")
		{
			var arrStatusActive = new Array(true,true,true,false,false);
			var arrConfigActive = new Array(true,true,false,true,false,false,false,false);
		}
		else
		{
			var arrStatusActive = new Array(true,true,true,true,true);
			var arrConfigActive = new Array(true,true,true,true,true,true,true,true);
		}
		var arrToolsActive = new Array(true,false,true,false,false,false);
		var arrSystemActive = new Array(true,true);
	}
	//User
	else
	{
		if (mode == "Station")
		{
			var arrStatusActive = new Array(true,true,true,false,false);
			var arrConfigActive = new Array(true,false,false,false,false,false,false,false);
		}
		else
		{
			var arrStatusActive = new Array(true,true,true,true,true);
			var arrConfigActive = new Array(true,false,false,false,true,true,true,true);
		}
		var arrToolsActive = new Array(false,false,true,false,false,false);
		var arrSystemActive = new Array(false,true);
	}
	strHtml='<ul id="menu">';
	document.write(strHtml);
	createMenuItem("1",arrMenuItem[0],arrStatusItem,arrStatusLink,arrStatusActive);
	createMenuItem("2",arrMenuItem[1],arrConfigItem,arrConfigLink,arrConfigActive);
	createMenuItem("3",arrMenuItem[2],arrToolsItem,arrToolsLink,arrToolsActive);
	createMenuItem("4",arrMenuItem[3],arrSystemItem,arrSystemLink,arrSystemActive);
	strHtml='</ul>';
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
}
