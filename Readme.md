# NP Project2  
__This is a project based on NP Project1__  
__In this project, the function of Remote-Access-Server is extended in two version__  
*	__concurrent connection-oriented paradigm with shared memory__  
*	__single-process concurrent paradigm__  
  
__In this project, server can handle mutiple client__  
__Beside functions in NP Project1, it have other functions__  
*	__who:__  
	Show all user online, include clients' id, name, ip/port, and indicate himself  
*	__name:__  
	Rename it self, every client is named (no name) after connection  
	The name client type cannot be the same as other name of user after succeeded, broadcast to every client online include himself  
*	__tell:__  
	With user id provide after tell, client can tell a specific client online  
*	__yell:__  
	Broadcast message to all clients including himself  
*	__command >n:__  
	n is user id, it pipe result of command to the client online having the user id n after succeed, broadcast to all client including himself  
*	__command <n:__  
	n is user id, it receive message piped from the client having the user id n as input of command after succeed, broadcast to all client including himself  
	"command <n >N", "command >N <n", "command1 | ... | commandN >n", "command <n | ...", "... | command <n | ..." are valid  
## concurrent connection-oriented paradigm with shared memory  
When a connection(client) comes in, it fork a child to handle the client  
Every client has unique environment variables  
  
After fork mentioned above, all environment variables of client is clear except PATH is set to bin:.  
  
It create one block of shared memory for all clients(childs)  
  
When __tell__ raised, the child store the message in the shared memory in place of the specific client and send signal to the specific client to inform it receiving message with kill(\<the pid of the specific client(child)\>, SIGUSER1)  
  
When __yell__ raised, the child store the message in the shared memory in place of all clients and raise __tell__ in a *for loop* for all client including itself  
  
When __name__ raised, the child first check every client's name stored in the shared memory, if not repeated, store the name in the shared memory in place of the client and raise __yell__  
  
When __who__ raised, the child check user list in the shared memory and print them all, and if one of them have the same user id, also print "<-me" after it to indicate the client  
  
When __command >n__ raised, the child open a file in /tmp named "/tmp/fifo\<the user id of the client\>\<n\>" with `fd = open(file, O_WRONLY | O_CREAT )`, after `dup2(fd, 1)`, call execvp()  
  
When __command <n__ raised, the child open file named "/tmp/fifo\<n\>\<the user id of the client\>" with `fd = open(file, O_RDONLY)`, after `dup2(fd, 0)`, all execvp() after execvp(), remove the file  

__After a client exit, clear client information, and remove file in /tmp if there exist file such named fifo\<the user id of the client\>\<any user id\> or fifo\<any user id\>\<the user id of the client\>__  

## single-process concurrent paradigm  
When a connection(client) comes in, it give it a file descript number  
Use select() to monitor every file descript number used, include fd for clients and hadling coming connection  
Because it has only one process, it cannot actually let every client has unique environment variables, however, it store PATH for every client individually to simulate all clients have unique PATH, before calling execvp(), it first setenv("PATH", \<PATH value for the client\>, 1).
  
When __tell__ raised, the child get the fd of the target client stored in `struct User user[]`, and then simply use `write(<targetfd>, message, strlen(message))`  
  
When __yell__ raised, raise __tell__ in a *for loop* for all client including itself  
  
When __name__ raised, the child first check every client's name stored in `struct User user[]`, if not repeated, store the name in `struct User user[]` in place of the client and raise __yell__  
  
When __who__ raised, the child check user list in the shared memory and print them all, and if one of them have the same user id, also print "<-me" after it to indicate the client  
  
When __command >n__ raised, `pipe(pipefd[<the user id of the client>][n])`, and `dup2(pipefd[<the user id of the client>][n][1], 1)` then call execvp()  
  
When __command <n__ raised, `dup2(pipefd[<the user id of the client>][n][0], 0)`, then call execvp(), and then `close(pipefd[<the user id of the client>][n][0])` and `close(pipefd[<the user id of the client>][n][1])`  
  
__After a client exit, clear client information,__  
__if there exist pipefd[\<the user id of the client\>][\<any user id\>],__ `close(pipefd[<the user id of the client>][<any user id>][0])` __and__ `close(pipefd[<the user id of the client>][<any user id>][1])`  
__if there exist pipefd[\<any user id\>][\<the user id of the client\>],__ `close(pipefd[<any user id>][<the user id of the client>][0])` __and__ `close(pipefd[<any user id>][<the user id of the client>][1])`  
