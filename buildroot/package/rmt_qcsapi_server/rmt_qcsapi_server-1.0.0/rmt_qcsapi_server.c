#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <wait.h>
#include <errno.h>
#include <fcntl.h>

#define PORT 5112
#define BACKLOG 2

#define MAXDATASIZE 1024
#define CALL_QCSAPI_STR_LEN sizeof("call_qcsapi ")
#define FILE_NAME_MAX_SIZE 512

void process_cli(int connectfd,struct sockaddr_in client);

void sig_handler(int s);

char* input_buffer = NULL;
char* result_buffer = NULL;

int main()
{
	//input buffer
	input_buffer = (char*)malloc(MAXDATASIZE);
	if(!input_buffer){
		exit(1);
	}

	//output buffer
	result_buffer = (char*)malloc(MAXDATASIZE - CALL_QCSAPI_STR_LEN);
	if(!result_buffer){
		free(input_buffer);
		exit(1);
	}


	int opt,listenfd,connectfd;
	pid_t pid;
	struct sockaddr_in server;
	struct sockaddr_in client;
	int sin_size;
	struct sigaction act;
	struct sigaction oact;

	memset(&server,0,sizeof(server));
	server.sin_family=AF_INET;
	server.sin_port=htons(PORT);
	server.sin_addr.s_addr=htonl(INADDR_ANY);

//========================
	act.sa_handler=sig_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags=0;
	
	if(sigaction(SIGCHLD,&act,&oact)<0)
	{
		perror("Sigaction Failed!\n");
		exit(1);
	}
//========================

	if((listenfd=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		perror("Creating Socket Failed.\n");
		exit(1);
	}
	opt=SO_REUSEADDR;
	setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

	if(bind(listenfd,(struct sockaddr *)&server,sizeof(struct sockaddr))==-1)
	{
		perror("Bind Error.\n");
		exit(1);
	}
	if(listen(listenfd,BACKLOG)==-1)
	{
		perror("Listen() Error.\n");
		exit(1);
	}
	sin_size=sizeof(struct sockaddr_in);
	while(1)
	{
		if((connectfd=accept(listenfd,(struct sockaddr *)&client,&sin_size))==-1)
		{
			if(errno==EINTR) continue;
				perror("Accept() Error.\n");
			exit(1);
		}
		if((pid=fork())>0)
		{
			close(connectfd);
			continue;
		}
		else if(pid==0)
		{
			close(listenfd);
			process_cli(connectfd,client);
			exit(0);
		}
		else
		{
			printf("Fork Error.\n");
			exit(1);
		}
	}
	close(listenfd);
	if(input_buffer) free(input_buffer);
	if(result_buffer) free(result_buffer);
	exit(0);
}

void process_cli(int connectfd,struct sockaddr_in client)
{
	FILE *stream;
	int i, num;
	char recvbuf[MAXDATASIZE];

	memset(recvbuf,0,MAXDATASIZE);
	memset(input_buffer,0,MAXDATASIZE);
	memset(result_buffer,0, MAXDATASIZE - CALL_QCSAPI_STR_LEN);

	num=recv(connectfd,result_buffer, (MAXDATASIZE - CALL_QCSAPI_STR_LEN - 1), 0);
	if(num==0)
	{
		close(connectfd);
		printf("Client Disconnected.\n");
		return;
	}

	result_buffer[num] = '\0';

	//Combine the QCSAPI command
	sprintf(input_buffer,"call_qcsapi %s",result_buffer);
	memset(result_buffer,0, MAXDATASIZE - CALL_QCSAPI_STR_LEN);

	//Get the message. Call the qcsapi here.
	stream = popen( input_buffer, "r" );
	if (!stream) {
		printf("wrong discriptor.\n");
		return;
	}
	num = fread( recvbuf, sizeof(char), sizeof(recvbuf) - 1, stream);
	if (num == 0 || ferror(stream)) {
		printf("error reading file.\n");
		return;
	}

	recvbuf[num] = '\0';

	//Return the response
	send(connectfd,recvbuf,strlen(recvbuf),0);

	pclose( stream );
	close(connectfd);
}

void sig_handler(int s)
{
	pid_t pid;
	int stat;
	while((pid=waitpid(-1,&stat,WNOHANG))>0)
		//printf("child %d terminated.\n",pid);
		return;
}
