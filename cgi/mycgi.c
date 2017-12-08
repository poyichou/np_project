#define F_CONNECTING 0
#define F_READING 1
#define F_WRITING 2
#define F_DONE 3
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
int sockfd[5];
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
int write_request(int sockfd, int idx, int *writen){
	char req[strlen("GET / HTTP/1.1\r\nHost: \r\n\r\r") + strlen(host[idx]) + strlen(file[idx]) + 1];
	snprintf(req, sizeof(req), "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\r", file[idx], host[idx]);
	int n = *writen;
	n = write(sockfd, req + n, strlen(req) - n);
	return n;
}
void send_request(int i, int *n, int *NeedWrite, int *status, fd_set *rs, fd_set *ws){
	n[i] = write_request(sockfd[i], i, &n[i]);
	NeedWrite[i] -= n[i];
	//if not, write next time
	if (n[i] <= 0 || NeedWrite[i] <= 0) {
		// write finished
		FD_CLR(sockfd[i], ws);
		status[i] = F_READING;
		FD_SET(sockfd[i], rs);
	}
}
int read_response(int sockfd, char *respmsg){
	int rc = 0;
	rc = read(sockfd, respmsg, MAXSIZE - 1);
	return rc;
}
int recv_response(int i, int *n, int *status, int conn, char *respmsg, int sockfd, fd_set *rs){
	n[i] = read_response(sockfd, respmsg);
	if (n[i] <= 0) {
		// read finished
		FD_CLR(sockfd, rs);
		status[i] = F_DONE ;
		conn--;
	}
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
		wc = write(1, respmsg + writen, strlen(respmsg) - writen);
		writen += wc;
		if(wc < 0){
			perror("write error");
			exit(1);
		}
	}
}
void set_fd_set_status(fd_set *rfds, fd_set *wfds, fd_set *rs, fd_set *ws, int *n, int *status, int *NeedWrite, int conn){
	//clear
	FD_ZERO(rfds);
	FD_ZERO(wfds);
	FD_ZERO(rs);
	FD_ZERO(ws);
	int i;
	for(i = 0 ; i < conn ; i++){
		FD_SET(sockfd[i], rs);
		FD_SET(sockfd[i], ws);
		n[i] = 0;
		status[i] = F_CONNECTING;
		NeedWrite[i] = strlen("GET / HTTP/1.1\r\nHost: \r\n\r\r") + strlen(host[i]) + strlen(file[i]) + 1;
	}
	*rfds = *rs; *wfds = *ws;
}
void fail_done_succeed_write(int i, int *status, fd_set *rs){
	int error = 0, errlen = sizeof(int);
	if (getsockopt(sockfd[i], SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&errlen) < 0 || error != 0) {
		// non-blocking connect failed
		perror("non blocking connect fail");
		exit(1);
	}
	//the first thing to do if succeeded is write
	status[i] = F_WRITING;
	FD_CLR(sockfd[i], rs);
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
	int n[5];//has writen or has read
	int status[5], NeedWrite[5];
	set_fd_set_status(&rfds, &wfds, &rs, &ws, n, status, NeedWrite, conn);
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
				fail_done_succeed_write(i, status, &rs);
			}
			//write first
			else if (status[i] == F_WRITING && FD_ISSET(sockfd[i], &wfds)){
				send_request(i, n, NeedWrite, status, &rs, &ws);
			}
			//then read
			else if (status[i] == F_READING && FD_ISSET(sockfd[i], &rfds) ) {
				conn = recv_response(i, n, status, conn, respmsg, sockfd[i], &rs);
				//print all
				print_result(respmsg, n[i]);
			}
		}
	}
}
