all:Remote_Access_Server.c  Remote_Access_Server.h Single_Process_Concurrent_Server_main.c Single_Process_Concurrent_Server_main.h
	gcc Remote_Access_Server.c Single_Process_Concurrent_Server_main.c -o Single_Process_Concurrent_Server -Wall
