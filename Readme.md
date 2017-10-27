# This is NP Project1  
# Server:  
Server will fork a child to handle request from chilld.  
The child will send a welcome message to client after client connect to it. Then start receiving message from client.  
After receiving message from client, server will first remove \\r and \\n, then catogorize this meesage into five case:  
*	1.\"/\": Return error message because project spec forbid proccess this message.  
*	2.\"exit\": Exit and close connection normally.  
*	3.\"printenv\": Run build-in function. If there's no string after it, it print all environment variables. If there's a varible name after it, it print value of this variable if this variable exists, and print not match otherwise.  
*	4.\"setenv\": It should followed by a variable name and a value, and set this variable to the value.  
*	5.Other: pass it to function \"parser\"  
# parser:  
It is like a shell, parse message by whitespace into tokens, regarding them as a \"command\" separated by \"|\" or \"|n\" and do pipe, number pipe, exec if needed.  
It catogorize tokens in to some case:  
*	\">\": Means token after it is a filename should be writen.  
*	\"<\": Means token after it is a filename should be read.  
*	\"|\" and \"|n\": Means pipe occur, where n is a number, it then store some information into a structure *numbered_pipe_command* for this command, which include:  
	*	int idx:set it to 0.  
	*	int count:set it to n or 1 if it is \"|\" follows the tokens.  
	*	int pipe_in_fd[2]:set to {0,0}  
	*	int pipe_out_fd[2]:set to {0, 0}  
	pipe_in_fd is used for storing data read from other command, namely, pipe from other command.  
	pipe_out_fd is used for storing data the command self produce after execute.  
	After storing, idx++ for every *numbered_pipe_command* stored before this one.  
	For every *numbered_pipe_command*, if idx == count, means the data stored in fd:pipe_out_fd[0] should be read, then read data from pipe_out_fd[0] and write it to pipe_in_fd[1] of current *numbered_pipe_command*, *dup2* pipe_in_fd[0] to 0, *dup2* pipe_out_fd[1] to 1, and execute this command with execvp.  
*	Other: Store them as argment of program executed afterword.  
And when reach the final token of \"Other\" type, it means that it reach the final command, which will not pipe to any other command. Before executing it, as done above, for every *numbered_pipe_command*, do idx++, if idx == count, means that the data stored in pipe_out_fd[0] should be read, then read data from pipe_out_fd[0], and write it to pipe_in_fd[1] for current command *dup2* pipe_in_fd[0] to 0, *dup2* \"sockfd\" to 1, and execute this command with execvp.  

