#define MAXLENG	10000
#define MAXSIZE 10001
const char WELCOME_MESSAGE[] =	"****************************************\n"
				"** Welcome to the information server. **\n"
				"****************************************\n";

#define SERV_TCP_PORT 7000
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include "Remote_Access_Server.h"
#include "Single_Process_Concurrent_Server_main.h"

struct User user[30];
int usercount = 0;

//user_pipefd[from user id][to user id]
int user_pipefd[31][31][2];

int line_offset = 0;

void err_dump(char* msg){
	perror(msg);
	exit(1);
}
int passiveTCP(int port, int qlen){
	int sockfd;
	struct sockaddr_in  serv_addr;
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
	listen(sockfd, qlen);
	return sockfd;
}
void printenv_all(int sockfd){
	int i;
	extern char **environ;
	for(i= 0 ; environ[i] != NULL ; i++){
		write(sockfd, environ[i], (strlen(environ[i])) * sizeof(char));
		write(sockfd, "\n", (strlen("\n")) * sizeof(char));
	}
	return;
}
void print_env(int sockfd, char *line){
	//printenv
	char tmp_line[MAXSIZE];
	char* vname;
	int wc = 0;
	strcpy(tmp_line, line);
	vname = strtok(tmp_line, " ");
	vname = strtok(NULL, " ");//name
	if(vname == NULL){//no arg
		printenv_all(sockfd);
	}else{//has arg
		char* vvalue = getenv(vname);
		if(vvalue == NULL){
			write(sockfd, "There is no match", (strlen("There is no match")) * sizeof(char));
		}else{
			char* writestr;
			writestr = malloc((strlen(vname) + strlen("=") + strlen(vvalue) + strlen("\n") + 1) * sizeof(char));
			strcpy(writestr, vname);
			strcat(writestr, "=");
			strcat(writestr, vvalue);
			strcat(writestr, "\n");
			wc = write(sockfd, writestr, (strlen(writestr)) * sizeof(char));
			free(writestr);
			if(wc < 0){
				err_dump_sock(sockfd, "write error");
			}
		}
	}
}
void set_env(int sockfd, char* line){
	char tmp_line[MAXSIZE];
	char* vname, *vvalue;// variable name, value
	strcpy(tmp_line, line);
	vname = strtok(tmp_line, " ");
	//name
	vname = strtok(NULL, " ");
	//usage error
	if(vname == NULL){
		write(sockfd, "no variable name", (strlen("no variable name")) * sizeof(char));
	}
	//value
	vvalue = strtok(NULL, " ");
	if(vvalue == NULL){// no value
		write(sockfd, "no value", (strlen("no value")) * sizeof(char));
	}else{// has value
		if(setenv(vname, vvalue, 1) != 0){//failed
			write(sockfd, "change failed", (strlen("change failed")) * sizeof(char));
		}
	}
}
int fd_idx(int fd){
	int i;
	for(i = 0 ; i < usercount ; i++){
		//find myself
		if(user[i].fd == fd){
			return i;
		}
	}
	return -1;
}
int id_idx(int id){
	int i;
	for(i = 0 ; i < usercount ; i++){
		//find myself
		if(user[i].id == id){
			return i;
		}
	}
	return -1;
}
int check_id_exist(int myfd, char *userid){
	int i;
	int userid_i = atoi(userid);
	for(i = 0 ; i < usercount ; i++){
		//find
		if(user[i].id == userid_i){
			return 0;
		}
	}
	//not found
	if(write(myfd, "Error:user #", strlen("Error:user #")) < 0){
		err_dump("write error");
	}
	if(write(myfd, userid, strlen(userid)) < 0){
		err_dump("write error");
	}
	if(write(myfd, " dose not exist yet.\n", strlen(" dose not exist yet.\n")) < 0){
		err_dump("write error");
	}
	return -1;
}
void simple_tell(int fd, char* msg){
	if(write(fd, msg, strlen(msg)) < 0){
		err_dump("write error");
	}
}
void tell(int myfd, char* line){
	char *buff;
	int userid;
	buff = strtok(line, " ");//buff == tell
	buff = strtok(NULL, " ");//buff == userid
	if(buff == NULL)
		err_dump("parse tell error");
	userid = atoi(buff);
	int myidx = fd_idx(myfd);
	//*** IamUser told you ***: Hello World.
	char msg[strlen("***  told you ***: ") + strlen(user[myidx].name) + strlen(line) + 2];
	if(check_id_exist(myfd, buff) < 0){//user not exist
		return;
	}
	snprintf(msg, sizeof(msg), "*** %s told you ***: ", user[myidx].name);
	//get msg
	buff = strtok(NULL, " ");
	while(buff != NULL)
	{
		strcat(msg, buff);
		strcat(msg, " ");
		buff = strtok(NULL, " ");
	}
	strcat(msg, "\n");
	simple_tell(user[id_idx(userid)].fd, msg);
	//simple_tell(user[id_idx(userid)].fd, "% ");
}
void simple_yell(int myfd, char* msg){
	int i;
	for(i = 0 ; i < usercount ; i++){
		if(user[i].fd != myfd){
			simple_tell(user[i].fd, msg);
		}
	}
}
void simple_broadcast(int myfd, char* msg){
	int i;
	for(i = 0 ; i < usercount ; i++){
		simple_tell(user[i].fd, msg);
	}
}
void yell(int myfd, char* line){
	int myidx = fd_idx(myfd);
	//*** IamUser yelled ***: Hi everybody
	char msg[strlen("***  yelled ***: ") + strlen(user[myidx].name) + strlen(line) + 2];
	char* buff;
	snprintf(msg, sizeof(msg), "*** %s yelled ***: ", user[myidx].name);
	buff = strtok(line, " ");//"yell"
	//get msg
	buff = strtok(NULL, " ");
	while(buff != NULL)
	{
		strcat(msg, buff);
		strcat(msg, " ");
		buff = strtok(NULL, " ");
	}
	strcat(msg, "\n");
	simple_yell(myfd, msg);
	//simple_yell(myfd, "% ");
}
void login_broadcast(int myfd){
	int myidx = fd_idx(myfd);
	char msg[strlen("*** User '' entered from . ***") + 20 + 30 + 2];
	snprintf(msg, sizeof(msg), "*** User '%s' entered from %s. ***\n", user[myidx].name, user[myidx].ip_port);
	simple_broadcast(myfd, msg);
}
void logout_yell(int myfd){
	int myidx = fd_idx(myfd);
	char msg[strlen("*** User '' left. ***") + 20 + 2];
	snprintf(msg, sizeof(msg), "*** User '%s' left. ***\n", user[myidx].name);
	simple_yell(myfd, msg);
}
void broadcast(int myfd, char* line){
	int myidx = fd_idx(myfd);
	//*** IamUser yelled ***: Hi everybody
	char msg[strlen("***  yelled ***: ") + strlen(user[myidx].name) + strlen(line) + 2];
	char* buff;
	snprintf(msg, sizeof(msg), "*** %s yelled ***: ", user[myidx].name);
	buff = strtok(line, " ");//"yell"
	//get msg
	buff = strtok(NULL, " ");
	while(buff != NULL)
	{
		strcat(msg, buff);
		strcat(msg, " ");
		buff = strtok(NULL, " ");
	}
	strcat(msg, "\n");
	simple_broadcast(myfd, msg);
	//simple_broadcast(myfd, "% ");
}
void name(int myfd, char* line){
	char* name;
	name = strtok(line, " ");
	name = strtok(NULL, " ");//name
	int i;
	for(i = 0 ; i < usercount ; i++){
		if(user[i].fd != myfd && strcmp(user[i].name, name) == 0){
			char msg[strlen("*** User '' already exists. ***\n") + strlen(name) + 1];
			snprintf(msg, sizeof(msg), "*** User '%s' already exists. ***\n", name);
			simple_tell(myfd, msg);
			return;
		}
	}
	int myidx = fd_idx(myfd);
	strcpy(user[myidx].name, name);
	//yell *** User from (IP/port) is named '(name)'. ***
	char msg[strlen("*** User from  is named ''. ***\n") + strlen(user[myidx].ip_port) + strlen(name) + 1];
	snprintf(msg, sizeof(msg), "*** User from %s is named '%s'. ***\n", user[myidx].ip_port, name);
	simple_broadcast(myfd, msg);
	//simple_broadcast(myfd, "% ");
}
void print_all_user(int myfd){
	int i = 0;
	char msg[3 + 1 + 20 + 1 + 6 + 1 + strlen("<-me") + 1];
	write(myfd, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n", strlen("<ID>\t<nickname>\t<IP/port>\t<indicate me>\n"));
	for(i = 0 ; i < usercount ; i++){
		memset(msg, 0, sizeof(msg));
		snprintf(msg, sizeof(msg), "%d\t%s\t%s", user[i].id, user[i].name, user[i].ip_port);
		if(user[i].fd == myfd){
			strcat(msg, "\t<-me");
		}
		strcat(msg, "\n");
		simple_tell(myfd, msg);
	}
}
int build_in_or_command(int sockfd, char* line, int len){
	if (strcmp(line, "/") == 0){
		write(sockfd, "\"/\" is blocked\n", strlen("\"/\" is blocked\n"));
		return 0;
	}else if(strncmp(line, "exit", 4) == 0){//exit
		return 1;
	}else if(strncmp(line, "printenv", 8) == 0){
		print_env(sockfd, line);
	}else if(strncmp(line, "setenv", 6) == 0){
		set_env(sockfd, line);
	}else if(strncmp(line, "tell", 4) == 0){//tell <client id> <msg(may contain white space)>
		tell(sockfd, line);
	}else if(strncmp(line, "yell", 4) == 0){//yell <msg>
		yell(sockfd, line);
	}else if(strncmp(line, "name", 4) == 0){//name <name>
		name(sockfd, line);
	}else if(strncmp(line, "who", 3) == 0){//who
		print_all_user(sockfd);
	}else{
		parser(sockfd, line, len);
	}
	//write prompt
	write(sockfd, "% ", 2);
	return 0;
}
int process_request(int sockfd){
	//deal with 3 case: 
	//		1.	aaaaaaaaa\r\n
	//		2.	aaaaaaaaa\r\nbbbbbbbbbb
	//		3.	aaaaaaaaaa
	//			aa\r\n
	int rc, i, j;
	//int fill_back_flag = 0;
	int enterflag = 0;
	char line[MAXSIZE];
	int read_end_flag = 0;
	//change dir
	//if(chdir(getenv("HOME")) < 0){
	//	err_dump("cd error");
	//}
	//if(chdir("ras") < 0){
	//	err_dump("cd error");
	//}
	//if(setenv("PATH", "bin:.", 1) != 0){//failed
	//	write(sockfd, "setenv failed", (strlen("setenv failed")) * sizeof(char));
	//}

	while(1)
	{
		enterflag = 0;
		if(read_end_flag == 0)
			rc = read(sockfd, line + line_offset, MAXLENG - line_offset);
		if(rc == 0)
			read_end_flag = 1;
		if(rc < 0){
			err_dump_sock(sockfd, "readline:read error\n");
		}else if(read_end_flag == 1 && line_offset == 0){//finished
			break;
		}else{
			//may contain not one command
			for(i = 0 ; i < rc + line_offset ; i++){// xxxxx\r\n
				if( (line[i] == '\n' || line[i] == '\r')){//remove telnet newline // xxxxxx\0\0
					enterflag = 1;
					line[i] = '\0';//i == length of first command
				}else if(line[i] != '\n' && line[i] != '\r' && enterflag == 1){//contain more than one command
					break;
				}
			}
			//read a not completed command
			if(enterflag == 0){
				line_offset += rc;
				break;
			}
			//test command is built-in or not(eg. exit, setenv, printenv or nornal command)
			if(build_in_or_command(sockfd, line, i) == 1)
				return 1; // client type "exit"
			if(i < rc + line_offset){//contain more than one command
				//clear
				for(j = 0 ; j < i ; j++){
					line[j] = '\0';
				}
				//fill back
				for(j = 0 ; j + i < rc + line_offset ; j++){
					line[j] = line[j + i];
					line[j + i] = '\0';
				}
				line_offset = j;
			}else if(i == rc + line_offset){//reset
				memset(line, 0, MAXSIZE);
				line_offset = 0;
				break;
			}
		}
	}
	return 0;
}
void sort_user(int usercount){
	struct User temp;
	int i = 0, j = 0;
	for(i = 0 ; i < usercount - 1 ; i++){
		for(j = i ; j >= 0 ; j--){
			//swap
			if(user[j].id > user[j + 1].id){
				temp = user[j];
				user[j] = user[j + 1];
				user[j + 1] = temp;
			}
		}
	}
}
int min_unused_user_id(int *usercount){
	int i = 0;
	int unused_id = 1;
	for(i = 0 ; i < *usercount ; i++){
		if(unused_id == user[i].id){
			unused_id++;
		}else
			return unused_id;
	}
	return unused_id;
}
void add_user(struct sockaddr_in cli_addr, int *usercount, int newsockfd){
	char port_buff[6];
	user[*usercount].id = min_unused_user_id(usercount);
	user[*usercount].fd = newsockfd;
	strcpy(user[*usercount].name, "(no name)");
	snprintf(port_buff, sizeof(port_buff), "%hu", cli_addr.sin_port);
	//inet_ntoa is for ipv4
	strcpy(user[*usercount].ip_port, inet_ntoa(cli_addr.sin_addr));
	strcat(user[*usercount].ip_port, "/");
	strcat(user[*usercount].ip_port, port_buff);
	(*usercount)++;
	sort_user(*usercount);
}
void del_user(int fd, int *usercount){
	int del_idx = fd_idx(fd);
	user[del_idx].id = 1000;
	user[del_idx].fd = -1;
	memset(user[del_idx].name, 0, sizeof(user[del_idx].name));
	memset(user[del_idx].ip_port, 0, sizeof(user[del_idx].ip_port));
	sort_user(*usercount);
	(*usercount)--;

}
int main(int argc, char* argv[])
{
	memset(user_pipefd, 0, sizeof(user_pipefd));
	int msockfd;//master sock
	struct sockaddr_in cli_addr;
	fd_set rfds; // read file descriptor set
	fd_set afds; // active file descriptor set
	int fd, nfds;
	socklen_t alen; // client address length
	msockfd = passiveTCP(SERV_TCP_PORT, 5);

	nfds = getdtablesize();
	FD_ZERO(&afds);
	FD_SET(msockfd, &afds);
	while(1)
	{
		memcpy(&rfds, &afds, sizeof(rfds));

		if(select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0){
			err_dump("select error");
		}
		// new connection from client
		if(FD_ISSET(msockfd, &rfds)){
			int newsockfd;
			alen = sizeof(cli_addr);
			newsockfd = accept(msockfd, (struct sockaddr *)&cli_addr, &alen);
			if(newsockfd < 0){
				err_dump("accept error");
			}
			FD_SET(newsockfd, &afds);
			//add a user
			add_user(cli_addr, &usercount, newsockfd);
			//write welcome message and prompt
			write(newsockfd, WELCOME_MESSAGE, strlen(WELCOME_MESSAGE) * sizeof(char));
			login_broadcast(newsockfd);
			write(newsockfd, "% ", 2);
		}
		//proccess requests
		for(fd = 0 ; fd < nfds ; fd++){
			if(fd != msockfd && FD_ISSET(fd, &rfds)){
				// client type "exit"
				if(process_request(fd) == 1){
					logout_yell(fd);
					del_user(fd, &usercount);
					close(fd);
					FD_CLR(fd, &afds);
				}
			}
		}
	}
}
