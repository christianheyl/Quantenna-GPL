/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2013 Quantenna Communications, Inc.          **
**                                                                           **
**  File        : webif.js                                                    **
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

function value(name)
{
	var item = document.getElementById(name);
	return (item ? item.value : "");
}
function set_value(name, value)
{
	var item = document.getElementById(name);
	if (item) item.value = value;
}
function isset(name, val)
{
	return (value(name) == val);
}
function checked(name)
{
	var item = document.getElementById(name);
	return ((item) && item.checked);
}
function hide(name)
{
	var item = document.getElementById(name);
	if (item)
		item.style.display = 'none';
}
function show(name)
{
	var item = document.getElementById(name);
	if (item)
		item.style.display = '';
}
function set_visible(name, value)
{
	if (value)
		show(name)
	else
		hide(name)
}
function set_disabled(name, value)
{
	var item = document.getElementById(name);
	if(item)
	{
		if (value)
			item.disabled = true;
		else
			item.disabled = false;
	}
}

function popnew(url)
{
	newwindow=window.open(url,'name');
	if (window.focus) {newwindow.focus();}
}

//Set control value common function
function set_control_value(cid,cvalue,ctype)
{
	var control = document.getElementById(cid);
	var i=0;
	if(ctype=="text")
	{
		control.value=cvalue;
	}
	else if(ctype == "checkbox")
	{
		if (cvalue == "0" || cvalue == "FALSE")
		{
			control.checked=false;
		}
		else if (cvalue=="1" || cvalue == "TRUE")
		{
			control.checked=true;
		}
	}
	else if(ctype == "combox")
	{
		for (i=0;i<control.options.length;i++)
		{
			if (control.options[i].value==cvalue)
			{
				control.options[i].selected = true;
				break;
			}
		}
	}
	else if(ctype == "radio")
	{
		control = document.getElementsByName(cid);
		if (cvalue == "0" || cvalue == "FALSE")
		{
			if (control[0].value == "0" || control[0].value == "FALSE")
			{
				control[0].checked=true;
			}
			else if (control[0].value == "1" || control[0].value == "TRUE")
			{
				control[0].checked=false;
			}
			if (control[1].value == "0" || control[1].value == "FALSE")
			{
				control[1].checked=true;
			}
			else if (control[1].value == "1" || control[1].value == "TRUE")
			{
				control[1].checked=false;
			}
		}
		else if (cvalue=="1" || cvalue == "TRUE")
		{
			if (control[0].value == "0" || control[0].value == "FALSE")
			{
				control[0].checked=false;
			}
			else if (control[0].value == "1" || control[0].value == "TRUE")
			{
				control[0].checked=true;
			}
			if (control[1].value == "0" || control[1].value == "FALSE")
			{
				control[1].checked=false;
			}
			else if (control[1].value == "1" || control[1].value == "TRUE")
			{
				control[1].checked=true;
			}
		}
	}
}
