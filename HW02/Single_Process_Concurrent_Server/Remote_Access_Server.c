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


extern struct User user[30];
//user_pipefd[from user id][to user id]
extern int user_pipefd[31][31][2];
extern char path[30][100];

int err_dump_sock(int sockfd, char* msg){
	dup2(sockfd, 2);
	perror(msg);
	return 1;
}
int err_dump_sock_v(int sockfd, char* msg, char* v, char* msg2){
	char* result = NULL;
	int size = (strlen(msg) + strlen(v) + strlen(msg2) + 2) * sizeof(char);
	result = malloc(size);
	memset(result, 0, size);
	strcpy(result, msg);
	strcat(result, v);
	strcat(result, msg2);
	strcat(result, "\n");
	write(sockfd, result, size - 1);
	return 1;
}
void close_user_pipe(int myid, int in_userid, int out_userid){
	if(in_userid > 0){
		close(user_pipefd[in_userid][myid][0]);
		user_pipefd[in_userid][myid][0] = 0;
		user_pipefd[in_userid][myid][1] = 0;
	}
	if(out_userid > 0){
		close(user_pipefd[myid][out_userid][1]);
	}
}
int check_arg_valid(int sockfd, char* arg){
	int myidx = fd_idx(sockfd);
	char *temp_path = malloc(strlen(path[user[myidx].id]) + 1);
	
	strcpy(temp_path, path[user[myidx].id - 1]);
	
	int i = 0;
	for(i = 0 ; i < 30 && user[myidx].Path[i][0] != 0 ; i++){
		char file[(strlen(user[myidx].Path[i]) + strlen("/") + strlen(arg) + 1)];
		snprintf(file, sizeof(file), "%s/%s", user[myidx].Path[i], arg);
		if(access(file, X_OK) == 0){
			free(temp_path);
			return 0;
		}
	}
	free(temp_path);
	return -1;
}
int parser(int sockfd, char* line, int len){//it also call exec
	char *buff;
	int pid;
	int wpid;
	int status = 0;
	int argcount = 0;
	char *arg[checkarg(line, " ", len)];
	int exec_result = 0;
	char *infile = NULL, *outfile = NULL;
	int pipe_in_fd[2] = {0};
	int out_userid = -1, in_userid = -1;
	char oneline[strlen(line) + 1];
	strcpy(oneline, line);
	buff = strtok(line, " ");
	arg[argcount] = malloc((strlen(buff) + 1) * sizeof(char));
	strcpy(arg[argcount++], buff);
	arg[argcount] = NULL;
	if(check_arg_valid(sockfd, arg[0]) != 0){
		err_dump_sock_v(sockfd, "Unknown command: [", buff, "].");
			int i;
			int myidx = fd_idx(sockfd);
			//check if numbered pipe should be execute
			for(i = 0 ; i < user[myidx].cmdcount ; i++){
				user[myidx].command[i].idx++;
				// there's a pipe_in to this cmd // already execute
				if(user[myidx].command[i].idx == user[myidx].command[i].count){
					free_command(sockfd, i, &(user[myidx].cmdcount));
					//because of filling back
					i--;
				}
			}
		return -1;
	}
	while(1)//not cmd: > < |n |
	{
		buff = strtok(NULL, " ");
		if(buff == NULL && argcount > 0){// with pipe before or not, aka "...... | cmd" or "cmd"//no more pipe!!
			int myidx = fd_idx(sockfd);
			read_timed_command(sockfd, pipe_in_fd, &(user[myidx].cmdcount), 0);
			//real command
			my_execvp(sockfd, &pid, pipe_in_fd, infile, outfile, arg, in_userid, out_userid);
			close_user_pipe(user[fd_idx(sockfd)].id, in_userid, out_userid);
			in_userid = -1;
			out_userid = -1;
			//parent
			free_arg(arg, &argcount);
			//free file
			if(infile != NULL){
				free(infile);
				infile = NULL;
			}
			if(outfile != NULL){
				free(outfile);
				outfile = NULL;
			}
			close_pipe_in_fd(sockfd, pipe_in_fd);
			//wait for all child
			while ((wpid = wait(&status)) > 0);
			break;
		}else if(buff == NULL && argcount == 0){
			break;
		}else if(buff[0] == '|' && strlen(buff) >= 1 && argcount > 0){// numbered pipe // store it
			int myidx = fd_idx(sockfd);
			//initial a command
			user[myidx].command[user[myidx].cmdcount].idx = 0;
			//initial_command_count
			initial_command_count(&(user[myidx].command[user[myidx].cmdcount]), buff);
			initial_command_refd_pipe_fd(sockfd, user[myidx].cmdcount);
			//put it here to avoid idx++ of this command
			(user[myidx].cmdcount)++;
			read_timed_command(sockfd, user[myidx].command[user[myidx].cmdcount - 1].pipe_in_fd, &(user[myidx].cmdcount), 1);
			mypipe(sockfd, user[myidx].command[user[myidx].cmdcount - 1].pipe_out_fd);
			exec_result = my_execvp_cmd(sockfd, &pid, user[myidx].cmdcount - 1, infile, arg, in_userid);
			close_user_pipe(user[fd_idx(sockfd)].id, in_userid, out_userid);
			in_userid = -1;
			out_userid = -1;
			parent_close(sockfd, user[myidx].command[user[myidx].cmdcount - 1].pipe_in_fd, user[myidx].command[user[myidx].cmdcount - 1].pipe_out_fd);
			free(infile);
			infile = NULL;
			free_arg(arg, &argcount);
			while ((wpid = wait(&status)) > 0);
			if(exec_result == 1){
				break;
			}

		}else if(buff[0] == '|' && strlen(buff) >= 1 && argcount == 0){
			int i;
			int myidx = fd_idx(sockfd);
			//check if numbered pipe should be execute
			for(i = 0 ; i < user[myidx].cmdcount ; i++){
				user[myidx].command[i].idx++;
				// there's a pipe_in to this cmd // already execute
				if(user[myidx].command[i].idx == user[myidx].command[i].count){
					free_command(sockfd, i, &(user[myidx].cmdcount));
					//because of filling back
					i--;
				}
			}
			//return err_dump_sock(sockfd, "continual pipe");
		}else if(buff[0] == '>' && strlen(buff) > 1 && argcount > 0){//pipe to other user
			// buff+1 to skip '>'
			int userid = atoi(buff + 1);
			int useridx = id_idx(userid);
			int myidx = fd_idx(sockfd);
			int myid = user[myidx].id;
			out_userid = userid;
			//check if pipe exist
			if(user_pipefd[myid][userid][0] > 0){
				//*** Error: the pipe #2->#1 already exists. *** 
				char errmsg[strlen("*** Error: the pipe #-># already exists. ***\n") + 2 + 2 + 1];
				snprintf(errmsg, sizeof(errmsg), "*** Error: the pipe #%d->#%d already exists. ***\n", myid, userid);
				simple_tell(sockfd, errmsg);
				return 1;
			}else if(myid == userid){
				char errmsg[strlen("*** Error: the pipe #-># should not be the same id. ***\n") + 2 + 2 + 1];
				snprintf(errmsg, sizeof(errmsg), "*** Error: the pipe #%d->#%d should not be the same id. ***\n", myid, userid);
				simple_tell(sockfd, errmsg);
				int i;
				//check if numbered pipe should be execute
				for(i = 0 ; i < user[myidx].cmdcount ; i++){
					user[myidx].command[i].idx++;
					// there's a pipe_in to this cmd // already execute
					if(user[myidx].command[i].idx == user[myidx].command[i].count){
						free_command(sockfd, i, &(user[myidx].cmdcount));
						//because of filling back
						i--;
					}
				}
				return 1;
			}else if(check_id_exist(sockfd, buff + 1) == -1){
				//*** Error: user #6 does not exist yet. ***
				
				int i;
				//check if numbered pipe should be execute
				for(i = 0 ; i < user[myidx].cmdcount ; i++){
					user[myidx].command[i].idx++;
					// there's a pipe_in to this cmd // already execute
					if(user[myidx].command[i].idx == user[myidx].command[i].count){
						free_command(sockfd, i, &(user[myidx].cmdcount));
						//because of filling back
						i--;
					}
				}
				return 1;
			}
			//yell *** IamUser (#3) just piped 'cat test.html | cat >1' to Iam1 (#1) ***
			char msg[strlen("***  (#) just piped '' to  (#) ***") + strlen(user[myidx].name) + 2 +	strlen(oneline) + strlen(user[useridx].name) + 2 + 2];
			snprintf(msg, sizeof(msg), "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", user[myidx].name, myid, oneline, user[useridx].name, userid);
			simple_broadcast(sockfd, msg);
			//simple_broadcast(sockfd, "% ");
			pipe(user_pipefd[myid][userid]);
		}else if(buff[0] == '<' && strlen(buff) > 1 && argcount > 0){//pipe from other user
			// buff+1 to skip '<'
			int userid = atoi(buff + 1);
			int useridx = id_idx(userid);
			int myidx = fd_idx(sockfd);
			int myid = user[myidx].id;
			in_userid = userid;
			//check if pipe exist
			if(user_pipefd[userid][myid][0] == 0){
				//*** Error: the pipe #1->#2 does not exist yet. *** 
				char errmsg[strlen("*** Error: the pipe #-># does not exist yet. ***\n") + 2 + 2 + 1];
				snprintf(errmsg, sizeof(errmsg), "*** Error: the pipe #%d->#%d does not exist yet. ***\n", userid, myid);
				simple_tell(sockfd, errmsg);
				return 1;
			}else if(myid == userid){
				char errmsg[strlen("*** Error: the pipe #-># should not be the same id. ***\n") + 2 + 2 + 1];
				snprintf(errmsg, sizeof(errmsg), "*** Error: the pipe #%d->#%d should not be the same id. ***\n", myid, userid);
				simple_tell(sockfd, errmsg);
				int i;
				//check if numbered pipe should be execute
				for(i = 0 ; i < user[myidx].cmdcount ; i++){
					user[myidx].command[i].idx++;
					// there's a pipe_in to this cmd // already execute
					if(user[myidx].command[i].idx == user[myidx].command[i].count){
						free_command(sockfd, i, &(user[myidx].cmdcount));
						//because of filling back
						i--;
					}
				}
				return 1;
			}else if(check_id_exist(sockfd, buff + 1) == -1){
				//*** Error: user #6 does not exist yet. ***
				
				int i;
				//check if numbered pipe should be execute
				for(i = 0 ; i < user[myidx].cmdcount ; i++){
					user[myidx].command[i].idx++;
					// there's a pipe_in to this cmd // already execute
					if(user[myidx].command[i].idx == user[myidx].command[i].count){
						free_command(sockfd, i, &(user[myidx].cmdcount));
						//because of filling back
						i--;
					}
				}
				return 1;
			}
			//yell *** IamUser (#3) just received from student7 (#7) by 'cat <7' ***
			char msg[strlen("***  (#) just received from  (#) by '' ***") + strlen(user[myidx].name) + 2 +	strlen(oneline) + strlen(user[useridx].name) + 2 + 2];
			snprintf(msg, sizeof(msg), "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", user[myidx].name, myid, user[useridx].name, userid, oneline);
			simple_broadcast(sockfd, msg);
			//simple_broadcast(sockfd, "% ");
		}
		else if(buff[0] == '>'){//redirect >
			buff = strtok(NULL, " ");//filename
			//copy filename
			if(outfile != NULL){
				free(outfile);
				outfile = NULL;
				return err_dump_sock(sockfd, "mutiple output file");
			}
			if(buff == NULL || buff[0] == '|' || buff[0] == '<' || buff[0] == '>'){
				return err_dump_sock(sockfd, "no filename");
			}
			outfile = malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(outfile, buff);
		}else if(buff[0] == '<'){//redirect <
			buff = strtok(NULL, " ");//filename
			//copy filename
			if(infile != NULL){
				free(infile);
				infile = NULL;
				return err_dump_sock(sockfd, "mutiple output file");
			}
			if(buff == NULL || buff[0] == '|' || buff[0] == '<' || buff[0] == '>'){
				return err_dump_sock(sockfd, "no filename");
			}
			infile = malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(infile, buff);
		}else if(buff != NULL){
			arg[argcount] = malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(arg[argcount++], buff);
			arg[argcount] = NULL;
			if(argcount == 0 && check_arg_valid(sockfd, arg[0]) != 0){
				err_dump_sock_v(sockfd, "Unknown command: [", buff, "].");
				return -1;
			}
		}
	}
	return 0;
}
void free_arg(char *arg[], int *argcount){
	int i;
	for(i = 0 ; i < *argcount ; i++){
		free(arg[i]);
		arg[i] = NULL;
	}
	*argcount = 0;
}
void read_timed_command(int sockfd, int *pipe_in_fd, int *cmdcount, int offset){
	int i;
	int myidx = fd_idx(sockfd);
	//check if numbered pipe should be execute
	for(i = 0 ; i < *cmdcount - offset ; i++){
		user[myidx].command[i].idx++;
		//execute it first
		if(user[myidx].command[i].idx == user[myidx].command[i].count){
			mypipe(sockfd, pipe_in_fd);
		}
	}
	// check if there's a pipe_in to this cmd // already execute
	for(i = 0 ; i < *cmdcount - offset ; i++){
		// there's a pipe_in to this cmd // already execute
		if(user[myidx].command[i].idx == user[myidx].command[i].count){
			read_in_write_out(sockfd, user[myidx].command[i].pipe_out_fd[0], pipe_in_fd[1]);
			//close out
			//won't close command[i].pipe_out_fd[1], cause it should already be closed
			free_command(sockfd, i, cmdcount);
			//because of filling back
			i--;
		}
	}
}
void read_in_write_out(int sockfd, int out_fd, int in_fd){
	int rc = 0, wc = 0;
	char tmpbuff[1000];
	do{
		//read
		rc = read(out_fd, tmpbuff, 1000);
		if(rc < 0){
			err_dump_sock(sockfd, "read error");
		}
		//write
		wc = write(in_fd, tmpbuff, rc);//will be read when command[cmdcount - 1] execute
		if(wc < 0){
			err_dump_sock(sockfd, "read error");
		}
	}while(rc == 1000);
}
void parent_close(int sockfd, int *pipe_in_fd, int *pipe_out_fd){
	if(pipe_in_fd[0] > 0){
		if(close(pipe_in_fd[0]) < 0){
			err_dump_sock(sockfd, "close pipe_in_fd[0] error");
		}
		pipe_in_fd[0] = 0;
	}
	if(pipe_in_fd[1] > 0){
		if(close(pipe_in_fd[1]) < 0){
			err_dump_sock(sockfd, "close pipe_in_fd[1] error");
		}
		pipe_in_fd[1] = 0;
	}
	if(pipe_out_fd[1] > 0){
		if(close(pipe_out_fd[1]) < 0){
			err_dump_sock(sockfd, "close pipe_in_fd[1] error");
		}
		pipe_out_fd[1] = 0;
	}
}
int my_execvp(int sockfd, int *pid, int *pipe_in_fd, char *infile, char *outfile, char *arg[], int in_userid, int out_userid){
	int refd_in = 0, refd_out = 0;
	//real command
	if((*pid = fork()) == 0){
		//change PATH to which stored for this client
		if(setenv("PATH", path[user[fd_idx(sockfd)].id - 1], 1) != 0){//failed
			write(sockfd, "change failed", (strlen("change failed")) * sizeof(char));
		}
		
		//determine_pipe_in_or_file_in
		if(pipe_in_fd[0] > 0 && infile == NULL){//input is pipe_in
			if(dup2(pipe_in_fd[0], 0) < 0){
				err_dump_sock(sockfd, "dup pipe_in_fd[0] error");
			}
			pipe_in_fd[0] = 0;
			if(pipe_in_fd[1] > 0 && close(pipe_in_fd[1]) < 0){
				err_dump_sock(sockfd, "close error");
			}
		}else if(pipe_in_fd[0] == 0 && infile != NULL){//input is file
			refd_in = open(infile, O_RDONLY);
			if(refd_in < 0){
				err_dump_sock(sockfd, "open infile error");
			}
			if(dup2(refd_in, 0) < 0){
				err_dump_sock(sockfd, "dup refd_in error");
			}
			refd_in = 0;
		}else if(in_userid >= 0){//input from other user
			close(user_pipefd[in_userid][user[fd_idx(sockfd)].id][1]);
			dup2(user_pipefd[in_userid][user[fd_idx(sockfd)].id][0], 0);
		}else if(pipe_in_fd[0] > 0 && infile != NULL){//input error
			err_dump_sock(sockfd, "pipe_in while has input file");
		}
		//only file_out could happen
		if(outfile != NULL){
			refd_out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if(refd_out < 0){
				err_dump_sock(sockfd, "open outfile error");
			}
			if(dup2(refd_out, 1) < 0){
				err_dump_sock(sockfd, "dup refd_out error");
			}
		}else if(out_userid >= 0){//pipe to other user
			close(user_pipefd[user[fd_idx(sockfd)].id][out_userid][0]);
			dup2(user_pipefd[user[fd_idx(sockfd)].id][out_userid][1], 1);
		}else if(outfile == NULL){
			dup2(sockfd, 1);
		}
		dup2(sockfd, 2);
		execvp(arg[0], arg);
		err_dump_sock_v(sockfd, "Unknown command: [", arg[0], "].");
		exit(1);
		//err_dump_sock(sockfd, "execvp failed");
	}else if(*pid < 0){
		return err_dump_sock(sockfd, "fork error");
	}
	return 0;
}
int my_execvp_cmd(int sockfd, int *pid, int i, char* infile, char* arg[], int in_userid){
	int refd_in_cmd = 0;
	int myidx = fd_idx(sockfd);
	//child
	if((*pid = fork()) == 0){
		//change PATH to which stored for this client
		if(setenv("PATH", path[user[fd_idx(sockfd)].id - 1], 1) != 0){//failed
			write(sockfd, "change failed", (strlen("change failed")) * sizeof(char));
		}
		//open file //outfile doesn't exist
		//won't happen while pipe_in exists
		if(infile != NULL && user[myidx].command[i].pipe_in_fd[0] == 0){//infile
			refd_in_cmd = open(infile, O_RDONLY);
			if(refd_in_cmd < 0){
				err_dump_sock(sockfd, "open error");
			}
			if(dup2(refd_in_cmd, 0) < 0){
				err_dump_sock(sockfd, "dup2 refd_in_cmd error");
			}
			close(refd_in_cmd);
			refd_in_cmd = 0;
		}else if(infile == NULL && user[myidx].command[i].pipe_in_fd[0] > 0){//pipe_in, then dup
			if(dup2(user[myidx].command[i].pipe_in_fd[0], 0) < 0){
				err_dump_sock(sockfd, "dup error");
			}
			if(user[myidx].command[i].pipe_in_fd[1] > 0 && close(user[myidx].command[i].pipe_in_fd[1]) < 0){
				err_dump_sock(sockfd, "close error");
			}
		}else if(in_userid >= 0){//input from other user
			close(user_pipefd[in_userid][user[fd_idx(sockfd)].id][1]);
			dup2(user_pipefd[in_userid][user[fd_idx(sockfd)].id][0], 0);
		}else if(infile != NULL && user[myidx].command[i].pipe_in_fd[0] > 0){
			err_dump_sock(sockfd, "inputfile while pipe_in");
		}
		//must pipe out
		if(dup2(user[myidx].command[i].pipe_out_fd[1], 1) < 0){
			err_dump_sock(sockfd, "dup error");
		}
		if(user[myidx].command[i].pipe_in_fd[0] > 0){
			if(close(user[myidx].command[i].pipe_in_fd[0]) < 0){
				err_dump_sock(sockfd, "close pipe_in_fd[0] error");
			}
			user[myidx].command[i].pipe_in_fd[0] = 0;
		}
		//execute
		dup2(sockfd, 2);
		execvp(arg[0], arg);
		err_dump_sock_v(sockfd, "Unknown command: [", arg[0], "].");
		exit(1);
		//err_dump_sock(sockfd, "execvp failed");
	}else if(*pid < 0){
		return err_dump_sock(sockfd, "fork error");
	}
	return 0;
}
void mypipe(int sockfd, int *pipe_fd){
	//pipe to real command
	if(pipe_fd[0] == 0 && pipe(pipe_fd) < 0){
		err_dump_sock(sockfd, "pipe_in_fd error");
	}
}
void initial_command_refd_pipe_fd(int sockfd, int commandcount){
	int myidx = fd_idx(sockfd);
	user[myidx].command[commandcount].pipe_in_fd[0] = 0;
	user[myidx].command[commandcount].pipe_in_fd[1] = 0;
	user[myidx].command[commandcount].pipe_out_fd[0] = 0;
	user[myidx].command[commandcount].pipe_out_fd[1] = 0;
}
void initial_command_count(struct numbered_pipe_command *command, char* buff){
	int i;
	if(strlen(buff) == 1){
		command -> count = 1;
	}else{
		char *pipe_num_str;
		pipe_num_str = malloc(strlen(buff) * sizeof(char));
		for(i = 1 ; i < strlen(buff) ; i++){
			pipe_num_str[i - 1] = buff[i];
		}
		pipe_num_str[i - 1] = '\0';
		command -> count = atoi(pipe_num_str);
	}
}
void close_pipe_in_fd(int sockfd, int *pipe_in_fd){
	if(pipe_in_fd[0] > 0){
		if(close(pipe_in_fd[0]) < 0){
			err_dump_sock(sockfd, "close pipe_in_fd[0] error");
		}
		pipe_in_fd[0] = 0;
	}
	if(pipe_in_fd[1] > 0){
		if(close(pipe_in_fd[1]) < 0){
			err_dump_sock(sockfd, "close pipe_in_fd[1] error");
		}
		pipe_in_fd[1] = 0;
	}
}
void free_command(int sockfd, int idx , int *commandcount){
	int i;
	int myidx = fd_idx(sockfd);
	if(user[myidx].command[idx].pipe_out_fd[0] > 0 && close(user[myidx].command[idx].pipe_out_fd[0]) < 0){
		err_dump_sock(sockfd, "close pipe_in_fd[1] error");
	}
	//free(command[idx].outfile);
	//fill back
	for(i = idx ; i < *commandcount - 1 ; i++){
		user[myidx].command[i] = user[myidx].command[i + 1];
	}
	//reset final one
	user[myidx].command[i].idx = 0;
	user[myidx].command[i].count = 0;
	//command[cmdcount].outfile = NULL;
	//****************************************************I change cmdcount to i here***************************************************************
	user[myidx].command[i].pipe_in_fd[0] = 0;
	user[myidx].command[i].pipe_in_fd[1] = 0;
	user[myidx].command[i].pipe_out_fd[0] = 0;
	user[myidx].command[i].pipe_out_fd[1] = 0;
	(*commandcount)--;
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
