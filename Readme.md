# NP Project3  
__This is a project based on NP Project2__  
__In this project, I write two new program:__  
*	__http_sever.c__  
*	__mycgi.c__  
__And a NP_hw3_winsock Visual Studio Project__  
  
## http_server.c  
A http server which can run cgi and dump html file.(only handle GET)  
There is a html file named "form_get.htm" for covenience to send request to cgi.  
### form_get.htm:  
Only rows which does not miss any element(ie.hx, px, fx must not miss) would be recard.  
*	hx:the server to connect.(server in NP Project2).  
*	px:the port of server to connect.  
*	fx:the file containing commands to pass to server.  
  
## mycgi.c  
A cgi that would connect to server in NP Project2, send commands, and print out the result according to request from http_server.c.  
At most 5 host allowed.  
  
## NP_hw3_winsock:  
It is like a http server which can dump ".htm" file and if the file request ends with ".cgi", it would become a "mycgi.c", connect to servers(in NP Project2) and return the result to browser.  
