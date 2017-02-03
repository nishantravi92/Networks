#include <iostream>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "../include/logger.h"
#include "../include/global.h"
#include "../include/server.h"
#include "../include/client.h"
#include <list>

using namespace std;

static const char *name = "n7";
static char hostname[HOSTNAME_LEN];
static char ip_addr[INET_ADDRSTRLEN];

static int init_network_structures_and_connect(char *,char *, int, char *, struct addrinfo *, struct addrinfo *, int *);
static void print_error_message(char *); 
static void fill_client_list(list<Host> &, char *);
static void display_client_list(list<Host> &);
static void deallocate_list_memory(list<Host> &);
static bool is_sender_valid(char *, list<Host> &);
static bool insert_into_blocked_list(char *, list<Host> &, list<string> &);
static bool unblock_ip(char *, list<Host> &, list<string> & );
Host *get_client_for_ip(char *, list<Host> &);

/*From Beej */
void *get_in_addrs(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
	}
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
/*Runs the whole client application */
int run_client_application(char *port)
{
	fd_set read_master;
    fd_set read_fds;
    fd_set write_master;							//Not actually used
    fd_set write_fds;								//Not actually used
    int fdmax;
	char server_ip[INET_ADDRSTRLEN];
    int port_integer = atoi((const char *)port);
	char recv_buffer[4*BUFFER_SIZE];				//Extra size as data size may be large	  	
	char buffer[BUFFER_SIZE];
	bool logged_in = false;
    int yes = 1;
    int sockfd = -1;
    socklen_t addrlen;
    int nbytes = 1;
    struct addrinfo hints, *servinfo;
	list<Host> client_list;	
	list<string> block_list;
    char *command_str;
	init_file_fd_data(&read_master,&read_fds, &write_master, &write_fds);

for(;;) {
	read_fds = read_master;									//Need a temporary set read_fds as "select" actually changes this set
	if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
 	   return ERROR+3;
	for(int i=0;i<=fdmax;i++) {
		if(FD_ISSET(i, &read_fds)) {
			if(FD_ISSET(STDIN_FILENO, &read_fds)) {
				char *token, *save_ptr;
                fgets(buffer, sizeof buffer, stdin);
                char temp_buffer[BUFFER_SIZE];
				memcpy(temp_buffer,buffer, strlen(buffer));  
                int n = strlen(temp_buffer);
                token = strtok_r(buffer, " ", &save_ptr);
                command_str =  parse_input(command_str, token);
				if(strcmp(command_str, "PORT") == 0 && logged_in) {
	                print_and_log_output(command_str, &port_integer, "PORT");
                }
                else if(strcmp(command_str, "AUTHOR") == 0) {
	                print_and_log_output(command_str,(void *) &name[0] , "AUTHOR");
                }
				else if(strcmp(command_str, "IP") == 0 && logged_in) {
	                 print_and_log_output(command_str, &ip_addr[0], "IP");
				}
			    else if(strcmp(command_str, "LOGIN") == 0 && !logged_in) {
	                int rv;
                    token = strtok_r(NULL, " ", &save_ptr);             //Get the the server IP
                    memcpy(server_ip, token, INET_ADDRSTRLEN);
                    token = strtok_r(NULL, "", &save_ptr);				//Get the port
					char port[strlen(token)];
					port[strlen(token)-1] = '\0';	
                    memcpy(port, token, strlen(token)-1);				//-1 to account for the \n which is at the end	
                    if( init_network_structures_and_connect(server_ip, port, port_integer, ip_addr, &hints, servinfo, &sockfd) != -1) {
                       if(sockfd >= fdmax)
                            fdmax = sockfd+1;
                    int temp = sockfd;
                    FD_SET(sockfd, &read_master);
                    cse4589_print_and_log("[%s:SUCCESS]\n", "LOGIN");
                    cse4589_print_and_log("[%s:END]\n", "LOGIN");
                    logged_in = true;
                	}
                 	else {
						 print_error_message(command_str); 
                	}
              	 }
				 else if(strcmp(command_str, "LIST") == 0 && logged_in) { //Checks for all possible valid commands
					display_client_list(client_list);					
               	 }
	             else if(strcmp(command_str, "REFRESH") == 0 && logged_in) {
						cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
                        cse4589_print_and_log("[%s:END]\n", command_str);
					    sendall(sockfd, temp_buffer, &n);						  
            	 }
        	     else if(strcmp(command_str, "BLOCK") == 0 && logged_in) {
					token = strtok_r(NULL, " ", &save_ptr);
					if(insert_into_blocked_list(token, client_list, block_list)) { 
                   	 	sendall(sockfd, temp_buffer, &n);
						 cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
                         cse4589_print_and_log("[%s:END]\n", command_str);
					}
					else print_error_message(command_str);	
                 }
                else if(strcmp(command_str, "UNBLOCK") == 0 && logged_in) {
					 if(token != NULL)
						 token = strtok_r(NULL,"", &save_ptr);	
					 if(unblock_ip(token, client_list, block_list)) {
                    	 sendall(sockfd, temp_buffer, &n);
						 cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
	                     cse4589_print_and_log("[%s:END]\n", command_str);
					}
					else print_error_message(command_str);
                 }
                else if(strcmp(command_str, "SEND") == 0 && logged_in) {
					token = strtok_r(NULL, " ", &save_ptr);						//Get the IP address
					if(is_sender_valid(token, client_list)) {
                    	 sendall(sockfd, temp_buffer, &n);
						 cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
	                     cse4589_print_and_log("[%s:END]\n", command_str);
					}
				    else 
						print_error_message(command_str);
                }
                else if(strcmp(command_str, "BROADCAST") == 0 && logged_in) {
                    sendall(sockfd, temp_buffer, &n);
					cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
                    cse4589_print_and_log("[%s:END]\n", command_str);
                }
				else if(strcmp(command_str, "LOGOUT") ==0 && logged_in) {
	                if(sockfd != -1)
		                close(sockfd);
	                 FD_CLR(sockfd, &read_master);
                     sockfd = -1;
                     logged_in = false;
                     print_and_log_output(command_str, &ip_addr[0], "");
                }
				else if(strcmp(command_str, "EXIT") == 0) {
					 if(sockfd != -1)
	                     close(sockfd);
	                print_and_log_output(command_str, &ip_addr[0], "EXIT");
                    return 0;
                }
				else {
					 print_error_message(command_str); 
				}
				free(command_str);
				command_str = NULL;
			}
			else if(i = sockfd) {
				if((nbytes = recv(sockfd, recv_buffer, sizeof recv_buffer, 0)) > 0){  
 	   	    	    recv_buffer[nbytes] = '\0';
					if(recv_buffer[0] == '1')					//Charecterizes a refresh or list message
						fill_client_list(client_list, recv_buffer);
					else {										//Else all other messages are messages received by the client 
						if( strncmp(recv_buffer, "msg", 3) == 0 ) {
							cse4589_print_and_log("[%s:SUCCESS]\n", "RECEIVED");
							cse4589_print_and_log("%s", recv_buffer);
							cse4589_print_and_log("[%s:END]\n", "RECEIVED");
						}
					}
					memset(recv_buffer, '\0',sizeof buffer); 
				}
				else {
					if(sockfd != -1)
	                    close(sockfd);
                    FD_CLR(sockfd, &read_master);
                    sockfd = -1;
                    logged_in = false;
				}
			}
		}
	}
}

}
/*Initialized structures needed for networking and gets data of self as well as host that we want to connect to */
int init_network_structures_and_connect(char *server_ip, char *port, int client_port,char client_ip[],  struct addrinfo *hints, struct addrinfo *serv_info, int *sockfd)
{
	
	int rv;
	memset(hints, 0, sizeof hints);	
	put_addrinfo(hints, AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP);     //Fill addrinfo fields with the correct data passed
	gethostname(hostname, HOSTNAME_LEN);                                    //Get HostName  
	if ((rv = getaddrinfo(hostname, port, hints, &serv_info)) != 0) {       //Get the IP address of the host
	 fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }
 	get_hosted_address(serv_info, ip_addr);									//Get the hostname ip address and put in into ip_addr 
	memset(hints, 0, sizeof hints);
    hints->ai_family = AF_INET;
    hints->ai_socktype = SOCK_STREAM;		
	hints->ai_protocol = IPPROTO_TCP;
	if ((rv = getaddrinfo(server_ip, port, hints, &serv_info)) != 0)		//Get details about the host we are trying to connect to
		return ERROR;		 	

	if( (*sockfd = socket(serv_info->ai_family, serv_info->ai_socktype, serv_info->ai_protocol)) == -1)  //Create a socket witd data	
		return ERROR;

	struct sockaddr_in client_sock;
	// Copied from stackoverflow http://stackoverflow.com/questions/2605182/when-binding-a-client-tcp-socket-to-a-specific-local-port-with-winsock-so-reuse
    client_sock.sin_family = AF_INET;
    client_sock.sin_addr.s_addr= htonl(INADDR_ANY);
    client_sock.sin_port=htons(client_port);				//End of copy 
    if(bind(*sockfd,(struct sockaddr *)&client_sock,sizeof(client_sock)) < 0)
		return -1;
	int yes = 1;	
	setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if( connect(*sockfd, serv_info->ai_addr, serv_info->ai_addrlen) == -1) {     //Try to connect
		close(*sockfd);
		return -1; 	
	}
	return 1;
}
/* Print error message for the command given */
void print_error_message(char *command_str) {
	 cse4589_print_and_log("[%s:ERROR]\n", command_str);
     cse4589_print_and_log("[%s:END]\n", command_str);
}

/* Fills client list when client logs in or when refresh command is end */
void fill_client_list(list<Host> & client_list, char *buffer) {
	if(!client_list.empty()) {	
		deallocate_list_memory(client_list);
		client_list.clear();
	}
	char *token,*save_ptr;
	token = strtok_r(buffer, " ", &save_ptr);							//Removes listcount
	while(true) {
		 if(token == NULL)
	         break;
		token = strtok_r(NULL, " ", &save_ptr);							//Parses hostname
		if(token == NULL)
			break;
		Host client;
		int n = strlen(token)+1;
		char *name = (char *)malloc(sizeof(char)*n);
		name[n-1] = '\0';
		memcpy(name,token,n-1);
		client.hostname = name;
		token = strtok_r(NULL, " ", &save_ptr);							//Parses ip address
		if(token == NULL)
	        break;
		memcpy(client.ip_addr, token, strlen(token));		
		client.ip_addr[strlen(token)] = '\0'; 
		token = strtok_r(NULL, " ", &save_ptr);							//Parses port number
 	    if(token == NULL)
	    	break;
		client.port = atoi(token);
		client_list.push_back(client);
		if(token != NULL)
			token = strtok_r(NULL, " ", &save_ptr);						//Parses character \n
		if(token != NULL) {
			 if(strncmp(save_ptr, "msg", 3) == 0) {
	             cse4589_print_and_log("[RECEIVED:SUCCESS]\n");
				 token = strtok_r(NULL, "\n", &save_ptr);                //msg from
				 cse4589_print_and_log("%s\n", token);
				 token = strtok_r(NULL, "\n", &save_ptr);                //msg:
				 cse4589_print_and_log("%s\n", token);
				 cse4589_print_and_log("[RECEIVED:END]\n");	
				 while(token != NULL) {
					 token = strtok_r(NULL, "\n", &save_ptr);
					 if(token == NULL)
						break;
					 cse4589_print_and_log("[RECEIVED:SUCCESS]\n");
					 cse4589_print_and_log("%s\n", token);
					 token = strtok_r(NULL, "\n", &save_ptr);	
					 cse4589_print_and_log("%s\n", token);
					 cse4589_print_and_log("[RECEIVED:END]\n");
				 }	
	             break;
			}
			token = strtok_r(NULL, " ", &save_ptr);
		}
	}	
}

/*Displays the client_list to the user on the list command */
void display_client_list(list<Host> & client_list) {
	cse4589_print_and_log("[%s:SUCCESS]\n", "LIST");
	list<Host>::iterator i; 
	int count = 1;
	for(i= client_list.begin();i != client_list.end();++i) {
		cse4589_print_and_log("%-5d%-35s%-20s%-8d\n",count, i->hostname, i->ip_addr,i->port);
		count++;
	}
	cse4589_print_and_log("[%s:END]\n", "LIST");	
}

void deallocate_list_memory(list<Host> &client_list) {
	list<Host>::iterator i;
	for(i = client_list.begin(); i != client_list.end(); ++i) {
		free(i->hostname);		
	} 
}
/*Checks to see if the sender that we have received as input is valid or not.
 * Retrieves the IP from a list stored in the client_list */ 
bool is_sender_valid(char *ip, list<Host> &client_list) {
	char *ip_addr = parse_input(ip_addr, ip);
	Host *client = get_client_for_ip(ip_addr,client_list);	
	free(ip_addr);
	if(client != NULL) 
		return true;
	else return false;
}
/*Returns the client that is inputted, from the client_list */
Host *get_client_for_ip(char *ip_addr, list<Host> &client_list) {
	list<Host>::iterator i;
	for(i = client_list.begin();i!= client_list.end();++i) {
		if(strcmp(i->ip_addr, ip_addr) == 0)
			return &(*i);		
	}
	return NULL;	
}
/*Inserts the IP if valid into the blocked list
 * Returns true on successgul, false otherwise */
bool insert_into_blocked_list(char *ip, list<Host> &client_list, list<string> &block_list) {
	char *ip_addr = parse_input(ip_addr, ip);
	Host *client = get_client_for_ip(ip_addr, client_list);
	free(ip_addr);
	if(client == NULL)
		return false;
	list<string>::iterator i;
	for(i=block_list.begin(); i!=block_list.end();++i) {		//Compares if the IP already has been blocked or not
		if((*i).compare(client->ip_addr) == 0) {
			return false; 
		}
	}
	 char *ipaddress = (char *)malloc(sizeof(char)*strlen(client->ip_addr)+1);
     ipaddress[strlen(client->ip_addr)] = '\0';
	 string str(ipaddress);
	 free(ipaddress);
     block_list.push_back(str);
	 return true;
}
/*Removes the IP from the blocked list
 * Returns true if the IP was unblocked, false otherwise */
bool unblock_ip(char *ip, list<Host> &client_list, list<string> &block_list ) {
	list<string>:: iterator i;
	for(i = block_list.begin();i != block_list.end();++i) {
		if((*i).compare(ip)) {
			block_list.erase(i);
			return true;
		}
	}
	return false;	
}
