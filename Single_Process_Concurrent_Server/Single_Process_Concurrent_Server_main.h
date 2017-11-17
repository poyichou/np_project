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
	int id;//user id, not nessecarily equal sockid
	int fd;//user fd
	char name[20];//user name
	char ip_port[30];//<ip>/<port>
	struct numbered_pipe_command command[1000];
	char Path[30][256];
};
void err_dump(char* msg);
int passiveTCP(int port, int qlen);
void printenv_all(int sockfd);
void print_env(int sockfd, char *line);
void set_env(int sockfd, char* line);
int fd_idx(int fd);
int id_idx(int id);
int check_id_exist(int myfd, char *userid);
void simple_tell(int fd, char* msg);
void tell(int myfd, char* line);
void simple_broadcast(int myfd, char* msg);
void broadcast(int myfd, char* line);
void name(int myfd, char* line);
void print_all_user(int myfd);
int build_in_or_command(int sockfd, char* line, int len);
int process_request(int sockfd);
void sort_user(int usercount);
int min_unused_user_id(int *usercount);
void add_user(struct sockaddr_in cli_addr, int *usercount, int newsockfd);
void del_user(int fd, int *usercount);
void set_Path_to_user(int sockfd, char *Path);
