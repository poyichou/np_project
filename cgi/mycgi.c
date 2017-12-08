//connect->read file->write to client->read response->read file->...
#define F_CONNECTING 0
#define F_WRITING 1
#define F_READING 2
#define F_DONE 4
#define MAXSIZE 256
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<fcntl.h>
#include<signal.h>
#include<errno.h>
#include<netdb.h>
#include<sys/socket.h>
#include <netinet/in.h>

char host[5][MAXSIZE];
int  port[5];
char file[5][MAXSIZE];
int  sockfd[5];
FILE *fp[5];
int connectTCP(char *addr, int port){
	int sockfd;
	struct sockaddr_in  serv_addr;
	struct hostent *he;
	he = gethostbyname(addr);
	/*
	 *Open a TCP socket(an Internet stream socket).
	 */
	memset((char*)&serv_addr, 0, sizeof(serv_addr));
	//bzero((char* )&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family	= AF_INET;
	serv_addr.sin_addr		= *(struct in_addr *)he -> h_addr;
	serv_addr.sin_port		= htons((short)port);

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("cgi:can't open stream socket");
		exit(1);
	}
	int flags;
	//get mode of sockfd before
	flags = fcntl(sockfd, F_GETFL, 0);
	//set sockfd as non blocking and the mode before
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		if (errno != EINPROGRESS){ 
			perror("cgi:can't connect sock");
			exit(-1);
		}
	}
	return sockfd;
}
int connect_request(){
	int i;
	int connectnum = 0;
	for(i = 0 ; i < 5 ; i++){
		if(strlen(host[i]) > 0 && strlen(file[i]) > 0 && port[i] > 0){
			sockfd[i] = connectTCP(host[i], port[i]);
			connectnum++;
		}else{
			break;
		}
	}
	return connectnum;
}
void send_comm(int i, int *status, fd_set *rs, fd_set *ws){
	char buff[MAXSIZE];
	int wc = 0;
	int writen = 0;
	//get 1 line of commands
	if(fgets(buff, MAXSIZE - 3, fp[i]) == NULL){
		//finish reading file
		return;
	}
	//change "\n" to "\r\n"
	if(buff[strlen(buff)] == '\n')
		buff[strlen(buff)] = '\0';
	strcat(buff, "\r\n");
	//send to server
	while(writen < strlen(buff))
	{
		wc = write(sockfd[i], buff + writen, strlen(buff) - writen);
		if(wc < 0){
			perror("write command error");
			exit(1);
		}
		writen += wc;
	}
	// write finished
	FD_CLR(sockfd[i], ws);
	status[i] = F_READING;
	FD_SET(sockfd[i], rs);
}
int recv_response(int i, int *len, int *status, int conn, char *respmsg, int sockfd, fd_set *rs, fd_set *ws){
	len[i] = read(sockfd, respmsg, MAXSIZE - 1);
	if (len[i] <= 0) {
		// read finished
		FD_CLR(sockfd, rs);
		status[i] = F_DONE ;
		conn--;
	}else if(len[i] < MAXSIZE - 1){
		//read done
		FD_CLR(sockfd, rs);
		status[i] = F_WRITING;
		FD_SET(sockfd, ws);
	}
	respmsg[len[i]] = '\0';
	return conn;
}
void read_query(){
	char *q = getenv("QUERY_STRING");
	char query[strlen(q) + 1];
	strcpy(query, q);//to not affect real address of QUERY_STRING
	char *buff;
	buff = strtok(query, "&");
	while(buff != NULL)
	{
		//hn=xxx.xxx.xxx.xxx, n start with 1
		if(buff[0] == 'h'){
			snprintf(host[buff[1] - '0' - 1], sizeof(host[buff[1] - '0' - 1]), "%s", buff + 3);
		}
		//pn=int
		else if(buff[0] == 'p'){
			port[buff[1] - '0' - 1] = atoi(buff + 3);
		}
		//fn=name
		else if(buff[0] == 'f'){
			snprintf(file[buff[1] - '0' - 1], sizeof(file[buff[1] - '0' - 1]), "%s", buff + 3);
		}
		buff = strtok(NULL, "&");
	}
}
void print_result(char *respmsg, int len){
	int wc = 0;
	int writen = 0;
	while(writen < len)
	{
		wc = write(1, respmsg + writen, len - writen);
		writen += wc;
		if(wc < 0){
			perror("write error");
			exit(1);
		}
	}
}
void set_fd_set_status(fd_set *rfds, fd_set *wfds, fd_set *rs, fd_set *ws, int *status, int conn){
	//clear
	FD_ZERO(rfds);
	FD_ZERO(wfds);
	FD_ZERO(rs);
	FD_ZERO(ws);
	int i;
	for(i = 0 ; i < conn ; i++){
		FD_SET(sockfd[i], rs);
		FD_SET(sockfd[i], ws);
		status[i] = F_CONNECTING;
		fp[i] = fopen(file[i], "r");
		if(fp[i] == NULL){
			perror("fopen error");
			exit(1);
		}
	}
	*rfds = *rs; *wfds = *ws;
}
void fail_done_succeed_write(int i, int *status, fd_set *ws){
	int error = 0, errlen = sizeof(int);
	if (getsockopt(sockfd[i], SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&errlen) < 0 || error != 0) {
		// non-blocking connect failed
		perror("non blocking connect fail");
		exit(1);
	}
	//the first thing to do if succeeded is read welcome
	status[i] = F_READING;
	FD_CLR(sockfd[i], ws);
}
int main()
{
	//clear
	memset(host, 0, sizeof(host)); memset(port, 0, sizeof(port)); memset(file, 0, sizeof(file));
	read_query();
	int conn;//connect number
	conn = connect_request();
	fd_set rfds; /* readable file descriptors*/
	fd_set wfds; /* writable file descriptors*/
	fd_set rs; /* active file descriptors*/
	fd_set ws; /* active file descriptors*/
	int nfds = FD_SETSIZE;
	int len[5];//has writen or has read
	int status[5];
	set_fd_set_status(&rfds, &wfds, &rs, &ws, status, conn);
	char respmsg[MAXSIZE];
	while(conn > 0)
	{
		memcpy(&rfds, &rs, sizeof(rfds));
		memcpy(&wfds, &ws, sizeof(wfds));
		if (select(nfds, &rfds, &wfds, (fd_set*)0, (struct timeval*)0) < 0 ){
			perror("select error");
			exit(1);
		}
		int i;
		for(i = 0 ; i < conn ; i++){
			//check connect succeeded or failed
			if (status[i] == F_CONNECTING && (FD_ISSET(sockfd[i], &rfds) || FD_ISSET(sockfd[i], &wfds))){
				fail_done_succeed_write(i, status, &ws);
			}
			//write
			else if (status[i] == F_WRITING && FD_ISSET(sockfd[i], &wfds)){
				send_comm(i, status, &rs, &ws);
			}
			//read
			else if (status[i] == F_READING && FD_ISSET(sockfd[i], &rfds) ) {
				conn = recv_response(i, len, status, conn, respmsg, sockfd[i], &rs, &ws);
				//print all
				print_result(respmsg, len[i]);
			}
		}
	}
}
