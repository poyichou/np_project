#define SERV_TCP_PORT 7000
#define MAXSIZE 512
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<fcntl.h>
#include<signal.h>
#include<errno.h>
char workdir[512];
int passiveTCP(int port, int qlen){
	int sockfd;
	struct sockaddr_in  serv_addr;
	/*
	 *Open a TCP socket(an Internet stream socket).
	 */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("server:can't open stream socket");
		exit(1);
	}
	memset((char*)&serv_addr, 0, sizeof(serv_addr));
	//bzero((char* )&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	serv_addr.sin_port			= htons(SERV_TCP_PORT);

	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("server:can't bind local address");
		exit(1);
	}
	listen(sockfd, qlen);
	return sockfd;
}
void chdir_to_server(){
	if(chdir(getenv("HOME")) < 0){
		perror("cd error");
		exit(1);
	}
	if(chdir("Downloads/NP_project3/cgi") < 0){
		perror("cd error");
		exit(1);
	}
}
void reaper(int signo){
	if(signo == SIGCHLD){
		//union wait status;
		int status;
		waitpid(-1, &status, WNOHANG);
		//while(wait3(&status, WNOHANG, (struct rusage*)0) >= 0)
		//	/*empty*/;
	}
}
void set_env(int sockfd){
	if(setenv("CONTENT_LENGTH", "whatever_I_want", 1) != 0){
		write(sockfd, "change failed", (strlen("change failed")) * sizeof(char));
	}
	if(setenv("REQUEST_METHOD", "whatever_I_want", 1) != 0){
		write(sockfd, "change failed", (strlen("change failed")) * sizeof(char));
	}
	if(setenv("SCRIPT_NAME", "whatever_I_want", 1) != 0){
		write(sockfd, "change failed", (strlen("change failed")) * sizeof(char));
	}
	if(setenv("REMOTE_HOST", "whatever_I_want", 1) != 0){
		write(sockfd, "change failed", (strlen("change failed")) * sizeof(char));
	}
	if(setenv("REMOTE_ADDR", "whatever_I_want", 1) != 0){
		write(sockfd, "change failed", (strlen("change failed")) * sizeof(char));
	}
	if(setenv("AUTH_TYPE", "whatever_I_want", 1) != 0){
		write(sockfd, "change failed", (strlen("change failed")) * sizeof(char));
	}
	if(setenv("REMOTE_USER", "whatever_I_want", 1) != 0){
		write(sockfd, "change failed", (strlen("change failed")) * sizeof(char));
	}
	if(setenv("REMOTE_IDENT", "whatever_I_want", 1) != 0){
		write(sockfd, "change failed", (strlen("change failed")) * sizeof(char));
	}
}
void read_request(int sockfd, char *req){
	int rc = 0;
	rc = read(sockfd, req, MAXSIZE - 1);
	if(rc == 0){
		close(sockfd);
		return;
	}else if(rc < 0){
		perror("readline:read error");
		exit(1);
	}else{
		int i;
		for(i = 0 ; i < rc ; i++){
			if( (req[i] == '\n' || req[i] == '\r')){//remove newline 
				req[i] = '\0';
			}
		}
	}
}
void parse_request(char request[], char *cgi_name){
	char req[strlen(request) + 1];
	strcpy(req, request);
	char *buff;
	int idx = -1;
	int i;
	for(i = 0 ; i < strlen(req) ; i++){
		if(req[i] == '/'){
			idx = i;
		}else if(req[i] == 'H' || req[i] == 'h'){
			break;
		}
	}
	if(idx == -1){
		perror("wrong GET format");
		exit(1);
	}
	//strcpy(cgi_name, buff + idx + 1);
	for(i = 0 ; i < MAXSIZE - 1 ; i++){
		if(i + idx + 1 >= strlen(req)){
			break;
		}
		cgi_name[i] = req[i + idx + 1];
		if(cgi_name[i] == ' ' || cgi_name[i] == '?'){
			cgi_name[i] = 0;
			break;
		}
	}
	//get arguments
	buff = strtok(req, "?");
	buff = strtok(NULL, "?");
	if(buff == NULL){
		return;
	}else{
		//GET [domain]?[query] HTTP 1.1
		//              ^
		for(i = 0 ; i < strlen(buff) ; i++){
			if(buff[i] == ' '){
				buff[i] = 0;
				break;
			}
		}
		if(setenv("QUERY_STRING", buff, 1) != 0){
			perror("change env failed");
			exit(1);
		}
	}
}
void exec_cgi(int sockfd, char cgi_name[]){
	int len = strlen(cgi_name);
	char filepath[strlen(cgi_name) + strlen(workdir) + 1 + 1];
	snprintf(filepath, sizeof(filepath), "%s/%s", workdir, cgi_name);
	if(!strcmp(cgi_name + len - 4, ".cgi") && access(filepath, X_OK) == 0){
		write(sockfd, "HTTP/1.1 200 OK\r\n", strlen("HTTP/1.1 200 OK\r\n"));
		write(sockfd, "Content-Type: text/html\r\n\r\n", strlen("Content-Type: text/html\r\n\r\n"));
		dup2(sockfd, 1);
		if(execlp(filepath, cgi_name, NULL) < 0){
			perror("exec error");
			exit(1);
		}
	}else if(!strcmp(cgi_name + len - 4, ".cgi") && access(filepath, R_OK) == 0){
		write(sockfd, "HTTP/1.1 200 OK\r\n", strlen("HTTP/1.1 200 OK\r\n"));
		write(sockfd, "Content-Type: text/html\r\n\r\n", strlen("Content-Type: text/html\r\n\r\n"));
		dup2(sockfd, 1);
		execlp("perl", "perl", filepath, NULL);
		perror("perl error");
		exit(1);
	}else if(access(filepath, R_OK) == 0){//html or somthing
		write(sockfd, "HTTP/1.1 200 OK\r\n", strlen("HTTP/1.1 200 OK\r\n"));
		write(sockfd, "Content-Type: text/html\r\n\r\n", strlen("Content-Type: text/html\r\n\r\n"));
		dup2(sockfd, 1);
		execlp("cat", "cat", filepath, NULL);
		perror("cat error");
		exit(1);
	}else{
		write(sockfd, "HTTP/1.1 404\r\n", strlen("HTTP/1.1 404\r\n"));
		char cwd[80];
		getcwd(cwd,80);
		perror(cwd);
		perror(cgi_name);
		perror("not readable or executable");
		write(sockfd, "not readable or executable\n", strlen("not readable or executable\n"));
		exit(1);
	}
}
void process_request(int sockfd){
	char req[MAXSIZE], cgi_name[MAXSIZE];
	memset(cgi_name, 0, sizeof(cgi_name));
	set_env(sockfd);
	read_request(sockfd, req);
	perror(req);
	parse_request(req, cgi_name);
	exec_cgi(sockfd, cgi_name);
}
int main(int argc, char* argv[])
{
	snprintf(workdir, sizeof(workdir), "%s/Downloads/NP_project3/cgi", getenv("HOME"));
	int msockfd;//master sock
	int pid;
	struct sockaddr_in cli_addr;
	socklen_t alen; // client address length
	msockfd = passiveTCP(SERV_TCP_PORT, 5);
	//delete shared memory when receive ctrl-c

	signal(SIGCHLD, reaper);//handle dead child
	while(1)
	{
		// new connection from client
		int newsockfd;
		alen = sizeof(cli_addr);
		newsockfd = accept(msockfd, (struct sockaddr *)&cli_addr, &alen);
		if(newsockfd < 0){
			if(errno == EINTR)
				continue;
			perror("accept error");
			exit(1);
		}
		if((pid = fork()) < 0){
			perror("fork error");
			exit(1);
		}else if(pid == 0){
			close(msockfd);
			chdir_to_server();
			//proccess requests
			process_request(newsockfd);
			exit(0);
		}
		close(newsockfd);
	}
}
