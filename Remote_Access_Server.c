#define MAXLINE	10000
#define SERV_TCP_PORT 5051
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
void mysetenv(char* ,char* , int);
void printenv();
void err_dump(char*);
void process_request(int);
int readline(int, char*, int);

int main(int argc, char* argv[])
{
	int sockfd, newsockfd, clilen, childpid;
	struct sockaddr_in	cli_addr, serv_addr;
	/*
	 *Open a TCP socket(an Internet stream socket).
	 */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		err_dump("server:can't open stream socket");
	}
	//memset((char*)&serv_addr, 0, sizeof(serv_addr));
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	serv_addr.sin_port			= htons(SERV_TCP_PORT);

	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		err_dump("server:can't bind local address");
	}
	listen(sockfd, 5);
	while(1)
	{
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
		if(newsockfd < 0){
			err_dump("server:accept error");
		}
		if((childpid = fork()) < 0){
			err_dump("server: fork error");
		}else if(childpid == 0){/*child process*/
			/*close original socket*/
			close(sockfd);
			/*process the request*/
			//TODO process request
			process_request(newsockfd);
			exit(0);
		}
		close(newsockfd);/*parent process*/
	}
}
void err_dump(char* msg){
	perror(msg);
	exit(1);
}
void process_request(int sockfd){
	int n;
	char line[MAXLINE];
	while(1)
	{
		//The length of a single-line input will not exceed 10000 characters.
		n = readline(sockfd, line, MAXLINE);
		if(n == 0){
			return;
		}else if(n < 0){
			err_dump("process_request:readline error");
		}
		//if(writen(sockfd, line, n)!= n){
		//	err_dump("process_request:writen error");
		//}
	}
}
int readline(int fd, char* ptr, int maxlen){
	int n,rc;
	read(fd, ptr, maxlen);
	
}
void mysetenv(char* name, char* value, int replace){
	if(setenv(name, value, replace) == 0){
		printf("change env succeeded!!\n");
	}else{
		printf("change env failed\n");
	}
	return;
}
void myprintenv(){
	int i;
	extern char **environ;
	for(i= 0 ; environ[i]!= NULL ; i++){
		printf("%s\n", environ[i]);
	}
	return;
}
