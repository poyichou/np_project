# NP Project4  
__This is a project based on NP Project3__  
__In this project, I write two new program:__  
*	__sock4_server.c__  
*	__hw4_cgi.c__  
__Which are placed in folder sock4_server__  
  
## socks.conf:  
configureation file for sock4_server.c.  
It indicate which IP the sock4_server.c could accept. IPs not indicated in it would not be accept.  
The format seems like following:  
`permit [cb] xxx.xxx.xxx.xxx`  
where c means connect mode, b means bind mode, and xxx could be "\*", meaning all. For example, 140.113.\*.\* means allowing all IPs starting with 104.113.  
## sock4_sever.c:  
It's a proxy server with protocol socks 4. It support *connect mode* and *bind mode*, with time limit set to 2 minutes for each connecton. When one of server and client close socket, close all connection.  
It requires a file named "socks.conf" in the same folder to function as firewall well.  
It scans socks.conf for each connection, so modification of socks.conf would take into effect without restarting sock4_server.  
### form_get2.htm:  
It works like form_get.htm in Project3. While it add two columns, shN, spN.  
Only rows which does not miss any element(ie.hx, px, fx, shN, spN must not miss) would be recard.  
*	shN:the proxy server for hN to connect.(sock4_server).  
*	spN:the port proxy server for hN to connect.  
  
## hw4_cgi.c  
  It is a cgi based on *mycgi.c* in Project3. There are some difference:  
*	it parse http request such that "shN=xxxxx" and "spN=xxx", where N is a number between 1 and 5, reprsenting proxy server host and port for N'th connection respectively.  
*	after analyzing http request, it send sock 4 request to proxy server assigned in http request instead of connect to host directly.  
