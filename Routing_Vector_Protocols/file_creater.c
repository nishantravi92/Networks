#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
void main(int argc, char **argv)
{   
    /*Start Here*/
	FILE *fp = fopen("test_file.txt","a");	
	char buffer[1024*10000];
	//char buffer[1024];
	//memset(buffer, '\n', 1024);	
	memset(buffer, '\n', 1024*10000);
	int bytes_wrote = fwrite(buffer, sizeof(char), 1024,fp);
}
