#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>

struct numbered_pipe_command{
	int idx;
	int count;
	int pipe_out_fd[2];
	int pipe_in_fd[2];
};
void err_dump(char* msg);
int err_dump_sock(int sockfd, char* msg);
int err_dump_sock_v(int sockfd, char* msg, char* v, char* msg2);
void ulink_user_fifo(int myid, int in_userid);
int parser(int sockfd, char* line, int len);//it also call exec
void free_arg(char *arg[], int *argcount);
void read_timed_command(int sockfd, int *pipe_in_fd, int *cmdcount, int offset);
void read_in_write_out(int sockfd, int out_fd, int in_fd);
void parent_close(int sockfd, int *pipe_in_fd, int *pipe_out_fd);
int my_execvp(int sockfd, int *pid, int *pipe_in_fd, char *infile, char *outfile, char *arg[], int in_userid, int out_userid);
int my_execvp_cmd(int sockfd, int *pid, int i, char* infile, char* arg[], int in_userid);
void mypipe(int sockfd, int *pipe_fd);
void initial_command_refd_pipe_fd(int commandcount);
void initial_command_count(struct numbered_pipe_command *command, char* buff);
void close_pipe_in_fd(int sockfd, int *pipe_in_fd);
void free_command(int sockfd, int idx , int *commandcount);
int checkarg(char* str, char* delim, int len);
