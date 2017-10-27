#define MAXLENG	10000
#define MAXSIZE 10001
const char WELCOME_MESSAGE[] =	"****************************************\n"
				"** Welcome to the information server. **\n"
				"****************************************\n";

#define SERV_TCP_PORT 5055
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
struct numbered_pipe_command command[1000];

int  checkarg(char* str, char* delim, int len);
void myprintenv(int sockfd);
void err_dump(char* msg);
int  err_dump_sock(int sockfd, char* msg);
int  err_dump_sock_v(int sockfd, char* msg, char* v, char* msg2);
void process_request(int sockfd);
int  parser(int sockfd, char* line, int len);
void free_command(int sockfd, int idx , int *commandcount);
void close_pipe_in_fd(int sockfd, int *pipe_in_fd);
void initial_command_count(struct numbered_pipe_command *command, char* buff);
void initial_command_refd_pipe_fd(int commandcount);
void mypipe(int sockfd, int *pipe_fd);
int  my_execvp_cmd(int sockfd, int *pid, int i, char* infile, char* arg[]);
int  my_execvp(int sockfd, int *pid, int *pipe_in_fd, char *infile, char *outfile, char *arg[]);
void parent_close(int sockfd, int *pipe_infd, int *pipe_out_fd);
void read_in_write_out(int sockfd, int out_fd, int in_fd);
int  build_in_or_command(int sockfd, char* line, int len);
void read_timed_command(int sockfd, int *pipe_in_fd, int *cmdcount, int offset);
void free_arg(char *arg[], int *argcount);

int pipefd[1000][2];
int pipe_flag = 0;
int cmdcount = 0;
char *pathes[256];

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
void process_request(int sockfd){
	int rc, i, j, line_offset = 0;
	//int fill_back_flag = 0;
	int enterflag = 0;
	char line[MAXSIZE];
	int read_end_flag = 0;
	int linecount = 0;

	write(sockfd, WELCOME_MESSAGE, strlen(WELCOME_MESSAGE) * sizeof(char));
	while(1)
	{
		linecount++;
		enterflag = 0;
		write(sockfd, "% ", 2);
		if(read_end_flag == 0){
			rc = read(sockfd, line + line_offset, MAXLENG - line_offset);
		}
		if(rc < 0){
			err_dump_sock(sockfd, "readline:read error\n");
		}else if(rc == 0){
			read_end_flag = 1;
		}else if(read_end_flag == 1 && line_offset == 0){//finished
			exit(0);
		}else{
			//may contain not one command
			for(i = 0 ; i < rc + line_offset ; i++){
				if( (line[i] == '\n' || line[i] == '\r')){//remove telnet newline
					enterflag = 1;
					line[i] = '\0';//i == length of first command
				}else if(line[i] != '\n' && line[i] != '\r' && enterflag == 1){//contain more than one command
					break;
				}
			}
			//test command is built-in or not(eg. exit, setenv, printenv or nornal command)
			if(build_in_or_command(sockfd, line, i) == 1){
				exit(0);
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
			}
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
		//printenv
		char tmp_line[MAXSIZE];
		char* vname;
		int wc = 0;
		strcpy(tmp_line, line);
		vname = strtok(tmp_line, " ");
		vname = strtok(NULL, " ");//name
		if(vname == NULL){//no arg
			myprintenv(sockfd);
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
	}else if(strncmp(line, "setenv", 6) == 0){
		//setenv
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
	}else{
		parser(sockfd, line, len);
	}
	return 0;
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

	buff = strtok(line, " ");
	arg[argcount] = malloc((strlen(buff) + 1) * sizeof(char));
	strcpy(arg[argcount++], buff);
	arg[argcount] = NULL;
	while(1)//not cmd: > < |n |
	{
		buff = strtok(NULL, " ");
		if(buff == NULL && argcount > 0){// with pipe before or not, aka "...... | cmd" or "cmd"//no more pipe!!
			read_timed_command(sockfd, pipe_in_fd, &cmdcount, 0);
			//real command
			my_execvp(sockfd, &pid, pipe_in_fd, infile, outfile, arg);
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
			//initial a command
			command[cmdcount].idx = 0;
			//initial_command_count
			initial_command_count(&command[cmdcount], buff);
			initial_command_refd_pipe_fd(cmdcount);
			//put it here to avoid idx++ of this command
			cmdcount++;
			read_timed_command(sockfd, command[cmdcount - 1].pipe_in_fd, &cmdcount, 1);
			mypipe(sockfd, command[cmdcount - 1].pipe_out_fd);
			exec_result = my_execvp_cmd(sockfd, &pid, cmdcount - 1, infile, arg);
			parent_close(sockfd, command[cmdcount - 1].pipe_in_fd, command[cmdcount - 1].pipe_out_fd);
			free(infile);
			infile = NULL;
			free_arg(arg, &argcount);
			while ((wpid = wait(&status)) > 0);
			if(exec_result == 1){
				break;
			}

		}else if(buff[0] == '|' && strlen(buff) >= 1 && argcount == 0){
			return err_dump_sock(sockfd, "continual pipe");
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
	//check if numbered pipe should be execute
	for(i = 0 ; i < *cmdcount - offset ; i++){
		command[i].idx++;
		//execute it first
		if(command[i].idx == command[i].count){
			mypipe(sockfd, pipe_in_fd);
		}
	}
	// check if there's a pipe_in to this cmd // already execute
	for(i = 0 ; i < *cmdcount - offset ; i++){
		// there's a pipe_in to this cmd // already execute
		if(command[i].idx == command[i].count){
			read_in_write_out(sockfd, command[i].pipe_out_fd[0], pipe_in_fd[1]);
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
int my_execvp(int sockfd, int *pid, int *pipe_in_fd, char *infile, char *outfile, char *arg[]){
	int refd_in = 0, refd_out = 0;
	//real command
	if((*pid = fork()) == 0){
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
int  my_execvp_cmd(int sockfd, int *pid, int i, char* infile, char* arg[]){
	int refd_in_cmd = 0;
	//child
	if((*pid = fork()) == 0){
		//open file //outfile doesn't exist
		//won't happen while pipe_in exists
		if(infile != NULL && command[i].pipe_in_fd[0] == 0){//infile
			refd_in_cmd = open(infile, O_RDONLY);
			if(refd_in_cmd < 0){
				err_dump_sock(sockfd, "open error");
			}
			if(dup2(refd_in_cmd, 0) < 0){
				err_dump_sock(sockfd, "dup2 refd_in_cmd error");
			}
			close(refd_in_cmd);
			refd_in_cmd = 0;
		}else if(infile == NULL && command[i].pipe_in_fd[0] > 0){//pipe_in, then dup
			if(dup2(command[i].pipe_in_fd[0], 0) < 0){
				err_dump_sock(sockfd, "dup error");
			}
			if(command[i].pipe_in_fd[1] > 0 && close(command[i].pipe_in_fd[1]) < 0){
				err_dump_sock(sockfd, "close error");
			}
		}else if(infile != NULL && command[i].pipe_in_fd[0] > 0){
			err_dump_sock(sockfd, "inputfile while pipe_in");
		}
		//must pipe out
		if(dup2(command[i].pipe_out_fd[1], 1) < 0){
			err_dump_sock(sockfd, "dup error");
		}
		if(command[i].pipe_in_fd[0] > 0){
			if(close(command[i].pipe_in_fd[0]) < 0){
				err_dump_sock(sockfd, "close pipe_in_fd[0] error");
			}
			command[i].pipe_in_fd[0] = 0;
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
void initial_command_refd_pipe_fd(int commandcount){
	command[commandcount].pipe_in_fd[0] = 0;
	command[commandcount].pipe_in_fd[1] = 0;
	command[commandcount].pipe_out_fd[0] = 0;
	command[commandcount].pipe_out_fd[1] = 0;
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
	if(command[idx].pipe_out_fd[0] > 0 && close(command[idx].pipe_out_fd[0]) < 0){
		err_dump_sock(sockfd, "close pipe_in_fd[1] error");
	}
	//free(command[idx].outfile);
	//fill back
	for(i = idx ; i < *commandcount - 1 ; i++){
		command[i] = command[i + 1];
	}
	//reset final one
	command[i].idx = 0;
	command[i].count = 0;
	//command[cmdcount].outfile = NULL;
	command[cmdcount].pipe_in_fd[0] = 0;
	command[cmdcount].pipe_in_fd[1] = 0;
	command[cmdcount].pipe_out_fd[0] = 0;
	command[cmdcount].pipe_out_fd[1] = 0;
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
void myprintenv(int sockfd){
	int i;
	extern char **environ;
	for(i= 0 ; environ[i] != NULL ; i++){
		write(sockfd, environ[i], (strlen(environ[i])) * sizeof(char));
		write(sockfd, "\n", (strlen("\n")) * sizeof(char));
	}
	return;
}
