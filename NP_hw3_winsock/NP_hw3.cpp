#include <windows.h>
#include <list>
#include <string.h>
using namespace std;

#include "resource.h"

#define SERVER_PORT 7799
#define WRITING 0
#define READING 1

#define WM_SOCKET_NOTIFY (WM_USER + 1)
#define WM_CONNECT1_NOTIFY (WM_USER + 2)
#define WM_CONNECT2_NOTIFY (WM_USER + 3)
#define WM_CONNECT3_NOTIFY (WM_USER + 4)
#define WM_CONNECT4_NOTIFY (WM_USER + 5)
#define WM_CONNECT5_NOTIFY (WM_USER + 6)
#define MAXSIZE 512
#define MYWRITE(m) \
/*read command from file*/\
			/*EditPrintf(hwndEdit, TEXT("=== FD_WRITE ===\r\n"));*/\
			if (fp[m] != NULL && fgets(sendbuff, MAXSIZE - 2, fp[m]) == NULL)\
			{\
				fclose(fp[m]);\
				fp[m] = NULL;\
				break;\
			}\
			else if (fp[m] == NULL)\
			{\
				break;\
			}\
		\
			/*EditPrintf(hwndEdit, TEXT("=== fgets fp[%d] ===\r\n"), m);*/\
			if (sendbuff[strlen(sendbuff) - 1] == '\n')\
			{\
				sendbuff[strlen(sendbuff) - 1] = '\0';\
			}\
			strcat(sendbuff, "\r\n");\
			/*send command to server*/\
			/*EditPrintf(hwndEdit, TEXT("=== sendbuff:%s ===\r\n"), sendbuff);*/\
			sc = send(wParam, sendbuff, strlen(sendbuff), 0);\
			if (sc < 0) {\
				/*EditPrintf(hwndEdit, TEXT("=== send command to server failed ===\r\n"));*/\
				closesocket(wParam);\
				WSACleanup();\
				return 1;\
			}\
			print_as_script(m, sendbuff, strlen(sendbuff), 1);\
			send_script(hwndEdit, Socks.back());\
			/*EditPrintf(hwndEdit, TEXT("=== sc:%d ===\r\n"), sc);*/\
			state[m] = READING;
#define ConnectCase(m) \
case WM_CONNECT1_NOTIFY + m:\
	switch (WSAGETSELECTEVENT(lParam))\
	{\
		case FD_CONNECT:\
			/*connection complete (I have no idea how to know successfully or not) */\
			state[m] = READING;\
			/*EditPrintf(hwndEdit, TEXT("=== FD_CONNECT ===\r\n"));*/\
			break;\
		case FD_READ:\
			if(state[m] != READING) break;\
			/*recv from server*/\
			/*EditPrintf(hwndEdit, TEXT("=== FD_READ ===\r\n"));*/\
			rc = recv(wParam, recvbuff, MAXSIZE - 1, 0);\
			if (rc < 0) {\
				/*EditPrintf(hwndEdit, TEXT("=== recv failed ===\r\n"));*/\
				closesocket(wParam);\
				WSACleanup();\
				return 1;\
			}\
			recvbuff[rc] = 0;\
			/*send to browser*/\
			/*EditPrintf(hwndEdit, TEXT("=== recvbuff:%s ===\r\n"), recvbuff);*/\
			print_as_script(m, recvbuff, strlen(recvbuff), 0);\
			send_script(hwndEdit, Socks.back());\
			for (int i = 0; i < strlen(recvbuff); i++) {\
				\
					if (recvbuff[i] == '%' && rc < MAXSIZE - 1) {\
						\
							state[m] = WRITING; \
							MYWRITE(m)\
					}\
			}\
			/*EditPrintf(hwndEdit, TEXT("=== rc=%d ===\r\n"), rc);*/ \
			break;\
		case FD_WRITE:\
			/*read command from file*/\
			\
			break;\
		case FD_CLOSE:\
			/*EditPrintf(hwndEdit, TEXT("=== FD_CLOSE ===\r\n"));*/\
			closesocket((SOCKET)wParam);\
			fclose(fp[m]);\
			fp[m] = NULL;\
			memset(host[m], 0, sizeof(file[m]));\
			memset(file[m], 0, sizeof(file[m]));\
			conn--;\
			break;\
		default:\
			break;\
	};\
break;

int state[5];
char host[5][MAXSIZE];
int  port[5];
char file[5][MAXSIZE];
int conn = 0;
SOCKET connectsock[5];
FILE *fp[5];
char QUERRY_STRING[MAXSIZE];
BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf (HWND, TCHAR *, ...);
void parse_request(char *req, char *cgi_name);
void parse_QUERRY_STRING();
//=================================================================
//	Global Variables
//=================================================================
list<SOCKET> Socks;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	
	return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

int connectTCP(HWND hwnd, HWND hwndEdit, SOCKET &connectsocket,char *addr, int port)
{
	struct sockaddr_in  serv_addr;
	struct hostent *he;
	he = gethostbyname(addr);
	/*
	*Open a TCP socket(an Internet stream socket).
	*/
	memset((char*)&serv_addr, 0, sizeof(serv_addr));
	//bzero((char* )&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr = *(struct in_addr *)he->h_addr;
	serv_addr.sin_port = htons((short)port);

	if ((connectsocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		/*EditPrintf(hwndEdit, TEXT("=== Error: socket error ===\r\n"))*/;
		//exit(-1);
	}
	int err = WSAAsyncSelect(connectsocket, hwnd, WM_CONNECT1_NOTIFY + conn, FD_CONNECT | FD_CLOSE | FD_READ | FD_WRITE);
	/*EditPrintf(hwndEdit, TEXT("=== WSAAsyncSelect: %d ===\r\n"), conn);*/
	if (err == SOCKET_ERROR) {
		/*EditPrintf(hwndEdit, TEXT("=== Error: WSAAsyncSelect error ===\r\n"));*/
		//exit(-1);
	}
	err = connect(connectsocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	/*EditPrintf(hwndEdit, TEXT("=== Connecting: %s:%d ===\r\n"), addr, port);*/
	if (err != SOCKET_ERROR)
	{
		/*EditPrintf(hwndEdit, TEXT("=== Error: connect error ===\r\n"));*/
		//exit(-1);
	}
	conn++;
	
	return 0;
}
void connectallTCP(HWND hwnd, HWND hwndEdit)
{
	conn = 0;
	for (int i = 0; i < 5; i++) {
		if (strlen(host[i]) > 0 && strlen(file[i]) > 0 && port[i] != 0)
		{
			EditPrintf(hwndEdit, TEXT("=== fp[%d] opened ===\r\n"), conn);
			fp[conn] = fopen(file[conn], "r");
			connectTCP(hwnd, hwndEdit, connectsock[conn], host[i], port[i]);
			EditPrintf(hwndEdit, TEXT("=== conn:%d ===\r\n"), conn);
		}
	}
}
void parse_QUERRY_STRING()
{
	char *buff;
	buff = strtok(QUERRY_STRING, "&");
	while (buff != NULL)
	{
		//h1=xxx
		if (strlen(buff) < 4) 
		{
			buff = strtok(NULL, "&");
			continue;
		}
		if (buff[0] == 'h')
		{
			strcpy(host[buff[1] - '0' - 1], buff + 3);
		}
		else if (buff[0] == 'p')
		{
			port[buff[1] - '0' - 1] = atoi(buff + 3);
		}
		else if (buff[0] == 'f')
		{
			strcpy(file[buff[1] - '0' - 1], buff + 3);
		}
		buff = strtok(NULL, "&");
	}
}
void parse_request(char *req, char *cgi_name) 
{
	char *buff;
	int idx = -1;
	int i;
	for (i = 0; i < strlen(req); i++) {
		if (req[i] == '/') {
			idx = i;
		}
		else if (req[i] == 'H' || req[i] == 'h') {
			break;
		}
	}
	if (idx == -1) {
		perror("wrong GET format");
		//exit(1);
	}
	//strcpy(cgi_name, buff + idx + 1);
	for (i = 0; i < MAXSIZE - 1; i++) {
		if (i + idx + 1 >= strlen(req)) {
			break;
		}
		cgi_name[i] = req[i + idx + 1];
		if (cgi_name[i] == ' ' || cgi_name[i] == '?') {
			cgi_name[i] = 0;
			break;
		}
	}
	//get arguments
	buff = strtok(req, "?");
	buff = strtok(NULL, "?");
	if (buff == NULL) {
		return;
	}
	else {
		//GET [domain]?[query] HTTP 1.1
		//              ^~
		for (i = 0; i < strlen(buff); i++) {
			if (buff[i] == ' ') {
				buff[i] = 0;
				break;
			}
		}
		strcpy(QUERRY_STRING, buff);
	}
}
void print_first_html(int conn, WPARAM wParam) {
	int i;
	send(wParam, "<html>", strlen("<html>"), 0);
	send(wParam, "<head>", strlen("<head>"), 0);
	send(wParam, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />", strlen("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />"), 0);
	send(wParam, "<title>Network Programming Homework 3</title>", strlen("<title>Network Programming Homework 3</title>"), 0);
	send(wParam, "</head>", strlen("</head>"), 0);
	send(wParam, "<body bgcolor=#336699>", strlen("<body bgcolor=#336699>"), 0);
	send(wParam, "<font face=\"Courier New\" size=2 color=#FFFF99>", strlen("<font face=\"Courier New\" size=2 color=#FFFF99>"), 0);
	send(wParam, "<table width=\"800\" border=\"1\">", strlen("<table width=\"800\" border=\"1\">"), 0);
	send(wParam, "<tr>", strlen("<tr>"), 0);
	char temp[MAXSIZE];
	for (i = 0; i < conn; i++) {
		snprintf(temp, sizeof(temp), "<td>%s</td>", host[i]);
		send(wParam, temp, strlen(temp), 0);
	}
	send(wParam, "<tr>", strlen("<tr>"), 0);
	for (i = 0; i < conn; i++) {
		snprintf(temp, sizeof(temp), "<td valign=\"top\" id=\"m%d\"></td>", i);
		send(wParam, temp, strlen(temp), 0);
	}
	send(wParam, "</table>", strlen("</table>"), 0);
	send(wParam, "</font>", strlen("</font>"), 0);
	send(wParam, "</body>", strlen("</body>"), 0);
	send(wParam, "</html>", strlen("</html>"), 0);
}
void print_as_script(int idx, char *respmsg, int len, int strong) {
	int i;
	int newline = 0;
	char temp[MAXSIZE];
	FILE *tfp;
	tfp = fopen("tempfile.txt", "a");
	
	fprintf(tfp, "<script>document.all['m%d'].innerHTML += \"", idx);
	//snprintf(temp, sizeof(temp), "<script>document.all['m%d'].innerHTML += \"", idx);
	//send(wParam, temp, strlen(temp), 0);
	//temp[0] = 0;
	memset(temp, 0, sizeof(temp));
	if (strong == 1) {
		//send(wParam, "<b>", strlen("<b>"), 0);
		fprintf(tfp, "<b>");
	}
	for (i = 0; i < len; i++) {
		if (strlen(temp) < MAXSIZE - 10)
		{
			//send(wParam, temp, strlen(temp), 0);
			fprintf(tfp, temp);
			//temp[0] = 0;
			memset(temp, 0, sizeof(temp));
		}
		if (respmsg[i] == '\n' || respmsg[i] == '\r') {
			newline++;
			continue;
		}
		if (newline > 0) {
			strcat(temp, "<br>");
			newline = 0;
		}
		switch (respmsg[i])
		{
		case '<':
			strcat(temp, "&lt");
			break;
		case '>':
			strcat(temp, "&gt");
			break;
		case ' ':
			strcat(temp, "&nbsp");
			break;
		case '&':
			strcat(temp, "&amp");
			break;
		case '\"':
			strcat(temp, "&quot");
			break;
			//case '\'':
			//	//ie not support
			//	printf("&apos");
			//	break;
		case '\t':
			strcat(temp, "&emsp");
			break;
		case '%':
			strcat(temp, "%%");
			break;
		default:
			char ch[2];
			ch[0] = respmsg[i];
			ch[1] = 0;
			strcat(temp, ch);
			break;
		}
	}
	if (strlen(temp) > 0)
	{
		//send(wParam, temp, strlen(temp), 0);
		fprintf(tfp, temp);
		//temp[0] = 0;
		memset(temp, 0, sizeof(temp));
	}
	if (strong == 1) {
		strcat(temp, "</b>");
	}
	if (newline > 0) {
		strcat(temp, "<br>");
	}
	strcat(temp, "\";</script>");
	fprintf(tfp, temp);
	//send(wParam, temp, strlen(temp), 0);
	//temp[0] = 0;
	memset(temp, 0, sizeof(temp));
	fclose(tfp);
}
void send_script(HWND hwndEdit, WPARAM wParam)
{
	char temp[MAXSIZE];
	FILE *tfp = fopen("tempfile.txt", "r");
	if (tfp == NULL)
	{
		EditPrintf(hwndEdit, TEXT("=== file not exists!!\r\n"));
		return;
	}
	while (fgets(temp, MAXSIZE, tfp) != NULL)
	{
		//EditPrintf(hwndEdit, TEXT("=== there is '%':%s\r\n"), temp);
		send(wParam, temp, strlen(temp), 0);
	}
	fclose(tfp);
	if (remove("tempfile.txt") != 0)
	{
		EditPrintf(hwndEdit, TEXT("=== file not exists!!\r\n"));
	}
}
BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	WSADATA wsaData;

	static HWND hwndEdit;
	static SOCKET msock, ssock;
	static struct sockaddr_in sa;

	int err;
	char recvbuff[MAXSIZE];
	int rc;
	char sendbuff[MAXSIZE];
	int sc;
	char cgi_name[MAXSIZE];

	switch(Message) 
	{
		//create a dialog box
		case WM_INITDIALOG:
			//hwndEdit == window handle of the specified control
			hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case ID_LISTEN:
					memset(host, 0, sizeof(host));
					memset(file, 0, sizeof(file));
					// Initialize Winsock
					WSAStartup(MAKEWORD(2, 0), &wsaData);

					//create master socket
					msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

					if( msock == INVALID_SOCKET ) {
						EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
						WSACleanup();
						return TRUE;
					}

					err = WSAAsyncSelect(msock, hwnd, WM_SOCKET_NOTIFY, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);

					if ( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
						closesocket(msock);
						WSACleanup();
						return TRUE;
					}

					//fill the address info about server
					sa.sin_family		= AF_INET;
					sa.sin_port			= htons(SERVER_PORT);
					sa.sin_addr.s_addr	= INADDR_ANY;

					//bind socket
					err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

					if( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
						WSACleanup();
						return FALSE;
					}

					err = listen(msock, 2);
		
					if( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
						WSACleanup();
						return FALSE;
					}
					else {
						EditPrintf(hwndEdit, TEXT("=== Server START ===\r\n"));
					}

					break;
				case ID_EXIT:
					EndDialog(hwnd, 0);
					break;
			};
			break;

		case WM_CLOSE:
			EndDialog(hwnd, 0);
			break;

		case WM_SOCKET_NOTIFY:
			switch( WSAGETSELECTEVENT(lParam) )
			{
				case FD_ACCEPT:
					//EditPrintf(hwndEdit, TEXT("=== FD_ACCEPT ===\r\n"));
					ssock = accept(msock, NULL, NULL);
					Socks.push_back(ssock);
					//EditPrintf(hwndEdit, TEXT("=== Accept one new client(%d), List size:%d ===\r\n"), ssock, Socks.size());
					break;
				case FD_READ:
				//Write your code for read event here.
					//EditPrintf(hwndEdit, TEXT("=== WM_SOCKET_NOTIFY FD_READ ===\r\n"));
					rc = recv(wParam, recvbuff, MAXSIZE - 1, 0);
					if (rc == SOCKET_ERROR) {
						printf("recv failed\n");
						closesocket(wParam);
						WSACleanup();
						return 1;
					}
					recvbuff[rc] = 0;
					parse_request(recvbuff, cgi_name);
					//EditPrintf(hwndEdit, TEXT("=== read:%s ===\r\n"), recvbuff);
					for (int i = 0; i < strlen(recvbuff) - 3; i++)
					{
						//exec cgi
						if (recvbuff[i] == '.' && recvbuff[i + 1] == 'c' && recvbuff[i + 2] == 'g' && recvbuff[i + 3] == 'i')
						{
							parse_QUERRY_STRING();
							connectallTCP(hwnd, hwndEdit);
							send(wParam, "HTTP/1.1 200 OK\r\n", strlen("HTTP/1.1 200 OK\r\n"), 0);
							send(wParam, "Content-Type: text/html\r\n\r\n", strlen("Content-Type: text/html\r\n\r\n"), 0);
							print_first_html(conn, wParam);
							break;
						}
					}
					for (int i = 0; i < strlen(recvbuff) - 3; i++)
					{
						//dump html
						if (recvbuff[i] == '.' && recvbuff[i + 1] == 'h' && recvbuff[i + 2] == 't' && recvbuff[i + 3] == 'm')
						{
							send(wParam, "HTTP/1.1 200 OK\r\n", strlen("HTTP/1.1 200 OK\r\n"), 0);
							send(wParam, "Content-Type: text/html\r\n\r\n", strlen("Content-Type: text/html\r\n\r\n"), 0);
							char temp[MAXSIZE];
							FILE *tfp = fopen(cgi_name, "r");
							while (fgets(temp, MAXSIZE, tfp) != NULL)
							{
								send(wParam, temp, strlen(temp), 0);
							}
							fclose(tfp);
							break;
						}
					}
					break;
				case FD_WRITE:
					//EditPrintf(hwndEdit, TEXT("=== WM_SOCKET_NOTIFY FD_WRITE ===\r\n"));
				//Write your code for write event here
					//sc = send(wParam, sendbuff, );
					send_script(hwndEdit, wParam);
					break;
				case FD_CLOSE:
					//EditPrintf(hwndEdit, TEXT("=== WM_SOCKET_NOTIFY FD_CLOSE ===\r\n"));
					closesocket((SOCKET)wParam);
					break;
			};
			break;
		ConnectCase(0)
		ConnectCase(1)
		ConnectCase(2)
		ConnectCase(3)
		ConnectCase(4)
		default:
			return FALSE;


	};

	return TRUE;
}

int EditPrintf (HWND hwndEdit, TCHAR * szFormat, ...)
{
     TCHAR   szBuffer [1024] ;
     va_list pArgList ;

     va_start (pArgList, szFormat) ;
     wvsprintf (szBuffer, szFormat, pArgList) ;
     va_end (pArgList) ;

     SendMessage (hwndEdit, EM_SETSEL, (WPARAM) -1, (LPARAM) -1) ;
     SendMessage (hwndEdit, EM_REPLACESEL, FALSE, (LPARAM) szBuffer) ;
     SendMessage (hwndEdit, EM_SCROLLCARET, 0, 0) ;
	 return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0); 
}