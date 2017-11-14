#define MAXLENG	10000
#define MAXSIZE 10001
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

struct User{
	int id;//user id, equal sockid
	char name[20];//user name
	char ip_port[30];//<ip>/<port>
}
struct User user[30];
int usercount = 0;

int line_offset = 0;

int  err_dump_sock(int sockfd, char* msg);
int  err_dump_sock_v(int sockfd, char* msg, char* v, char* msg2);

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
	}else{
		parser(sockfd, line, len);
	}
	return 0;
}
void process_request(int sockfd){
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
		if(enterflag == 1)
			write(sockfd, "% ", 2);
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
	int i = 0;
	for(i = 0 ; i < usercount - 1 ; i++){
		//swap
		if(user[i].id > user[i + 1].id){
			temp = user[i];
			user[i] = user[i + 1];
			user[i + 1] = temp;
		}
	}
}
void add_user(struct sockaddr_in cli_addr, int *usercount, int newsockfd){
	char port_buff[6];
	user[*usercount].id = newsockfd;
	strcpy(user[*usercount].name, "(no name)");
	sprintf(port_buff, "%hu", cli_addr.sin_port);
	strcpy(user[*usercount].ip_port, ntoa(cli_addr.sin_addr));
	strcat(user[*usercount].ip_port, "/");
	strcat(user[*usercount].ip_port, port_buff);
	*usercount++;
	sort_user(*usercount);
}

int main(int argc, char* argv[])
{
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
		if(FD_SET(msockfd, rfds)){
			int newsockfd;
			char port[10];
			alen = sizeof(cli_addr);
			newsockfd = accept(msockfd, (struct sock_addr *)&cli_addr, &alen);
			if(newsockfd < 0){
				err_dump("accept error");
			}
			FD_SET(newsockfd, &afds);
			//add a user
			add_user(cli_addr, &usercount, newsockfd);
			//write welcome message and prompt
			write(sockfd, WELCOME_MESSAGE, strlen(WELCOME_MESSAGE) * sizeof(char));
			write(sockfd, "% ", 2);
		}
		//proccess requests
		for(fd = 0 ; fd < nfds ; fd++){
			if(fd != msockfd && FD_ISSET(fd, &rfds)){
				// client type "exit"
				if(process_request(fd) == 1){
					close(fd);
					FD_CLR(fd, &afds);
				}
			}
		}
	}
}
void err_dump(char* msg){
	perror(msg);
	exit(1);
}
