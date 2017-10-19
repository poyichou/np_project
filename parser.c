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

int pipefd[MAXPIPE][2];
int pipecount = 0;//only parent can handle it
int main(int argc, char** argv)
{
	int rc, i, j, line_offset = 0;
	int fill_back_flag = 0;
	char line[MAXSIZE];
	for(i = 0 ; i < MAXPIPE ; i++){
		if(pipe(pipefd[i]) < 0){
			err_dump("pipe");
		}
		//printf("pipefd[0]=%d, pipefd[1]=%d\n", pipefd[i][0], pipefd[i][1]);
	}
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
		if(buff == NULL){
			//no more pipe!!
			arg[argcount] = NULL;
			argcount = 0;
			if((pid = fork()) == 0){
				if(refd_out > 0){
					dup2(refd_out, 1);
				}
				if(refd_in > 0){
					dup2(refd_in, 0);
				}
				if(pipecount > 0){
					dup2(pipefd[pipecount - 1][0], 0);//last pipe read
					close(pipefd[pipecount - 1][1]);
				}
				/**********************dup2(sockfd, 1);***************************************/
				for(i = 0 ; i < MAXPIPE ; i++){
					close(pipefd[i][0]);
					close(pipefd[i][1]);
				}
				execvp(arg[0], arg);
				err_dump("execvp failed");
			}
			//parent
			if(refd_out > 0){
				close(refd_out);
			}
			if(refd_in > 0){
				close(refd_in);
			}
			break;
		}else if(buff[0] == '>'){//redirect >
			buff = strtok(NULL, " ");//filename
			if(buff == NULL || buff[0] == '|' || buff[0] == '<' || buff[0] == '>'){
				err_dump("no filename");
			}
			refd_out = open(buff, O_WRONLY | O_CREAT, 0666);
			if(refd_out < 0){
				err_dump("open error");
			}
		}else if(buff[0] == '<'){//redirect <
			buff = strtok(NULL, " ");//filename
			if(buff == NULL || buff[0] == '|' || buff[0] == '<' || buff[0] == '>'){
				err_dump("no filename");
			}
			refd_in = open(buff, O_RDONLY);
			if(refd_in < 0){
				err_dump("open error");
			}
		}else if(buff != NULL){
			arg[argcount] = malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(arg[argcount++], buff);
			arg[argcount] = NULL;
		}
		//waitpid(pid, &status, WNOHANG);
		//while ((wpid = wait(&status)) > 0);//wait for all child
	}
	while ((wpid = wait(&status)) > 0);//wait for all child
	//waitpid(pid, &status, WNOHANG);
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
