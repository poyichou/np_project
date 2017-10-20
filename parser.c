#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#define MAXPIPE 500
#define MAXLENG 10001
#define MAXSIZE 10000

void err_dump(char*);
void process_request(int);
int readline(int, char*, int);
void parser(int, char*, int);
int checkarg(char*, char*, int);
void reset_io_redirection(char*, int*, char*, int*);
void change_pipecount(int*);
void close_all_pipe();
void close_pipe(int);
struct numbered_pipe_command{
	int idx;
	int count;
	char* arg[1000];
	char* outfile;
	char* infile;
};
void freeCommand(struct numbered_pipe_command);
int pipefd[2][2];
int pipecount = 0;//only parent can handle it
int pipe_flag = 0;
int cmdcount = 0;
struct numbered_pipe_command command[1001];
int main(int argc, char** argv)
{
	int rc, i, j, line_offset = 0;
	int fill_back_flag = 0;
	char line[MAXSIZE];
	while(1)
	{
		write(1, "% ", 2);
		rc = read(1, line + line_offset, MAXLENG - line_offset);
		if(rc < 0){
			err_dump("readline:read error");
		}else if(rc == 0 && line_offset == 0){//finished
			exit(0);
		}else{//may contain not one command
			for(i = 0 ; i < rc + line_offset ; i++){
				if( (line[i] == '\n' || line[i] == '\0') && i < (rc - 1) ){//contain more than one command
					line[i] = '\0';//i == length of first command
					parser(1, line, i);
					for(j = 0 ; j < rc + line_offset -i ; j++){
						line[j] = line[j+i];
					}
					line_offset = rc + line_offset - i;
					fill_back_flag = 1;
					break;
				}else if(line[i] == '\n'){
					line[i] = '\0';
					rc--;
				}
			}
			if(fill_back_flag == 1){
				fill_back_flag = 0;
				break;
			}
			//contain only one command
			line[rc + line_offset] = '\0';
			parser(1, line, rc + line_offset);
		}
	}
	return 0;
}
void parser(int fd, char* line, int len){//it also call exec
	char *buff;
	int pid;
	int wpid;
	int status = 0;
	int argcount = 0;
	char *arg[checkarg(line, " ", len)];
	int i;
	int refd_in = 0, refd_out = 0;
	char *infile = NULL, *outfile = NULL;

	buff = strtok(line, " ");
	arg[argcount] = malloc((strlen(buff) + 1) * sizeof(char));
	strcpy(arg[argcount++], buff);
	arg[argcount] = NULL;

	if(buff[0] == '|'){//pipe occur!! weird!!
		err_dump("parser:first token is |");
	}
	while(1)//not cmd: > < |n |
	{
		buff = strtok(NULL, " ");
		if(buff == NULL){// with pipe before or not, aka "...... | cmd" or "cmd"//no more pipe!!
			arg[argcount] = NULL;

			if((pid = fork()) == 0){//child
				//no pipe_out
				//precheck io_redirection, pipe_in
				if(refd_out > 0){
					if(dup2(refd_out, 1) < 0){
						err_dump("dup error");
					}
					refd_out = 0;
					free(outfile);
					outfile = NULL;
				}
				if(pipe_flag == 1 && refd_in <= 0){
					if(dup2(pipefd[pipecount][0], 0) < 0){
						err_dump("dup error");
					}
					if(close(pipefd[pipecount][1]) < 0){
						err_dump("close error");
					}
					pipefd[pipecount][1] = 0;
				}else if(pipe_flag == 0 && refd_in > 0){
					if(dup2(refd_in, 0) < 0){
						err_dump("dup error");
					}
					refd_in = 0;
					free(infile);
					infile = NULL;
				}else if(pipe_flag == 1 && refd_in > 0){
					err_dump("pipe in while has input file");
				}
				/**********************dup2(sockfd, 1);***************************************/
				execvp(arg[0], arg);
				err_dump("execvp failed");
			}

			//parent
			close_pipe(pipecount);
			pipe_flag = 0;
			//change pipecount
			change_pipecount(&pipecount);
			//reset in out redirection
			reset_io_redirection(infile, &refd_in, outfile, &refd_out);
			//free arg
			for(i = 0 ; i < argcount ; i++){
				free(arg[i]);
				arg[i] = NULL;
			}
			argcount = 0;

			while ((wpid = wait(&status)) > 0);//wait for all child
			break;
		}else if(buff[0] == '>'){//redirect >
			if(outfile != NULL){
				err_dump("mutiple output file");
			}
			buff = strtok(NULL, " ");//filename
			if(buff == NULL || buff[0] == '|' || buff[0] == '<' || buff[0] == '>'){
				err_dump("no filename");
			}
			outfile = malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(outfile, buff);
			refd_out = open(buff, O_WRONLY | O_CREAT, 0666);
			if(refd_out < 0){
				err_dump("open error");
			}
		}else if(buff[0] == '<'){//redirect <
			if(infile != NULL){
				err_dump("mutiple input file");
			}
			buff = strtok(NULL, " ");//filename
			if(buff == NULL || buff[0] == '|' || buff[0] == '<' || buff[0] == '>'){
				err_dump("no filename");
			}
			infile = malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(infile, buff);
			refd_in = open(buff, O_RDONLY);
			if(refd_in < 0){
				err_dump("open error");
			}
		}else if(buff[0] == '|' && ((strlen(buff) == 1) || (strlen(buff) == 2 && buff[1] == '1'))){// | or |1 (the same)
			//pipe
			if(pipecount == 0){
				if(pipe(pipefd[pipecount + 1]) < 0){
					err_dump("pipe error");
				}
			}else if(pipecount == 1){
				if(pipe(pipefd[pipecount - 1]) < 0){
					err_dump("pipe error");
				}
			}

			if((pid = fork()) == 0){// child
				//precheck o_redirection
				if(refd_out > 0){
					err_dump("pipe out while has output file");
				}
				//dup, pipe_out
				if(pipecount == 0){
					if(dup2(pipefd[pipecount + 1][1], 1) < 0){
						err_dump("dup error");
					}
					if(close(pipefd[pipecount + 1][0]) < 0){
						err_dump("close error");
					}
					pipefd[pipecount + 1][0] = 0;
				}else if(pipecount == 1){
					if(dup2(pipefd[pipecount - 1][1], 1) < 0){
						err_dump("dup error");
					}
					if(close(pipefd[pipecount - 1][0]) < 0){
						err_dump("close error");
					}
					pipefd[pipecount - 1][0] = 0;
				}
				//precheck i_redirection, pipe_in
				if(pipe_flag == 1 && refd_in <= 0){
					if(dup2(pipefd[pipecount][0], 0) < 0){
						err_dump("dup error");
					}
					if(close(pipefd[pipecount][1]) < 0){
						err_dump("close error");
					}
					pipefd[pipecount][1] = 0;
				}else if(pipe_flag == 0 && refd_in > 0){
					if(dup2(refd_in, 0) < 0){
						err_dump("dup error");
					}
					refd_in = 0;
					free(infile);
					infile = NULL;
				}else if(pipe_flag == 1 && refd_in > 0){
					err_dump("pipe in while has input file");
				}

				execvp(arg[0], arg);
				err_dump("execvp failed");
			}
			//parent
			close_pipe(pipecount);
			pipe_flag = 1;
			//change pipecount
			change_pipecount(&pipecount);
			//reset in out redirection
			reset_io_redirection(infile, &refd_in, outfile, &refd_out);

			//free arg
			for(i = 0 ; i < argcount ; i++){
				free(arg[i]);
				arg[i] = NULL;
			}
			argcount = 0;

			while ((wpid = wait(&status)) > 0);//wait for all child
		}
		else if(buff != NULL){
			arg[argcount] = malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(arg[argcount++], buff);
			arg[argcount] = NULL;
		}
		//waitpid(pid, &status, WNOHANG);
		//while ((wpid = wait(&status)) > 0);//wait for all child
	}
	//while ((wpid = wait(&status)) > 0);//wait for all child
	//waitpid(pid, &status, WNOHANG);
}
void reset_io_redirection(char* infile, int* refd_in, char* outfile, int* refd_out){
	if(*refd_out > 0){
		close(*refd_out);
		*refd_out = 0;
		free(outfile);
		outfile = NULL;
	}
	if(*refd_in > 0){
		close(*refd_in);
		*refd_in = 0;
		free(infile);
		infile = NULL;
	}
}
void close_all_pipe(){
	if(pipefd[0][0] > 0 && close(pipefd[0][0]) < 0) err_dump("close error");
	if(pipefd[0][1] > 0 && close(pipefd[0][1]) < 0) err_dump("close error");
	if(pipefd[1][0] > 0 && close(pipefd[1][0]) < 0) err_dump("close error");
	if(pipefd[1][1] > 0 && close(pipefd[1][1]) < 0) err_dump("close error");
}
void close_pipe(int count){
	if(pipefd[count][0] > 0 && close(pipefd[count][0]) < 0) err_dump("close error");
	if(pipefd[count][0] > 0 && close(pipefd[count][1]) < 0) err_dump("close error");
}
void change_pipecount(int* p){
	if(*p == 0){
		*p = 1;
	}else if(*p == 1){
		*p = 0;
	}
}
void err_dump(char* msg){
	perror(msg);
	exit(1);
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
