#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#define PIPE_NAME "/tmp/mypipe"
void read_pipe_and_print() {
	// block until the other side opened
	int fd = open(PIPE_NAME, O_RDONLY);
	if (fd < 0) {
		perror("failed to open fifo for read");
	}
	char str[512];
	ssize_t size = read(fd, str, sizeof(str));
	if (size > 0) {
		str[size] = 0;
	}
	printf("%s\n", str);
	close(fd);
}
void write_pipe() {
	// block until the other side opened
	int fd = open(PIPE_NAME, O_WRONLY);
	if (fd < 0) {
		perror("failed to open fifo for write");
	}
	write(fd, "this is a test for named pipe", strlen("this is a test for named pipe"));
	close(fd);
}
int main() {
	pid_t pid;
	if (mkfifo(PIPE_NAME, 0600) < 0) {
		perror("");
		exit(1);
	}
	printf("named pipe created\n");
	if ((pid = fork()) == 0) { //child
		write_pipe();
		return 0;
	} else if (pid > 0) { //parent
		read_pipe_and_print();
		int status;
		wait(&status);
	} else { //fork fail
		perror("");
	}
	if (unlink(PIPE_NAME) < 0) {
		perror("");
		exit(1);
	}
	printf("unlink named pipe successfully\n");
	return 0;
}
