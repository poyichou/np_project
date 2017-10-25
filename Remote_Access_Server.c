#define MAXLENG	10000
#define MAXSIZE 10001
const char WELCOME_MESSAGE[] =	"**************************************************************\n"
				"** Welcome to the information server, myserver.nctu.edu.tw. **\n"
				"**************************************************************\n"
				"** You are in the directory, /home/studentA/ras/.\n"
				"** This directory will be under \"/\", in this system.\n"
				"** This directory includes the following executable programs.\n"
				"** \n"
				"**	bin/\n"
				"**	test.html	(test file)\n"
				"**\n"
				"** The directory bin/ includes:\n"
				"**	cat\n"
				"**	ls\n"
				"**	removetag		(Remove HTML tags.)\n"
				"**	removetag0		(Remove HTML tags with error message.)\n"
				"**	number			(Add a number in each line.)\n"
				"**\n"
				"** In addition, the following two commands are supported by ras. \n"
				"**	setenv\n"
				"**	printenv\n"
				"**\n";

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
	char* arg[1000];
	char* infile;
	int refd_in;
	int refd_out;
	int pipe_out_fd[2];
	int pipe_in_fd[2];
};
struct numbered_pipe_command command[1000];

void mysetenv(char* ,char* , int);
void printenv();
void err_dump(char*);
void process_request(int);
int readline(int, char*, int);
void parser(int, char*, int);
int checkarg(char*, char*, int);
void close_pipe(int);
void redirection(char *, char *, int *, int);
void free_command(int, int *);
void close_pipe_in_fd(int *);
void initial_command_count(struct numbered_pipe_command *, char*);
void initial_command_refd_pipe_fd(int);
void pipe_out_pipe_in(int *, int *);
void my_execvp_cmd(int *, int);
void my_execvp(int, int *, int *, char *, char *, char *[]);
void parent_close(int *pipe_infd, int *pipe_out_fd);
void read_in_write_out(int, int);

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
void process_request(int sockfd){
	int rc, i, j, line_offset = 0;
	//int fill_back_flag = 0;
	int enterflag = 0;
	char line[MAXSIZE];
	char *PATH = getenv("PATH");
	char my_PATH[256];
	char *buff;
	i=0;
	strcpy(my_PATH, PATH);
	buff = strtok(my_PATH, ":");
	while(buff != NULL)
	{
		pathes[i] = malloc((strlen(buff) + 1) * sizeof(char));
		strcpy(pathes[i++], buff);
		buff = strtok(NULL, ":");
	}
	pathes[i] = NULL;

	write(sockfd, WELCOME_MESSAGE, strlen(WELCOME_MESSAGE) * sizeof(char));
	while(1)
	{
		enterflag = 0;
		write(sockfd, "% ", 2);
		rc = read(sockfd, line + line_offset, MAXLENG - line_offset);
		if(rc < 0){
			err_dump("readline:read error\n");
		}else if(rc == 0 && line_offset == 0){//finished
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
				//if( (line[i] == '\n' || line[i] == '\0' || line[i] == '\r') && i < (rc + line_offset) ){//contain more than one command
				//	line[i] = '\0';//i == length of first command
				//	parser(sockfd, line, i);
				//	for(j = 0 ; j < rc + line_offset -i ; j++){
				//		line[j] = line[j + rc + line_offset];
				//	}
				//	line_offset = rc + line_offset - i;
				//	fill_back_flag = 1;
				//	break;
				//}
			}
			//if(fill_back_flag == 1){
			//	fill_back_flag = 0;
			//	break;
			//}
			parser(sockfd, line, i);
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
			//contain only one command
			//line[rc + line_offset] = '\0';
			//parser(sockfd, line, rc + line_offset);
		}
	}
}
void parser(int fd, char* line, int len){//it also call exec
	char *buff;
	int pid;
	int wpid;
	int status = 0;
	int argcount = 0;
	char *arg[checkarg(line, " ", len)];
	int i;
	char *infile = NULL, *outfile = NULL;
	int pipe_in_fd[2] = {0};
	buff = strtok(line, " ");
	arg[argcount] = malloc((strlen(buff) + 1) * sizeof(char));
	strcpy(arg[argcount++], buff);
	arg[argcount] = NULL;
	//test if it is valid
	char tmp_path[256];
	int passflag = 0;
	for(i = 0 ; pathes[i] != NULL ; i++){
		tmp_path[0] = '\0';
		strcpy(tmp_path, pathes[i]);
		strcat(tmp_path, "/");
		strcat(tmp_path, buff);
		//printf("%s", tmp_path);
		if(access(tmp_path, X_OK) == 0){
			passflag = 1;
			break;
		}
	}
	//only one invalid command in one line
	if(passflag == 0){
		for(i = 0 ; i < cmdcount ; i++){
			if(++command[i].idx == command[i].count){
				free_command(i, &cmdcount);
			}
		}
		err_dump("Command not found");
	}

	//if(buff[0] == '|'){//pipe occur!! weird!!
	//	err_dump("parser:first token is |");
	//}
	while(1)//not cmd: > < |n |
	{
		buff = strtok(NULL, " ");
		if(buff == NULL && argcount > 0){// with pipe before or not, aka "...... | cmd" or "cmd"//no more pipe!!
			//check if numbered pipe should be execute
			for(i = 0 ; i < cmdcount ; i++){
				command[i].idx++;
				//execute it first
				if(command[i].idx == command[i].count){
					pipe_out_pipe_in(command[i].pipe_out_fd, pipe_in_fd);
					//child
					my_execvp_cmd(&pid, i);
					//parent
					parent_close(command[i].pipe_in_fd, command[i].pipe_out_fd);
				}
			}
			// check if there's a pipe_in to this cmd // already execute
			for(i = 0 ; i < cmdcount ; i++){
				// there's a pipe_in to this cmd // already execute
				if(command[i].idx == command[i].count){
					read_in_write_out(command[i].pipe_out_fd[0], pipe_in_fd[1]);
					//close out
					//won't close command[i].pipe_out_fd[1], cause it should already be closed
					free_command(i, &cmdcount);
					//because of filling back
					i--;
				}
			}
			//real command
			my_execvp(fd, &pid, pipe_in_fd, infile, outfile, arg);
			//parent

			//free arg
			for(i = 0 ; i < argcount ; i++){
				free(arg[i]);
				arg[i] = NULL;
			}
			argcount = 0;
			//free file
			if(infile != NULL){
				free(infile);
				infile = NULL;
			}
			if(outfile != NULL){
				free(outfile);
				outfile = NULL;
			}
			close_pipe_in_fd(pipe_in_fd);
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
			for(i = 0 ; i < argcount ; i++){
				command[cmdcount].arg[i] = malloc((strlen(arg[i]) + 1) * sizeof(char));
				strcpy(command[cmdcount].arg[i], arg[i]);
				free(arg[i]);
				arg[i] = NULL;
			}
			command[cmdcount].arg[i] = NULL;
			argcount = 0;
			//initial_command_file
			if(outfile != NULL){
				free(outfile);
				outfile = NULL;
				err_dump("pipe out while has output file");
			}
			if(infile != NULL){
				command[cmdcount].infile = malloc((strlen(infile) + 1) * sizeof(char));
				strcpy(command[cmdcount].infile, infile);
				free(infile);
				infile = NULL;
			}
			initial_command_refd_pipe_fd(cmdcount);
			//put it here to avoid idx++ of this command
			cmdcount++;
			//check if numbered pipe should be execute
			for(i = 0 ; i < cmdcount - 1 ; i++){
				command[i].idx++;
				//execute it first
				if(command[i].idx == command[i].count){
					pipe_out_pipe_in(command[i].pipe_out_fd, command[cmdcount - 1].pipe_in_fd);
					//child
					my_execvp_cmd(&pid, i);
					//parent
					parent_close(command[i].pipe_in_fd, command[i].pipe_out_fd);
				}
			}
			// check if there's a pipe_in to this cmd // already execute
			for(i = 0 ; i < cmdcount - 1 ; i++){
				// there's a pipe_in to this cmd // already execute
				if(command[i].idx == command[i].count){
					read_in_write_out(command[i].pipe_out_fd[0], command[cmdcount - 1].pipe_in_fd[1]);
					//won't close command[i].pipe_out_fd[1], cause it should already be closed
					free_command(i, &cmdcount);
					//because of filling back
					i--;
				}
			}
			while ((wpid = wait(&status)) > 0);

		}else if(buff[0] == '|' && strlen(buff) >= 1 && argcount == 0){
			err_dump("continual pipe");
		}
		else if(buff[0] == '>'){//redirect >
			buff = strtok(NULL, " ");//filename
			//copy filename
			if(outfile != NULL){
				free(outfile);
				err_dump("mutiple output file");
			}
			if(buff == NULL || buff[0] == '|' || buff[0] == '<' || buff[0] == '>'){
				err_dump("no filename");
			}
			outfile = malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(outfile, buff);
			//redirection(outfile, buff, &refd_out, 1);
		}else if(buff[0] == '<'){//redirect <
			buff = strtok(NULL, " ");//filename
			//copy filename
			if(infile != NULL){
				free(infile);
				infile = NULL;
				err_dump("mutiple input file");
			}
			if(buff == NULL || buff[0] == '|' || buff[0] == '<' || buff[0] == '>'){
				err_dump("no filename");
			}
			infile = malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(infile, buff);
			//redirection(infile, buff, &refd_in, 0);
		}else if(buff != NULL){
			//test if it exists
			if(argcount == 0){
				for(i = 0 ; pathes[i] != NULL ; i++){
					tmp_path[0] = '\0';
					strcpy(tmp_path, pathes[i]);
					strcat(tmp_path, "/");
					strcat(tmp_path, buff);
					if(access(tmp_path, X_OK) == 0){
						passflag = 1;
					}
				}
				if(passflag == 0){//only one invalid command in one line
					err_dump("Command not found");
					break;
				}
			}
			arg[argcount] = malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(arg[argcount++], buff);
			arg[argcount] = NULL;
		}
	}
}
void read_in_write_out(int out_fd, int in_fd){
	int rc = 0, wc = 0;
	char tmpbuff[1000];
	do{
		//read
		rc = read(out_fd, tmpbuff, 1000);
		if(rc < 0){
			err_dump("read error");
		}
		//write
		wc = write(in_fd, tmpbuff, rc);//will be read when command[cmdcount - 1] execute
		if(wc < 0){
			err_dump("read error");
		}
	}while(rc == 1000);
}
void parent_close(int *pipe_in_fd, int *pipe_out_fd){
	if(pipe_in_fd[0] > 0){
		if(close(pipe_in_fd[0]) < 0){
			err_dump("close pipe_in_fd[0] error");
		}
		pipe_in_fd[0] = 0;
	}
	if(pipe_in_fd[1] > 0){
		if(close(pipe_in_fd[1]) < 0){
			err_dump("close pipe_in_fd[1] error");
		}
		pipe_in_fd[1] = 0;
	}
	if(pipe_out_fd[1] > 0){
		if(close(pipe_out_fd[1]) < 0){
			err_dump("close pipe_in_fd[1] error");
		}
		pipe_out_fd[1] = 0;
	}
}
void my_execvp(int fd, int *pid, int *pipe_in_fd, char *infile, char *outfile, char *arg[]){
	int refd_in = 0, refd_out = 0;
	//real command
	if((*pid = fork()) == 0){
		//determine_pipe_in_or_file_in
		if(pipe_in_fd[0] > 0 && infile == NULL){//input is pipe_in
			if(dup2(pipe_in_fd[0], 0) < 0){
				err_dump("dup pipe_in_fd[0] error");
			}
			pipe_in_fd[0] = 0;
			if(close(pipe_in_fd[1]) < 0){
				err_dump("close error");
			}
		}else if(pipe_in_fd[0] == 0 && infile != NULL){//input is file
			refd_in = open(infile, O_RDONLY);
			if(refd_in < 0){
				err_dump("open infile error");
			}
			if(dup2(refd_in, 0) < 0){
				err_dump("dup refd_in error");
			}
			refd_in = 0;
		}else if(pipe_in_fd[0] > 0 && infile != NULL){//input error
			err_dump("pipe_in while has input file");
		}
		//only file_out could happen
		if(outfile != NULL){
			refd_out = open(outfile, O_WRONLY | O_CREAT, 0666);
			if(refd_out < 0){
				err_dump("open outfile error");
			}
			if(dup2(refd_out, 1) < 0){
				err_dump("dup refd_out error");
			}
		}else if(outfile == NULL){
			dup2(fd, 1);
		}
		execvp(arg[0], arg);
		err_dump("execvp failed");
	}else if(*pid < 0){
		err_dump("fork error");
	}
}
void my_execvp_cmd(int *pid, int i){
	int refd_in_cmd = 0;
	//child
	if((*pid = fork()) == 0){
		//open file //outfile doesn't exist
		//won't happen while pipe_in exists
		if(command[i].infile != NULL && command[i].pipe_in_fd[0] == 0){//infile
			refd_in_cmd = open(command[i].infile, O_RDONLY);
			if(refd_in_cmd < 0){
				err_dump("open error");
			}
			if(dup2(refd_in_cmd, 0) < 0){
				err_dump("dup2 refd_in_cmd error");
			}
			refd_in_cmd = 0;
		}else if(command[i].infile == NULL && command[i].pipe_in_fd[0] > 0){//pipe_in, then dup
			if(dup2(command[i].pipe_in_fd[0], 0) < 0){
				err_dump("dup error");
			}
			if(close(command[i].pipe_in_fd[1]) < 0){
				err_dump("close error");
			}
		}else if(command[i].infile != NULL && command[i].pipe_in_fd[0] > 0){
			err_dump("inputfile while pipe_in");
		}
		//must pipe out
		if(dup2(command[i].pipe_out_fd[1], 1) < 0){
			err_dump("dup error");
		}
		if(command[i].pipe_out_fd[0] > 0){
			if(close(command[i].pipe_in_fd[0]) < 0){
				err_dump("close pipe_in_fd[0] error");
			}
			command[i].pipe_in_fd[0] = 0;
		}
		//execute
		execvp(command[i].arg[0], command[i].arg);
		err_dump("execvp failed");
	}else if(*pid < 0){
		err_dump("fork error");
	}
}
void pipe_out_pipe_in(int *pipe_out_fd, int *pipe_in_fd){
	//pipe to real command
	if(pipe_in_fd[0] == 0 && pipe(pipe_in_fd) < 0){
		err_dump("pipe_in_fd error");
	}
	//pipe to self pipe_out_fd
	if(pipe(pipe_out_fd) < 0){
		err_dump("pipe_out_fd error");
	}
}
void initial_command_refd_pipe_fd(int commandcount){
	command[commandcount].refd_in = 0;
	command[commandcount].refd_out = 0;
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
void close_pipe_in_fd(int *pipe_in_fd){
	if(pipe_in_fd[0] > 0){
		if(close(pipe_in_fd[0]) < 0){
			err_dump("close pipe_in_fd[0] error");
		}
		pipe_in_fd[0] = 0;
	}
	if(pipe_in_fd[1] > 0){
		if(close(pipe_in_fd[1]) < 0){
			err_dump("close pipe_in_fd[1] error");
		}
		pipe_in_fd[1] = 0;
	}
}
void free_command(int idx , int *commandcount){
	int i;
	for(i = 0 ; command[idx].arg[i] != NULL ; i++){
		free(command[idx].arg[i]);
	}
	//free(command[idx].outfile);
	free(command[idx].infile);
	//fill back
	for(i = idx ; i < *commandcount - 1 ; i++){
		command[i] = command[i + 1];
	}
	//reset final one
	command[i].idx = 0;
	command[i].count = 0;
	for(i = 0 ; command[i].arg[i] != NULL ; i++){
		command[cmdcount].arg[i] = NULL;
	}
	//command[cmdcount].outfile = NULL;
	command[cmdcount].infile = NULL;
	command[cmdcount].pipe_in_fd[0] = 0;
	command[cmdcount].pipe_in_fd[1] = 0;
	command[cmdcount].pipe_out_fd[0] = 0;
	command[cmdcount].pipe_out_fd[1] = 0;
	(*commandcount)--;
}
void redirection(char *file, char *buff, int *refd, int io){
	if(file != NULL){
		err_dump("mutiple input file");
	}
	if(buff == NULL || buff[0] == '|' || buff[0] == '<' || buff[0] == '>'){
		err_dump("no filename");
	}
	file = malloc((strlen(buff) + 1) * sizeof(char));
	strcpy(file, buff);
	//if(io == 0){
	//	*refd = open(buff, O_RDONLY);
	//}else if(io == 1){
	//	*refd = open(buff, O_WRONLY | O_CREAT, 0666);
	//}else{
	//	err_dump("redirection 4th argument error");
	//}
	//if(*refd < 0){
	//	err_dump("open error");
	//}
}
void close_pipe(int count){
	if(pipefd[count][0] > 0 && close(pipefd[count][0]) < 0) err_dump("close error");
	if(pipefd[count][1] > 0 && close(pipefd[count][1]) < 0) err_dump("close error");
	pipefd[count][0] = 0;
	pipefd[count][1] = 0;
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
