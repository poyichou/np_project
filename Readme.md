# This is NP Project1  
#### When the server accept a client, it fork a child to handle request from client by calling *proccess_request*.  
  
# proccess_request:  
It first send a welcome message to client. Then start receiving message from client.  
### It hanndle 3 types of message:  
*	#### \"xxxxxx\\r\\n\":  	
	excellent one line of message with a newline.  
*	#### \"xxxxxx\\r\\nxxxxxxx\":  
	Could seldom happen, read many packet one time. In this case, Only use string before *\\n* and leave the rest be concatonated by next message read.  
*	#### \"xxxxxxxxxxxxxx\":
	No newline, means message be split into several packets. In this case, continue reading until reach a newline.  
  
### After receiving message from client, server will first remove \\r and \\n, then catogorize this meesage into five case:  
*	#### \"/\":  
	Return error message because project spec forbid proccessing this message.  
*	#### Start with \"exit\":  
	Exit and close connection normally.  
*	#### Start with \"printenv\":  
	Run build-in function. If there's no string after it, it print all environment variables. If there's a varible name after it, it print value of this variable if this variable exists, and print not match otherwise.  
*	#### Start with \"setenv\":  
	It should followed by a variable name and a value separated by whitespace, and set this variable to the value.  
*	#### Other:  
	pass it to function \"parser\"  
  
# parser:  
### It is like a shell, parse one line of message by whitespace into tokens, regarding them as *\"command\"*s separated by \"|\" or \"|n\" and do pipe, number pipe, exec if needed.  
***************************************************************************************************************************************************************************************  
*numbered pipe: A new type of pipe in this project. Having symbol such as \"|n\", where n is an integer.*  
*\"|n\" means the stdout of last command should be piped to next nth legal process, where 1 <= N <= 1000.*  
*\"|1\" has the same behavior of \"|\"*  
*If there is any error in a input line, the line number still count 1. If the nth command is invalid, then the pipe just close.*  
*For example:*  
*	*%ls |2*  
	*% ctt               <= unknown command, process number is counted*  
	*Unknown command: [ctt].*  
	*% nl*  
	*1 file1*  
	*2 file2*  
*Also, it is fine that many commands pipe to the same command*  
*	*%ls |3 ls | nl | nl*  
	*1 file1*  
	*2 file2*  
	*3 1 file1*  
	*4 2 file2*  
***************************************************************************************************************************************************************************************  
### It catogorize tokens in to four case:  
*	#### \">\":  
	Means token after it is a file name should be writen. When it reach it, it read next token as output file name.   
*	#### \"<\":   
	Means token after it is a file name should be read. When it reach it , it read next token as input file name.   
*	#### \"|\" and \"|n\":  
	Means pipe occur, where n is a number, it then store some information into a structure *numbered_pipe_command* for this command, which include:  
	*	int idx:		set it to 0.  
	*	int count:		set it to n or 1 if it is \"|\" follows the tokens.  
	*	int pipe_in_fd[2]:	set to {0,0}  
		pipe_in_fd is used for storing data read from other command, namely, pipe from other command.  
	*	int pipe_out_fd[2]:set to {0, 0}  
		pipe_out_fd is used for storing data the command self produce after execute.  
*	#### Other:  
	Store them as argments (or tokens) of program executed afterword.  
  
### It execute command with *\"execvp\"* in two condition:  
*	#### Meets \"|\" or \"|n\"  
	It tells parser that tokens it has read should be seems as one command:  
	As mentioned above, it first stores some information into a structure *numbered_pipe_command* for this command.
	After storing, do idx++ for every *numbered_pipe_command* stored before this command.  
	For every *numbered_pipe_command*, if idx == count, means the data stored in fd:pipe_out_fd[0] should be read.  
	If there is no command such that idx == count, then it is OK that input file exists for current command.  
	So it has two cases:  
	*	There is any *numbered_pipe_command* for a command such that idx == count:  
		Read data from pipe_out_fd[0] for all match command and write it to pipe_in_fd[1] of current *numbered_pipe_command*, and then *dup2* pipe_in_fd[0] to 0.  
	*	There is no *numbered_pipe_command* for a command such that idx == count:  
		If there is an input file, set *refd_in* to the fd get after open the file, then *dup2* *refd_in* to 0.  
		***************************************************************************************  
		#### *So the structure for the first condition may like this*  
		*pipe_out_fd (of command1) =>*  
		*pipe_out_fd (of command2) => read => write => current pipe_in_fd => current command => current pipe_out_fd*  
		*pipe_out_fd (of command3) =>*  
		  
		#### *The structure for the second condition may like this if there is an input file*  
		*refd_in => current command => currnet pipe_out_fd*  
		  
		#### *The structure for the second condition may like this if there is no input file*  
		*current command => currnet pipe_out_fd*  
		***************************************************************************************  
	  
	Then, *dup2* pipe_out_fd[1] to 1, and execute this command with execvp.  
	  
*	#### Meets the final token of one line:	
	When it reach the final token of *Other* type, it means that it reach the final command, which will not pipe to any other command.  
	Before executing it, as done above, for every *numbered_pipe_command*, do idx++.  
	if idx == count, means that the data stored in pipe_out_fd[0] should be read.  
	Also this is the only time that it could exist output file.  
	So, it has some cases:  
	*	There is any *numbered_pipe_command* for a command such that idx == count and there is an ouput file:  
		Read data from pipe_out_fd[0] for all match command and write it to pipe_in_fd[1] of current *numbered_pipe_command*, *dup2* pipe_in_fd[0] to 0.  
		Set *refd_out* to the fd get after open the file, then *dup2* *refd_out* to 1.  
	*	There is any *numbered_pipe_command* for a command such that idx == count and there is no ouput file:  
		Read data from pipe_out_fd[0] for all match command and write it to pipe_in_fd[1] of current *numbered_pipe_command*, *dup2* pipe_in_fd[0] to 0.  
		*dup2* *sockfd* to 1.  
	*	There is no *numbered_pipe_command* for a command such that idx == count and there is an ouput file:  
		Set *refd_in* to the fd get after open the file, then *dup2* *refd_in* to 0.  
		Set *refd_out* to the fd get after open the file, then *dup2* *refd_out* to 1.  
	*	There is no *numbered_pipe_command* for a command such that idx == count and there is no ouput file:  
		If there is an input file, set *refd_in* to the fd get after open the file, then *dup2* *refd_in* to 0.  
		*dup2* *sockfd* to 1.  
		***************************************************************************************  
		#### *So the structure for the first condition may like this*  
		*pipe_out_fd (of command1) =>*  
		*pipe_out_fd (of command2) => read => write => current pipe_in_fd => current command => output file*  
		*pipe_out_fd (of command3) =>*  
		  
		#### *The structure for the second condition may like this if there is an input file*  
		*pipe_out_fd (of command1) =>*  
		*pipe_out_fd (of command2) => read => write => current pipe_in_fd => current command => sockfd of client*  
		*pipe_out_fd (of command3) =>*  
		  
		#### *The structure for the third condition may like this if there is an input file*  
		*refd_in => current command => output file*  
		  
		#### *The structure for the third condition may like this if there is no input file*  
		*current command => output file*  
		  
		#### *The structure for the forth condition may like this if there is an input file*  
		*refd_in => current command => sockfd of client*  
		  
		#### *The structure for the forth condition may like this if there is no input file*  
		*current command => sockfd of client*  
		***************************************************************************************  
	  
	And then, execute this command with execvp.  
