#define SERV_TCP_PORT 7000
#define REQSIZE 20
#define MAXSIZE 512
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
int passiveTCP(int port, int qlen){
	int sockfd;
	struct sockaddr_in  serv_addr;
	/*
	 *Open a TCP socket(an Internet stream socket).
	 */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("server:can't open stream socket");
		exit(1);
	}
	memset((char*)&serv_addr, 0, sizeof(serv_addr));
	//bzero((char* )&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	serv_addr.sin_port			= htons(SERV_TCP_PORT);

	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("server:can't bind local address");
		exit(1);
	}
	listen(sockfd, qlen);
	return sockfd;
}
//blocking
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
	serv_addr.sin_addr		= *(struct in_addr *)(he -> h_addr);
	serv_addr.sin_port		= htons((short)port);

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("can't open stream socket");
		exit(1);
	}
	//int flags;
	//get mode of sockfd before
	//flags = fcntl(sockfd, F_GETFL, 0);
	//set sockfd as non blocking and the mode before
	//fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		if (errno != EINPROGRESS){ 
			perror("can't connect sock");
			exit(-1);
		}
	}
	return sockfd;
}
int analyze_conf(unsigned int DST_IP, unsigned int mode){
	//permit [cb] xxx.xxx.xxx.xxx
	//(connect mode or bind mode)
	//accept * in xxx
	char buffer[MAXSIZE];
	char ip[4][4];
	memset(ip, 0, sizeof(ip));
	FILE *fp = fopen("socks.conf", "r");
	if(fp == NULL){
		perror("can't read conf file");
		exit(1);
	}
	int i, j = 0;
	unsigned int t_IP;
	int len;
	char m = (mode == 1) ? 'c' : 'b';//1:connect , 2:bind
	while(fgets(buffer, MAXSIZE, fp) != NULL)
	{
		t_IP = 0;
		if(buffer[7] != m)
			continue;
		for(i = 9 ; i < strlen(buffer) && j < 4; i++){
			if(strlen(ip[j]) >= 3){
				perror("conf file not follow format");
				exit(1);
			}
			if(buffer[i] == '.')
				j++;
			if(buffer[i] == '#')
				break;
			//like strcat
			len = strlen(ip[j]);
			ip[j][len] = buffer[i];
			ip[j][len + 1] = 0;
		}
		for(i = 0 ; i < 4 ; i++){
			//bitwise or
			if(strncmp(ip[i], "*", 1) == 0){
				t_IP |= ((unsigned int)0xFF) << (8 * (3 - i));
			}else{
				t_IP |= ((unsigned int)atoi(ip[i])) << (8 * (3 - i));
			}
		}
		//bitwise and
		if((DST_IP & t_IP) == DST_IP)
			return 1;
	}
	return 0;
}
void sock4_reply(int sockfd, unsigned int ip, unsigned int port, unsigned cd, int code){
	unsigned int package[8];
	package[0] = 0;
	package[1] = code;//90:granted, 91:reject or fail
	package[2] = port / 256;
	package[3] = port % 256;
	package[4] = (cd == 1) ? (ip >> 24)        : 0;
	package[5] = (cd == 1) ? (ip >> 16) & 0xFF : 0;
	package[6] = (cd == 1) ? (ip >> 8 ) & 0xFF : 0;
	package[7] = (cd == 1) ? (ip      ) & 0xFF : 0;
	int wc = 0;
	while(wc < 8)
	{
		wc = write(sockfd, package, 8);
	}
}
void process_connect(int msockfd, unsigned int DST_IP, unsigned int DST_PORT){
	sock4_reply(msockfd, DST_IP, DST_PORT, 1, 90);
	char addr[16];
	snprintf(addr, sizeof(addr), "%u.%u.%u.%u", DST_IP >> 24, DST_IP >> 16 & 0xFF, DST_IP >> 8 & 0xFF, DST_IP & 0xFF);
	int port = DST_PORT / 256 * 10 + DST_PORT % 256;
	int ssockfd = connectTCP(addr, port);
	int rc, wc = 0;
	char buff[MAXSIZE];
	while(1)
	{
		rc = read(ssockfd, buff, MAXSIZE);
		if(rc < 0){
			perror("read error");
			exit(1);
		}else if(rc == 0){
			exit(0);
		}else{
			wc = write(msockfd, buff, rc);
			if(wc < 0){
				perror("write error");
				exit(1);
			}
		}
	}
}
int passive_bind_TCP(int qlen){
	int bindfd;
	struct sockaddr_in  bind_addr;
	/*
	 *Open a TCP socket(an Internet stream socket).
	 */
	if((bindfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("server:can't open stream socket");
		exit(1);
	}
	memset((char*)&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sin_family		= AF_INET;
	bind_addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	bind_addr.sin_port			= htons(INADDR_ANY);
	//the only difference!!             ^^^^^^^^^^

	if(bind(bindfd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0){
		perror("server:can't bind local address");
		exit(1);
	}
	listen(bindfd, qlen);
	return bindfd;
}
int send_data(int sfd, int dfd, int count, fd_set *rs){//source fd, destination fd
	int rc = 0, wc = 0;
	char buff[MAXSIZE];
	rc = read(sfd, buff, MAXSIZE);
	if(rc < 0){
		perror("read error");
		exit(1);
	}else if(rc == 0){//finish
		FD_CLR(sfd, rs);
		count--;
	}else{
		wc = write(dfd, buff, rc);
		if(wc < 0){
			perror("write error");
			exit(1);
		}
	}
	return count;
}
void transfer_data(int msockfd, int ftp_fd){
	fd_set rfds; /* readable file descriptors*/
	fd_set rs; /* active file descriptors*/
	int nfds = FD_SETSIZE;
	FD_ZERO(&rfds);
	FD_ZERO(&rs);
	FD_SET(msockfd, &rs);
	FD_SET(ftp_fd,  &rs);
	int count = 2;
	while(count > 0)
	{
		memcpy(&rfds, &rs, sizeof(rfds));
		if (select(nfds, &rfds, (fd_set*)0, (fd_set*)0, (struct timeval*)0) < 0 ){
			perror("select error");
			exit(1);
		}
		if(FD_ISSET(msockfd, &rfds)){
			count = send_data(msockfd, ftp_fd, count, &rs);
		}
		if(FD_ISSET(ftp_fd, &rfds)){
			count = send_data(ftp_fd, msockfd, count, &rs);
		}
	}
}
void process_bind(int msockfd){
	int bindfd = passive_bind_TCP(5);
	struct sockaddr_in bindaddr;
	socklen_t blen = sizeof(bindaddr);
	//get self information
	if(getsockname(bindfd, (struct sockaddr *)&bindaddr, &blen) < 0){
		perror("getsockname error");
		exit(1);
	}
	//reply port, it'll connect itself
	sock4_reply(msockfd, 0, ntohs(bindaddr.sin_port), 2, 90);
	int ftp_fd;
	struct sockaddr_in ftp_addr;
	socklen_t ftplen = sizeof(ftp_addr);
	if((ftp_fd = accept(bindfd, (struct sockaddr *)&ftp_addr, &ftplen)) < 0){
		perror("ftp accept error");
		exit(1);
	}
	//must send again
	sock4_reply(msockfd, 0, ntohs(bindaddr.sin_port), 2, 90);
	transfer_data(msockfd, ftp_fd);
}
int handle_request(int sockfd){
	int rc;
	char request[REQSIZE];
	unsigned char VN;
	unsigned char CD;
	unsigned int DST_PORT;
	unsigned int DST_IP;
	char *USER_ID;
	while(1)
	{
		rc = read(sockfd, request, REQSIZE);
		if(rc < 0){
			perror("read error");
			exit(1);
		}
		VN			= request[0];
		CD			= request[1];//1:connect, 2:bind
		DST_PORT	= request[2] << 8  | request[3];
		DST_IP		= request[4] << 24 | request[5] << 16 | request[6] << 8 | request[7];
		USER_ID 	= request + 8;
		//pass
		if(analyze_conf(DST_IP, CD) == 1){
			if(CD == 1)
				process_connect(sockfd, DST_IP, DST_PORT);
			else
				process_bind(sockfd);
		}else{//fail
			sock4_reply(sockfd, DST_IP, DST_PORT, CD, 91);
			exit(1);
		}
	}
	return 0;
}
int main()
{
	int msockfd;//master sock
	int pid;
	struct sockaddr_in cli_addr;
	socklen_t alen; // client address length
	msockfd = passiveTCP(SERV_TCP_PORT, 5);
	while(1)
	{
		// new connection from client
		int newsockfd;
		alen = sizeof(cli_addr);
		newsockfd = accept(msockfd, (struct sockaddr *)&cli_addr, &alen);
		if(newsockfd < 0){
			if(errno == EINTR)
				continue;
			perror("accept error");
			exit(1);
		}
		if((pid = fork()) < 0){
			perror("fork error");
			exit(1);
		}else if(pid == 0){
			close(msockfd);
			//proccess requests
			handle_request(newsockfd);
			exit(0);
		}
		close(newsockfd);
	}
}
