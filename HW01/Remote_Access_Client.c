#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#define MAXLENG 10000
void err_sys(char * msg){
	perror(msg);
	exit(1);
}
int main(int argc, char **argv)
{
	//usage: <program> <server ip> <server port>
	fd_set readfds;
	int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *he;
	short SERV_TCP_PORT = 0;
	if(argc != 3){
		err_sys("usage error: <program> <server ip> <server port>");
	}else{
		if((he = gethostbyname(argv[1])) == NULL){
			err_sys("usage error: <program> <server ip> <server port>");
		}
		SERV_TCP_PORT = (short)atoi(argv[2]);
	}
	memset((char*)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr = *(struct in_addr *)he -> h_addr;
	serv_addr.sin_port = htons(SERV_TCP_PORT);
	// Open a TCP socket (an Internet stream socket).
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		err_sys("client: can't connect to server");
	}
	if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("client:connect error");
		exit(1);
	}
	//send_request
	char send_str[MAXLENG + 1] = "\0";
	int len = 0;
	char receive_str[MAXLENG + 1] = "\0";
	int rc = 0;
	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		FD_SET(fileno(stdin),&readfds);
		if(select(sockfd + 1, &readfds, NULL, NULL, NULL) < 0){
			exit(1);
		}
		//read from client
		if(FD_ISSET(fileno(stdin), &readfds)){
			if(fgets(send_str, MAXLENG, stdin) == NULL){
				return 0;
			}
			if(strncmp("exit", send_str, 4) == 0){
				fputs("connection to server closed\n", stdout);
				fflush(stdout);
				exit(0);
			}
			len = strlen(send_str);
			send_str[len] = 10;
			send_str[len - 1] = 13;

			if(write(sockfd, send_str, (len + 1) * sizeof(char)) < 0){
				err_sys("send_request: write error");
			}
		}
		//read prompt from server
		if(FD_ISSET(sockfd, &readfds)){
			while(1)
			{
				rc = read(sockfd, receive_str, MAXLENG);
				if(rc < 0){//error
					err_sys("send_request: read error");
				}else if(rc == 0){//finished
					break;
				}else if(rc  < MAXLENG){
					receive_str[rc] = '\0';
					fputs(receive_str, stdout);
					fflush(stdout);
					break;
				}else if(rc == MAXLENG){
					write(1, receive_str, rc);
				}
				//if rc == MAXLENG, read again
			}
		}
	}
	close(sockfd);
	exit(0);
}

