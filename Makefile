all:Remote_Access_Server.c  Remote_Access_Server.c
	gcc Remote_Access_Server.c -o Remote_Access_Server -Wall
	gcc Remote_Access_Client.c -o Remote_Access_Client -Wall
server:Remote_Access_Server.c
	gcc Remote_Access_Server.c -o Remote_Access_Server -Wall
client:Remote_Access_Client.c
	gcc Remote_Access_Client.c -o Remote_Access_Client -Wall
