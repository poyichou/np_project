#define MAXLENG	10000
#define MAXSIZE 10001
#define DEMO	1
const char WELCOME_MESSAGE[] =	"****************************************\n"
				"** Welcome to the information server. **\n"
				"****************************************\n";

#define SERV_TCP_PORT 7000
#define SHMKEY ((key_t) 7890) /*base value for shmem key*/
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
#include "Remote_Access_Server_Shared.h"
#include "Shared_mem_concurrent_connection_oriented_Server_main.h"

struct Shared_Mem *memptr;
int my_userid_global;
int line_offset = 0;
int shmid;
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
int id_idx(int id){
	int i;
	for(i = 0 ; i < memptr -> usercount ; i++){
		//find myself
		if(memptr -> user[i].id == id){
			return i;
		}
	}
	return -1;
}
int check_id_exist(int myfd, char *userid){
	int i;
	int userid_i = atoi(userid);
	for(i = 0 ; i < memptr -> usercount ; i++){
		//find
		if(memptr -> user[i].id == userid_i){
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
void add_msg(int myid, int targetid, char *msg){
	//myid must != targetid
	int msgcount = memptr -> user[id_idx(myid)].msg[targetid].msgcount;
	(memptr -> user[id_idx(myid)].sendflag) += 1;
	strcpy(memptr -> user[id_idx(myid)].msg[targetid].message[msgcount], msg);
	(memptr -> user[id_idx(myid)].msg[targetid].msgcount)++;
}
void simple_tell(int myid, int targetid, char* msg){
	if(myid == targetid && write(memptr -> user[id_idx(myid)].fd, msg, strlen(msg)) < 0){
		err_dump("write error");
	}else if(myid != targetid){
		add_msg(myid, targetid, msg);
		sig_to_user(targetid, SIGUSR1);
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
	int myidx = id_idx(my_userid_global);
	//*** IamUser told you ***: Hello World.
	char msg[strlen("***  told you ***: ") + strlen(memptr -> user[myidx].name) + strlen(line) + 2];
	if(check_id_exist(myfd, buff) < 0){//user not exist
		return;
	}
	snprintf(msg, sizeof(msg), "*** %s told you ***: ", memptr -> user[myidx].name);
	//get msg
	buff = strtok(NULL, " ");
	while(buff != NULL)
	{
		strcat(msg, buff);
		strcat(msg, " ");
		buff = strtok(NULL, " ");
	}
	strcat(msg, "\n");
	simple_tell(memptr -> user[myidx].id, userid, msg);
	//simple_tell(user[id_idx(userid)].fd, "% ");
}
void simple_yell(int myid, char* msg){
	int i;
	for(i = 0 ; i < memptr -> usercount ; i++){
		if(memptr -> user[i].id != myid){
			simple_tell(myid, memptr -> user[i].id, msg);
		}
	}
}
void simple_broadcast(int myid, char* msg){
	int i;
	for(i = 0 ; i < memptr -> usercount ; i++){
		simple_tell(myid, memptr -> user[i].id, msg);
	}
}
void yell(int myfd, char* line){
	int myidx = id_idx(my_userid_global);
	//*** IamUser yelled ***: Hi everybody
	char msg[strlen("***  yelled ***: ") + strlen(memptr -> user[myidx].name) + strlen(line) + 2];
	char* buff;
	snprintf(msg, sizeof(msg), "*** %s yelled ***: ", memptr -> user[myidx].name);
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
	simple_yell(memptr -> user[myidx].id, msg);
	//simple_yell(myfd, "% ");
}
void login_broadcast(int myid){
	int myidx = id_idx(myid);
	char msg[strlen("*** User '' entered from . ***") + 20 + 30 + 2];
	snprintf(msg, sizeof(msg), "*** User '%s' entered from %s. ***\n", memptr -> user[myidx].name, memptr -> user[myidx].ip_port);
	simple_broadcast(myid, msg);
}
void logout_broadcast(int myid){
	int myidx = id_idx(myid);
	char msg[strlen("*** User '' left. ***") + 20 + 2];
	snprintf(msg, sizeof(msg), "*** User '%s' left. ***\n", memptr -> user[myidx].name);
	simple_broadcast(myid, msg);
}
void initialize_env(){
	clearenv();
	if(setenv("PATH", "bin:.", 1) != 0){//failed
		err_dump("setenv failed");
	}
}
void broadcast(int myfd, char* line){
	int myidx = id_idx(my_userid_global);
	//*** IamUser yelled ***: Hi everybody
	char msg[strlen("***  yelled ***: ") + strlen(memptr -> user[myidx].name) + strlen(line) + 2];
	char* buff;
	snprintf(msg, sizeof(msg), "*** %s yelled ***: ", memptr -> user[myidx].name);
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
	simple_broadcast(memptr -> user[myidx].id, msg);
	//simple_broadcast(myfd, "% ");
}
void name(int myfd, char* line){
	char* name;
	name = strtok(line, " ");
	name = strtok(NULL, " ");//name
	int i;
	for(i = 0 ; i < memptr -> usercount ; i++){
		if(memptr -> user[i].id != my_userid_global && strcmp(memptr -> user[i].name, name) == 0){
			char msg[strlen("*** User '' already exists. ***\n") + strlen(name) + 1];
			snprintf(msg, sizeof(msg), "*** User '%s' already exists. ***\n", name);
			simple_tell(memptr -> user[id_idx(my_userid_global)].id, memptr -> user[id_idx(my_userid_global)].id, msg);
			return;
		}
	}
	int myidx = id_idx(my_userid_global);
	strcpy(memptr -> user[myidx].name, name);
	//yell *** User from (IP/port) is named '(name)'. ***
	char msg[strlen("*** User from  is named ''. ***\n") + strlen(memptr -> user[myidx].ip_port) + strlen(name) + 1];
	snprintf(msg, sizeof(msg), "*** User from %s is named '%s'. ***\n", memptr -> user[myidx].ip_port, name);
	simple_broadcast(memptr -> user[myidx].id, msg);
	//simple_broadcast(myfd, "% ");
}
void print_all_user(int myfd){
	int i = 0;
	int myid = memptr -> user[id_idx(my_userid_global)].id;
	char msg[3 + 1 + 20 + 1 + 6 + 1 + strlen("<-me") + 2];
	write(myfd, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n", strlen("<ID>\t<nickname>\t<IP/port>\t<indicate me>\n"));
	for(i = 0 ; i < memptr -> usercount ; i++){
		memset(msg, 0, sizeof(msg));
		snprintf(msg, sizeof(msg), "%d\t%s\t%s", memptr -> user[i].id,
												 memptr -> user[i].name, 
												 memptr -> user[i].ip_port);
		if(memptr -> user[i].id == myid){
			strcat(msg, "\t<-me");
		}
		strcat(msg, "\n");
		simple_tell(memptr -> user[id_idx(my_userid_global)].id, memptr -> user[id_idx(my_userid_global)].id, msg);
	}
}
void detatch_and_delete(){
	//no user left, so clean
	if(memptr -> usercount == 0){
		if(shmdt(memptr) < 0) 
			err_dump("server: can't detach shared memory");
		if (shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0) < 0) 
			err_dump("cannot remove shm");
	}else{
		if(shmdt(memptr) < 0) 
			err_dump("server: can't detach shared memory");
	}
}
void sig_to_user(int userid, int signo){
	int i;
	int myidx = id_idx(my_userid_global);
	if(userid == -1){//yell sig to siblings
		for(i = 0 ; i < memptr -> usercount ; i++){
			if(i != myidx)
				sig_to_user(memptr -> user[i].pid, signo);
		}
	}else if(userid > 0){//signal to one user
		kill(memptr -> user[id_idx(userid)].pid, signo);
	}else{
		err_dump("sig_to_user:usage error");
	}
}
int build_in_or_command(int sockfd, char* line, int len){
	if (strcmp(line, "/") == 0){
		write(sockfd, "\"/\" is blocked\n", strlen("\"/\" is blocked\n"));
		return 0;
	}else if(strncmp(line, "exit", 4) == 0){//exit
		logout_broadcast(my_userid_global);
		del_user(sockfd, &(memptr -> usercount));
		if(shmdt(memptr) < 0) 
			err_dump("server: can't detach shared memory");
		//detatch_and_delete();
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
void recv_msg(){
	int i;
	int j;
	//for(i = 0 ; i < memptr -> usercount ; i++){
	for(i = 0 ; i < 30 ; i++){
		if(memptr -> user[i].sendflag > 0){
			//send to me !!
			//for(j = 0 ; j < memptr -> user[i].msg[my_userid_global].msgcount ; j++){
			for(j = 0 ; j < 10 && memptr -> user[i].msg[my_userid_global].msgcount > 0 ; j++){
				write((memptr -> user[id_idx(my_userid_global)].fd),
					(memptr -> user[i].msg[my_userid_global].message[j]), strlen(memptr -> user[i].msg[my_userid_global].message[j]));
				memset(memptr -> user[i].msg[my_userid_global].message[j], 0, sizeof(memptr -> user[i].msg[my_userid_global].message[0]));
				(memptr -> user[i].sendflag) --;
				(memptr -> user[i].msg[my_userid_global].msgcount)--;
			}
			//total msg < msg to a specific target
			if((memptr -> user[i].sendflag) < 0){
				err_dump("total msg < msg to a specific target");
			}
			return;
		}
	}
}
void reaper(int signo){
	if(signo == SIGCHLD){
		//union wait status;
		int status;
		waitpid(-1, &status, WNOHANG);
		//while(wait3(&status, WNOHANG, (struct rusage*)0) >= 0)
		//	/*empty*/;
	}else if(signo == SIGUSR1){//receive from tell, yell
		recv_msg();
	}
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
	signal(SIGUSR1, reaper);
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
			exit(0);
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
				continue;
			}
			//test command is built-in or not(eg. exit, setenv, printenv or nornal command)
			if(build_in_or_command(sockfd, line, i) == 1){
				if(close(sockfd) < 0){
					err_dump("close sockfd error");
				}
				exit(0); // client type "exit"
			}
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
			if(memptr -> user[j].id > memptr -> user[j + 1].id){
				temp = memptr -> user[j];
				memptr -> user[j] = memptr -> user[j + 1];
				memptr -> user[j + 1] = temp;
			}
		}
	}
}
int min_unused_user_id(int *usercount){
	int i = 0;
	int unused_id = 1;
	for(i = 0 ; i < *usercount ; i++){
		if(unused_id == memptr -> user[i].id){
			unused_id++;
		}else
			return unused_id;
	}
	return unused_id;
}
void add_user(struct sockaddr_in cli_addr, int *usercount, int newsockfd){
	memptr -> user[*usercount].pid = getpid();
	memptr -> user[*usercount].id = min_unused_user_id(usercount);
	my_userid_global = memptr -> user[*usercount].id;
	memptr -> user[*usercount].fd = newsockfd;
	memptr -> user[*usercount].sendflag = 0;
	strcpy(memptr -> user[*usercount].name, "(no name)");
	//inet_ntoa is for ipv4
	if(DEMO == 0){
		snprintf(memptr -> user[*usercount].ip_port, sizeof(memptr -> user[*usercount].ip_port), "%s/%hu", inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);
	}else if(DEMO == 1){
		snprintf(memptr -> user[*usercount].ip_port, sizeof(memptr -> user[*usercount].ip_port), "CGILAB/511");
	}
	memset(memptr -> user[*usercount].msg, 0, sizeof(memptr -> user[*usercount].msg));
	(*usercount)++;
	sort_user(*usercount);
}
void del_user(int fd, int *usercount){
	int del_idx = id_idx(my_userid_global);
	memptr -> user[del_idx].pid = -1;
	memptr -> user[del_idx].id = 1000;
	memptr -> user[del_idx].fd = -1;
	memset(memptr -> user[del_idx].name, 0, sizeof(memptr -> user[del_idx].name));
	memset(memptr -> user[del_idx].ip_port, 0, sizeof(memptr -> user[del_idx].ip_port));
	sort_user(*usercount);
	(*usercount)--;
}
void get_shared_mem(){
	shmid = shmget(SHMKEY, sizeof(struct Shared_Mem), 0666 | IPC_CREAT);
	//shmget error
	if(shmid < 0)
		err_dump("server: can't create shared memory");
	if((memptr = (struct Shared_Mem*)shmat(shmid, (char*)0, 0)) == (void *)-1) 
		err_dump("shmat error");
	//first create
	if(memptr -> first_flag != 1){//just create
		memset(memptr, 0, sizeof(struct Shared_Mem));
		memptr -> first_flag = 1;
	}
}
void del_shared_mem(){
	shmid = shmget(SHMKEY, sizeof(struct Shared_Mem), 0666 | IPC_CREAT);
	//shmget error
	if(shmid < 0){
		err_dump("server: can't create shared memory");
		printf("errno=%d\n",errno);
	}
	if((memptr = (struct Shared_Mem*)shmat(shmid, (char*)0, 0)) == (void *)-1) 
		err_dump("shmat error");
	//delete shared memory no matter it's which last server failed to delete or which getten this time
	if(shmdt(memptr) < 0) 
		err_dump("server: can't detach shared memory");
	if(shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0) < 0) 
		err_dump("cannot remove shm");
}
int main(int argc, char* argv[])
{
	int msockfd;//master sock
	int pid;
	struct sockaddr_in cli_addr;
	socklen_t alen; // client address length
	msockfd = passiveTCP(SERV_TCP_PORT, 5);
	//delete shared memory no matter it's which last server failed to delete or which getten this time
	del_shared_mem();


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
			err_dump("accept error");
		}
		if((pid = fork()) < 0){
			err_dump("fork error");
		}else if(pid == 0){
			close(msockfd);
			initialize_env();
			//get shared memory
			get_shared_mem();
			//add a user
			add_user(cli_addr, &(memptr -> usercount), newsockfd);
			//write welcome message and prompt
			write(newsockfd, WELCOME_MESSAGE, strlen(WELCOME_MESSAGE) * sizeof(char));
			login_broadcast(my_userid_global);
			write(newsockfd, "% ", 2);
			//proccess requests
			process_request(newsockfd);
			exit(0);
		}
		close(newsockfd);
	}
}
