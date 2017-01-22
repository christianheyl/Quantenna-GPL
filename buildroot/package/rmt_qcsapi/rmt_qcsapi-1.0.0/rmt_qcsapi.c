/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2012 Quantenna Communications Inc            **
**                                                                           **
**  File        : rmt_qcsapi.c                                               **
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
EH1*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define PORT 5112
#define MAXDATASIZE 1000

const char* g_config_file = "/mnt/jffs2/rmt_ip.conf";

char ipaddr[20];
char temp[MAXDATASIZE];


void process(FILE *fp,int sockfd);

char *getMessage(char *sendline,int len,FILE *fp);

int read_ipconfig()
{
	FILE* fp = NULL;
	
	fp = fopen(g_config_file, "rt");
	if(0 == fp)
	{return 1;}
	fgets(ipaddr, 20, fp) ;

	fclose(fp);
	fp = NULL;
	return 0;
}

int write_ipconfig()
{
	FILE* fp = NULL;
	fp = fopen(g_config_file, "wt");
	if(0 == fp)
	{return 1;}
	else
	{
		int countwrite = fwrite(ipaddr, 1, strlen(ipaddr), fp);		
		fclose(fp);
		fp = NULL;

		if( countwrite != strlen(ipaddr))
		{return 1;}
	}
	return 0;
}

int isip(char *str,char *ip)
{
	int s;
	s = inet_pton(AF_INET,str, ip);  
    if(s<=0)
    {return 1;}
    return 0;
}

int main(int argc,char *argv[])
{
	int fd;
	int i=0;
	struct hostent *he;
	struct sockaddr_in server;

	//Read the IP address form the ip config file
	i=read_ipconfig();
	
	//If can`t read the file, let the user input a new IP, then save to file and exit.
	if(i!=0)
	{
		printf("Fail to load the IP conf file. Please input the server IP address:\n");
		if(fgets(ipaddr,20,stdin)==NULL)
		{
			printf("Input the IP address error, exit.\n");
			exit(1);
		}
		i=write_ipconfig();
		if (i!=0)
		{
			printf("Fail to save the IP conf file.\n");
			exit(1);
		}
		else
		{exit(0);}
	}
	//no argv, just output the configed IP addr and exit
	if(argc==1)
	{
		printf("IP Address : %s",ipaddr);
		exit(1);
	}
	else
	{
		//if the first argv is an IP addr, save to file and exit
		if (isip(argv[1],ipaddr)==0)
		{
			memcpy(ipaddr,argv[1],20);
			sprintf(ipaddr,"%s\n",ipaddr);
			i=write_ipconfig();
			if (i!=0)
			{
				printf("Fail to save the IP conf file.\n");
				exit(1);
			}
			else
			{exit(0);}
		}
		//combine the qcsapi command
		for(i=0;i<argc-1;i++)
		{
			sprintf(temp,"%s %s",temp,argv[i+1]);
		}
	}

	if((he=gethostbyname(ipaddr))==NULL)
	{
		printf("get host by name error.\n");
		exit(1);
	}

	if((fd=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		printf("socket() error.\n");
		//perror("socket() error.\n");
		exit(1);
	}

	memset(&server,0,sizeof(server));
	server.sin_family=AF_INET;
	server.sin_port=htons(PORT);

	server.sin_addr=*((struct in_addr *)he->h_addr);
	if(connect(fd,(struct sockaddr *)&server,sizeof(struct sockaddr))==-1)
	{
		printf("connect() error.\n");
		//perror("connect() error.\n");
		exit(1);
	}
	process(stdin,fd);
	close(fd);
}

void process(FILE *fp,int sockfd)
{
	char sendbuf[MAXDATASIZE];
	char recvbuf[MAXDATASIZE];
	int num;
	send(sockfd,temp,strlen(temp),0);
	if((num=recv(sockfd,recvbuf,MAXDATASIZE,0))==0)
	{
		printf("Server no send you any data.\n");
		return;
	}
	recvbuf[num]='\0';
	printf("%s",recvbuf);
}

char* getMessage(char *sendline,int len,FILE *fp)
{
	printf("Input string to server:\n");
	return(fgets(sendline,len,fp));
}

