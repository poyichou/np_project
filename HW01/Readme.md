# NP Project1  
__When the server accept a client, it fork a child to handle request from client by calling *proccess_request*.__  
  
## proccess_request:  
It first send a welcome message to client. Then start receiving message from client.  
__It hanndle 3 types of message:__  
*	__"xxxxxx\\r\\n":__  	
	excellent one line of message with a newline.  
*	__"xxxxxx\\r\\nxxxxxxx":__  
	Could seldom happen, read many packet one time. In this case, Only use string before *\\n* and leave the rest be concatonated by next message read.  
*	__"xxxxxxxxxxxxxx":__  
	No newline, means message be split into several packets. In this case, continue reading until reach a newline.  
  
__After receiving message from client, server will first remove \\r and \\n, then catogorize this meesage into five case:__  
*	__*"/"*:__  
	Return error message because project spec forbid proccessing this message.  
*	__Start with *"exit"*:__  
	Exit and close connection normally.  
*	__Start with *"printenv"*__:  
	Run build-in function. If there's no string after it, it print all environment variables. If there's a varible name after it, it print value of this variable if this variable exists, and print not match otherwise.  
*	__Start with *"setenv"*:__  
	It should followed by a variable name and a value separated by whitespace, and set this variable to the value.  
*	__Other:__  
	pass it to function "parser"  
  
## parser:  
__It is like a shell, parse one line of message by whitespace into tokens, regarding them as *"command"*s separated by "|" or "|n" and do pipe, number pipe, exec if needed.__  
***********************************************************************************************************  
**numbered pipe**: A new type of pipe in this project. Having symbol such as "|n", where n is an integer.  
"|n" means the stdout of last command should be piped to next nth legal process, where 1 <= N <= 1000.  
"|1" has the same behavior of "|"  
If there is any error in a input line, the line number still count 1.  
And if the nth command is invalid, then the pipe just close.  
For example:  
```bash
% ls |2
% ctt  #unknown command, process number is counted
Unknown command: [ctt].
% nl
1 file1
2 file2
```
Also, it is fine that many commands pipe to the same command  
```bash
%ls |3 ls | nl | nl
1 file1
2 file2
3 1 file1
4 2 file2
```
***********************************************************************************************************  
__It catogorize tokens in to four case:__  
*	__*">"*:__  
	Means token after it is a file name should be writen. When it reach it, it read next token as output file name.   
*	__*"<"*:__   
	Means token after it is a file name should be read. When it reach it , it read next token as input file name.   
*	__*"|"* and *"|n"*:__  
	Means pipe occur, where n is a number, it then store some information into a structure *numbered_pipe_command* for this command, which include:  
	*	__int idx__:		set it to 0.  
	*	__int count__:		set it to n or 1 if it is "|" follows the tokens.  
	*	__int pipe_in_fd[2]__:	set to {0,0}  
		Will call *pipe(pipe_in_fd)* before pipe_in_fd is used  
		pipe_in_fd is used for storing data read from other command, namely, pipe from other command.  
	*	__int pipe_out_fd[2]__: set to {0, 0}  
		Will call *pipe(pipe_out_fd)* before pipe_out_fd is used  
		pipe_out_fd is used for storing data the command self produce after execute.  
*	__Other__:  
	Store them as argments (or tokens) of program executed afterword.  
  
__It execute command with *"execvp"* in two condition:__  
*	__Meets *"|"* or *"|n"*__  
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
		********************************************************************************************  
		__*So the structure for the first condition may like this*__  
		*pipe_out_fd (of command1) =>*  
		*pipe_out_fd (of command2) => read => write => current pipe_in_fd => current command => current pipe_out_fd*  
		*pipe_out_fd (of command3) =>*  
		  
		__*The structure for the second condition may like this if there is an input file*__  
		*refd_in => current command => currnet pipe_out_fd*  
		  
		__*The structure for the second condition may like this if there is no input file*__  
		*current command => currnet pipe_out_fd*  
	 	********************************************************************************************  
  
	Then, *dup2* pipe_out_fd[1] to 1, and execute this command with execvp.  
	  
*	__Meets the final token of one line:__  
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
		__*So the structure for the first condition may like this*__  
		*pipe_out_fd (of command1) =>*  
		*pipe_out_fd (of command2) => read => write => current pipe_in_fd => current command => output file*  
		*pipe_out_fd (of command3) =>*  
		  
		__*The structure for the second condition may like this if there is an input file*__  
		*pipe_out_fd (of command1) =>*  
		*pipe_out_fd (of command2) => read => write => current pipe_in_fd => current command => sockfd of client*  
		*pipe_out_fd (of command3) =>*  
		  
		__*The structure for the third condition may like this if there is an input file*__  
		*refd_in => current command => output file*  
		  
		__*The structure for the third condition may like this if there is no input file*__  
		*current command => output file*  
		  
		__*The structure for the forth condition may like this if there is an input file*__  
		*refd_in => current command => sockfd of client*  
		  
		__*The structure for the forth condition may like this if there is no input file*__  
		*current command => sockfd of client*  
		***************************************************************************************  
	  
	And then, execute this command with execvp.  

