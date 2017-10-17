#include<stdio.h>
#include <arpa/inet.h>
void mysetenv(char* ,char* , int);
void printenv();
int main(int argc, char* argv[])
{

}
void mysetenv(char* name, char* value, int replace){
	if(setenv(name, value, replace) == 0){
		printf("change env succeeded!!\n");
	}else{
		printf("change env failed\n");
	}
	return;
}
void myprintenv(){
	int i;
	extern char **environ;
	for(i= 0 ; environ[i]!= NULL ; i++){
		printf("%s\n", environ[i]);
	}
	return;
}
