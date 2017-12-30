//connect->read file->write to client->read response->read file->...
#define F_CONNECTING 0
#define F_WRITING 1
#define F_READING 2
#define F_DONE 4
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

char host[5][MAXSIZE];
int  port[5];
char file[5][MAXSIZE];
int  sockfd[5];
FILE *fp[5];

char proxy_h[5][MAXSIZE];
int  proxy_p[5];
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
		if(strlen(host[i]) > 0 && strlen(file[i]) > 0 && port[i] > 0 && strlen(proxy_h[i]) > 0 && proxy_p[i] > 0){
			sockfd[i] = connectTCP(proxy_h[i], proxy_p[i]);
			connectnum++;
		}else{
			break;
		}
	}
	return connectnum;
}
unsigned int change_ip(char buff[]){
	//buff may be domain name
	char buffer[16];
	struct hostent *he;
	he = gethostbyname(buff);
	snprintf(buffer, sizeof(buffer), "%s", inet_ntoa(*(struct in_addr *)he -> h_addr));
	//printf("query to %s\n", buffer);

	char ip[4][4];
	memset(ip, 0, sizeof(ip));
	int i, j = 0;
	int len;
	unsigned int t_IP = 0;
	for(i = 0 ; i < strlen(buffer) && j < 4; i++){
		//printf("ip[%d]=%s", j,ip[j]);
		//printf("i=%d, len=%d\n",i,(int)strlen(buffer));
		if(buffer[i] == '.'){
			j++;
			continue;
		}
		if(strlen(ip[j]) >= 3){
			perror("ip addr not in format");
			exit(1);
		}
		//like strcat
		len = strlen(ip[j]);
		ip[j][len] = buffer[i];
		ip[j][len + 1] = 0;
	}
		
	//printf("ip=%s.%s.%s.%s\n", ip[0], ip[1], ip[2],ip[3]);
	//fflush(stdout);
	for(i = 0 ; i < 4 ; i++){
		//bitwise or
		if(strncmp(ip[i], "*", 1) == 0){
			t_IP |= ((unsigned int)0xFF) << (8 * (3 - i));
		}else{
			t_IP |= ((unsigned int)atoi(ip[i])) << (8 * (3 - i));
		}
	}
	//printf("test\n");
	return t_IP;
}
int tell_sock4_reply(char msg, int idx, unsigned int ip, unsigned int port){
	if(idx >= 8){
		return 0;
	}
	//int i;
	//for (i = 0 ; i < 8 ; i++){
	//	printf("%u\n", (unsigned int)msg[i]);
	//}
	unsigned char c = msg;
	switch (idx){
		case 0:
			if(c != 0){
				return 0;
			}
			break;
		case 1:
			if(c != 90){
				return 0;
			}
			break;
		case 2:
			if(c != port / 256){
				return 0;
			}
			break;
		case 3:
			if(c != port % 256){
				return 0;
			}
			break;
		case 4:
			if(c != (ip >> 24)){
				return 0;
			}
			break;
		case 5:
			if(c != ((ip >> 16) & 0xFF)){
				return 0;
			}
			break;
		case 6:
			if(c != ((ip >> 8) & 0xFF)){
				return 0;
			}
			break;
		case 7:
			if(c != ((ip     ) & 0xFF)){
				return 0;
			}
			break;
	}
	//it is indeed a sock4_reply
	return 1;
}
void print_as_script(int idx, char *respmsg, int len, int strong){
	int i;
	int newline = 0;
	printf("<script>document.all['m%d'].innerHTML += \"", idx);
	if(strong == 1){
		printf("<b>");
	}
	for(i = 0 ; i < len ; i++){
		if(i < 8){
			//not print sock4_reply
			if(tell_sock4_reply(respmsg[i], i, change_ip(host[idx]), port[idx]) == 1){
				continue;
			}
		}
		if(respmsg[i] == '\n' || respmsg[i] == '\r'){
			newline++;
			continue;
		}
		if(newline > 0){
			printf("<br>");
			newline = 0;
		}
		switch (respmsg[i])
		{
			case '<':
				printf("&lt");
				break;
			case '>':
				printf("&gt");
				break;
			case ' ':
				printf("&nbsp");
				break;
			case '&':
				printf("&amp");
				break;
			case '\"':
				printf("&quot");
				break;
			//case '\'':
			//	//ie not support
			//	printf("&apos");
			//	break;
			case '\t':
				printf("&emsp");
				break;
			default:
				printf("%c", respmsg[i]);
				break;
		}
	}
	if(strong == 1){
		printf("</b>");
	}
	if(newline > 0){
		printf("<br>");
	}
	printf("\";</script>");
	fflush(stdout);
}
int send_comm(int idx, int *status, int conn, fd_set *rs, fd_set *ws){
	char buff[MAXSIZE];
	int wc = 0;
	int writen = 0;
	//get 1 line of commands
	if(fgets(buff, MAXSIZE - 3, fp[idx]) == NULL){
		//finish reading file
		status[idx] = F_DONE;
		conn--;
		fclose(fp[idx]);
		return conn;
	}
	//change "\n" to "\r\n"
	if(buff[strlen(buff) - 1] == '\n')
		buff[strlen(buff) - 1] = '\0';
	strcat(buff, "\r\n");
	//send to server
	//if sockfd[idx] == 0, has been closed by server
	while(sockfd[idx] != 0 && writen < strlen(buff))
	{
		wc = write(sockfd[idx], buff + writen, strlen(buff) - writen);
		if(wc < 0){
			perror("write command error");
			exit(1);
		}
		writen += wc;
	}
	// write finished
	if(sockfd[idx] != 0){
		FD_CLR(sockfd[idx], ws);
		status[idx] = F_READING;
		FD_SET(sockfd[idx], rs);
	}
	print_as_script(idx, buff, strlen(buff), 1);
	return conn;
}
void recv_response(int i, int *len, int *status, char *respmsg, int sock, fd_set *rs, fd_set *ws, int conn){
	len[i] = read(sockfd[i], respmsg, MAXSIZE - 1);
	if (len[i] <= 0) {
		// read finished
		FD_CLR(sockfd[i], rs);
		FD_CLR(sockfd[i], ws);
		close(sockfd[i]);
		sockfd[i] = 0;
		status[i] = F_DONE ;
		conn--;
		return conn;
	}
	}
	else{
		//respmsg[len[i]] = '\0';
		int j;
		for(j = 0 ; j < len[i] ; j++){
			if(respmsg[j] == '%'){
				//read done
				FD_CLR(sockfd[i], rs);
				status[i] = F_WRITING;
				FD_SET(sockfd[i], ws);
				return conn;
			}
		}
	}
}
void read_query(){
	char *q = getenv("QUERY_STRING");
	char query[strlen(q) + 1];
	strcpy(query, q);//to not affect real address of QUERY_STRING
	char *buff;
	buff = strtok(query, "&");
	while(buff != NULL)
	{	
		if(strlen(buff) < 4){
			buff = strtok(NULL, "&");
			continue;
		}
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
		}else if(buff[0] == 's' && strlen(buff) >= 5){
			//shn=xxx.xxx.xxx.xxx
			if(buff[1] == 'h'){
				snprintf(proxy_h[buff[2] - '0' - 1], sizeof(proxy_h[buff[2] - '0' - 1]), "%s", buff + 4);
			}
			//spn=int
			else if(buff[1] == 'p'){
				proxy_p[buff[2] - '0' - 1] = atoi(buff + 4);
			}
		}
		buff = strtok(NULL, "&");
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
void sock4_query(int sockfd, unsigned int ip, unsigned int port){
	unsigned char package[8];
	package[0] = 0;
	package[1] = 1;//1:connect, 2:bind
	package[2] = port / 256;
	package[3] = port % 256;
	package[4] = (ip >> 24)        ;
	package[5] = (ip >> 16) & 0xFF ;
	package[6] = (ip >> 8 ) & 0xFF ;
	package[7] = (ip      ) & 0xFF ;
	int wc = 0;
	while(wc < 8)
	{
		wc = write(sockfd, package, 8);
		if(wc < 0){
			perror("write error");
			exit(1);
		}
	}
}
void fail_done_succeed_write(int i, int *status, fd_set *ws){
	int error = 0, errlen = sizeof(int);
	if (getsockopt(sockfd[i], SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&errlen) < 0 || error != 0) {
		// non-blocking connect failed
		perror("non blocking connect fail");
		exit(1);
	}
	sock4_query(sockfd[i], change_ip(host[i]), (unsigned int)port[i]);
	//the first thing to do if succeeded is read welcome
	status[i] = F_READING;
	FD_CLR(sockfd[i], ws);
	//int oldfl = fcntl(sockfd[i], F_GETFL, 0);
	//fcntl(sockfd[i], F_SETFL, oldfl & ~O_NONBLOCK);
}
void print_first_html(int conn){
	int i;
	printf("<html>");
	printf("<head>");
	printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />");
	printf("<title>Network Programming Homework 3</title>");
	printf("</head>");
	printf("<body bgcolor=#336699>");
	printf("<font face=\"Courier New\" size=2 color=#FFFF99>");
	printf("<table width=\"800\" border=\"1\">");
	printf("<tr>");
	for(i = 0 ; i < conn ; i++){
		printf("<td>%s</td>", host[i]);
	}
	printf("<tr>");
	for(i = 0 ; i < conn ; i++){
		printf("<td valign=\"top\" id=\"m%d\"></td>", i);
	}
	printf("</table>");
	printf("</font>");
	printf("</body>");
	printf("</html>");
	fflush(stdout);
}
int main()
{
	//clear
	memset(host, 0, sizeof(host)); memset(port, 0, sizeof(port)); memset(file, 0, sizeof(file));
	memset(proxy_h, 0, sizeof(proxy_h)); memset(proxy_p, 0, sizeof(proxy_p));
	read_query();
	int conn = 0;//connect number
	conn = connect_request();
	print_first_html(conn);
	fd_set rfds; /* readable file descriptors*/
	fd_set wfds; /* writable file descriptors*/
	fd_set rs; /* active file descriptors*/
	fd_set ws; /* active file descriptors*/
	int nfds = FD_SETSIZE;
	int len[5];//has writen or has read
	int status[5];
	set_fd_set_status(&rfds, &wfds, &rs, &ws, status, conn);
	char respmsg[MAXSIZE];
	int bconn = conn;//connect number at beginning
	while(conn > 0)
	{
		memcpy(&rfds, &rs, sizeof(rfds));
		memcpy(&wfds, &ws, sizeof(wfds));
		if (select(nfds, &rfds, &wfds, (fd_set*)0, (struct timeval*)0) < 0 ){
			perror("select error");
			exit(1);
		}
		int i;
		for(i = 0 ; i < bconn ; i++){
			//check connect succeeded or failed
			if (status[i] == F_CONNECTING && (FD_ISSET(sockfd[i], &rfds) || FD_ISSET(sockfd[i], &wfds))){
				fail_done_succeed_write(i, status, &ws);
			}
			//write
			else if (status[i] == F_WRITING && FD_ISSET(sockfd[i], &wfds)){
				conn = send_comm(i, status, conn, &rs, &ws);
			}
			//read
			else if (status[i] == F_READING && FD_ISSET(sockfd[i], &rfds) ) {
				conn = recv_response(i, len, status, respmsg, sockfd[i], &rs, &ws, conn);
				//print all
				print_as_script(i, respmsg, len[i], 0);
			}
		}
	}
}
