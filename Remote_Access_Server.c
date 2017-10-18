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
int checkarg(char*, char*, int);

int pipefd[1000][2];
int pipecount = 0;//only parent can handle it

int main(int argc, char* argv[])
{
	int sockfd, newsockfd, childpid;
	struct sockaddr_in	cli_addr, serv_addr;
	socklen_t clilen;
	/*
	 *Open a TCP socket(an Internet stream socket).
	 */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		err_dump("server:can't open stream socket\n");
	}
	memset((char*)&serv_addr, 0, sizeof(serv_addr));
	//bzero((char* )&serv_addr, sizeof(serv_addr));
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
		newsockfd = accept(sockfd, ((struct sockaddr*)&cli_addr), &clilen);
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
	int rc, i, j, line_offset = 0;
	int fill_back_flag = 0;
	char line[MAXSIZE];
	for(i = 0 ; i < 1000 ; i++){
		pipe(pipefd[i]);
	}
	while(1)
	{
		rc = read(sockfd, line + line_offset, MAXLENG - line_offset);
		if(rc < 0){
			err_dump("readline:read error\n");
		}else if(rc == 0 && line_offset == 0){//finished
			exit(0);
		}else{//may contain not one command
			for(i = 0 ; i < rc + line_offset ; i++){
				if( (line[i] == '\n' || line[i] == '\0') && i < (rc - 1) ){//contain more than one command
					line[i] = '\0';//i == length of first command
					parser(line, i);
					for(j = 0 ; j < rc + line_offset -i ; j++){
						line[j] = line[j+i];
					}
					line_offset = rc + line_offset - i;
					fill_back_flag = 1;
					break;
				}
			}
			if(fill_back_flag == 1){
				fill_back_flag = 0;
				break;
			}
			//contain only one command
			line[rc + line_offset] = '\0';
			parser(line, rc + line_offset);
		}
	}
}
void parser(char* line, int len){//it also call exec
	char *buff;
	int pid;
	int argcount = 0;
	char *arg[checkarg(line, " ", len)];
	int i;
	buff = strtok(line, " ");
	arg[argcount] = malloc((strlen(buff) + 1) * sizeof(char));
	strcpy(arg[argcount++], buff);
	while(buff != NULL)
	{
		if(buff[0] == '|'){//pipe occur!!
			pipecount++;
			arg[argcount] = NULL;
			argcount = 0;
			if((pid = fork()) == 0){//fork a child
				if(pipecount > 1){// there'd been other pipe
					dup2(pipecount * 2 - 1, 0);//last pipe read
				}
				if(buff[1] == '\0'){//normal pipe, equal |x, x is a number
					dup2((pipecount + 1) * 2, 1);
					for(i = 1 ; i <= 1000 ; i++){
						close((i + 1) * 2 - 1);
						close((i + 1) * 2);
					}
				}else{//|x x is a number
					dup2(pipecount + (buff[1] - '0') * 2, 1);
					for(i = 1 ; i <= 1000 ; i++){
						close((i + 1) * 2 - 1);
						close((i + 1) * 2);
					}
				}
				execvp(arg[0], arg);
				exit(1);
			}
		}else{
			arg[argcount] = malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(arg[argcount++], buff);
		}
		buff = strtok(line, " ");
	}
	//no more pipe!!
	if((pid =fork()) == 0){
		if(pipecount > 1){
			dup2(pipecount * 2 - 1, 0);
		}
		for(i = 1 ; i <= 1000 ; i++){
			close((i + 1) * 2 - 1);
			close((i + 1) * 2);
		}
		execvp(arg[0], arg);
	}
}
int checkarg(char* str, char* delim, int len){
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
//int readline(int fd, char* ptr, int maxlen){
//	int n,rc;
//	rc = read(fd, ptr, maxlen);
//	if(rc < 0){
//		err_dump("readline:read error\n");
//	}else if(rc < maxlen){
//		
//	}
//	
//}
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
