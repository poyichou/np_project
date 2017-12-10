all: hw3.cgi http_server
hw3.cgi: mycgi.c
	gcc mycgi.c -o hw3.cgi -Wall -g
http_server:http_server.c
	gcc http_server.c -o http_server -Wall -g
.PHONY: clean
clean:
	rm hw3.cgi
	rm http_server
