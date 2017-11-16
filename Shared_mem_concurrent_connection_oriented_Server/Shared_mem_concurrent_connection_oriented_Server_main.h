#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<signal.h>

struct Msg{
	//int target_id;
	int msgcount;
	char message[10][1024 + 1];
};

struct User{
	int pid;
	int id;
	int fd;
	char name[20];
	char ip_port[21 + 1];
	int sendflag;
	struct Msg msg[31];//tell, yell to other user
};

struct Shared_Mem{
	struct User user[30];
	int usercount;
	int first_flag;
	int fifo_flag[31][31];
	int to_close_fd[30];
};
int passiveTCP(int port, int qlen);
void printenv_all(int sockfd);
void print_env(int sockfd, char *line);
void set_env(int sockfd, char* line);
int fd_idx(int fd);
int id_idx(int id);
int check_id_exist(int myfd, char *userid);
void add_msg(int myid, int targetid, char *msg);
void simple_tell(int myid, int targetid, char* msg);
void tell(int myfd, char* line);
void simple_yell(int myid, char* msg);
void simple_broadcast(int myid, char* msg);
void yell(int myfd, char* line);
void broadcast(int myfd, char* line);
void name(int myfd, char* line);
void print_all_user(int myfd);
void detatch_and_delete();
void sig_to_user(int userid, int signo);
int build_in_or_command(int sockfd, char* line, int len);
void recv_msg();
void server_close_fd();
void reaper(int signo);
int process_request(int sockfd);
void sort_user(int usercount);
int min_unused_user_id(int *usercount);
void add_user(struct sockaddr_in cli_addr, int *usercount, int newsockfd);
void del_user(int fd, int *usercount);
void get_shared_mem();
