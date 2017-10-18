#define MAXLENG	10000
#define MAXSIZE 10001
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
void parser(char*, int);
int checkargv(char*, char*, int);
int main(int argc, char* argv[])
{
	int sockfd, newsockfd, clilen, childpid;
	struct sockaddr_in	cli_addr, serv_addr;
	/*
	 *Open a TCP socket(an Internet stream socket).
	 */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		err_dump("server:can't open stream socket\n");
	}
	//memset((char*)&serv_addr, 0, sizeof(serv_addr));
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	serv_addr.sin_port			= htons(SERV_TCP_PORT);

	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		err_dump("server:can't bind local address\n");
	}
	listen(sockfd, 5);
	while(1)
	{
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
		if(newsockfd < 0){
			err_dump("server:accept error\n");
		}
		if((childpid = fork()) < 0){
			err_dump("server: fork error\n");
		}else if(childpid == 0){/*child process*/
			/*close original socket*/
			close(sockfd);
			/*process the request*/
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
	int n, rc, i;
	char line[MAXSIZE];
	//while(1)
	//{
	//	//The length of a single-line input will not exceed 10000 characters.
	//	n = readline(sockfd, line, MAXLENG);
	//	if(n == 0){
	//		return;
	//	}else if(n < 0){
	//		err_dump("process_request:readline error\n");
	//	}
	//	//if(writen(sockfd, line, n)!= n){
	//	//	err_dump("process_request:writen error");
	//	//}
	//}
	while(1)
	{
		rc = read(sockfd, line, MAXLENG);
		if(rc < 0){
			err_dump("readline:read error\n");
		}else{//maybe contain not one command
			for(i = 0 ; i < rc ; i++){
				if( (line[i] == '\n' || line[i] == '\0') && i < (rc - 1) ){//contain more than one command
					line[i] = '\0';
					parser(line, i);
					//TODO:not done
				}
			}
			line[rc] = '\0';
			parser(line, rc);
		}
	}
}
void parser(char* line, int len){
	char *buff;
	char *argv[checkargv(line, " ", len)];
	buff = strtok(line, " ");
	while(buff != NULL)
	{
		buff = strtok(NULL, " ");
	}
	
}
int checkargv(char* str, char* delim, int len){
	char tempstr[len + 1];
	memset(tempstr, 0, sizeof(tempstr));
	int i = 0;
	for(i = 0 ; i < len ; i++){
		tempstr[i] = str[i];
	}
	tempstr[i] = '\0';
	char *buff;
	int n = 0;
	buff = strtok(tempstr, delim);
	while(buff != NULL && buff[0] != '\n')
	{
		n++;
		buff = strtok(NULL, delim);
	}
	return n;
}
int readline(int fd, char* ptr, int maxlen){
	int n,rc;
	rc = read(fd, ptr, maxlen);
	if(rc < 0){
		err_dump("readline:read error\n");
	}else if(rc < maxlen){
		
	}
	
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
