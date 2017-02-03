#ifndef CLIENT_H 
#define CLIENT_H 
int run_client_application(char *);

typedef struct {
	char *hostname;
	int port;
    char ip_addr[INET_ADDRSTRLEN];
	char *blocked_list[MAX_CLIENTS];			  
}Host; 

#endif
