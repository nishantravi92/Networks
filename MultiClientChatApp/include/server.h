#ifndef SERVER_H
#define SERVER_H 

#define ONLINE 1
#define OFFLINE 0
#define BUFFER_SIZE 256
#include<netinet/in.h>
int run_server_application(char *port);
char *parse_input(char *, char *);
void init_file_fd_data(fd_set * ,fd_set * , fd_set * , fd_set *);
void put_addrinfo(struct addrinfo *addr_info,int flags, int family, int socktype, int protocol );	
void init_networking_structures(struct addrinfo * , struct addrinfo **, char *, char *, char *);
void print_and_log_output(char *, void *,const char *);
int get_char_count(char *);
int sendall(int, char *, int *);
void get_hosted_address(struct addrinfo *, char []);

typedef struct {
	char ip_addr[INET_ADDRSTRLEN];
	char *name;
	int port;
	int msg_sent;
	int msg_rev;
 	int status;
	int fd;
	char *blocked_by_list[MAX_CLIENTS];
	char buffer[256];
	char *log[128];
	int log_count;
	bool log_pending;
}Client;
#endif
