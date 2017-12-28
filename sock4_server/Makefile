all: sock4_server hw4.cgi
sock4_server: sock4_server.c
	gcc sock4_server.c -o sock4_server -g -Wall -Wno-unused-but-set-variable
hw4.cgi: hw4_cgi.c
	gcc hw4_cgi.c -o hw4.cgi -g -Wall
.PHONY:clean
clean:
	rm sock4_server
	rm hw4.cgi
