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
#include <unistd.h>
#include "../include/logger.h"
#include "../include/global.h"
#include "../include/server.h"
#include <list>
using namespace std;

static void update_client_list(struct sockaddr_storage, int , list<Client>& );
static void update_log_data(struct sockaddr*, char *,  int , list<Client> &);
static Client *get_client_from_list_and_update(struct sockaddr_storage,int ,list<Client> &);
static Client *get_client_for_ip(char *, list<Client>&); 
static void list_output(list<Client> &, int);
static void display_statistics(list<Client> &);
static Client *get_client_for_fd(int , list<Client>&);
static bool handle_message_received(char *, int, int,  list<Client> &);
static void log_output(Client *);
static bool compare_func(Client &, Client &);
static void display_blocked_clients(Client *, list<Client> &);
static bool is_sender_blocked_by_receiver(Client *, Client *); 

static const char *name = "n7";
static char hostname[HOSTNAME_LEN];
static char ip_addr[INET_ADDRSTRLEN];
static void send_message(Client *, Client *, char *, char *, int); 

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
		}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
/* Runs the server application completely
 * For the socket implementation part, sending and receiving data and closing socket, code parts have been taken from Beej */   
int run_server_application(char *port)
{
	fd_set read_master;
	fd_set read_fds;
	fd_set write_master;
	fd_set write_fds;
	int fdmax;
	int port_integer = atoi(port);
	char buffer[BUFFER_SIZE];
	int listener;
	int yes = 1;
	int newfd;
	struct sockaddr_storage remoteaddr; // client address Testing
	socklen_t addrlen = addrlen = sizeof remoteaddr;
	int nbytes;
	struct addrinfo hints, *servinfo;
	char *command_str;
	char remoteIP[INET_ADDRSTRLEN];
	std::list<Client > client_list;
	init_file_fd_data(&read_master,&read_fds, &write_master, &write_fds);
	init_networking_structures(&hints, &servinfo, port, hostname, ip_addr);		//Get IP address as well as set up basic structures for use

	listener = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if(listener == -1)													//Something wrong with creating a socket, return error
		return ERROR;
	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));  //Multiple connections for same socket option yes

	if (bind(listener, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {   //Bind the socket, if something is wrong return ERROR
        close(listener);
		return ERROR;
	}
	if (listen(listener, MAX_CLIENTS) == -1) {					//Start listening on this socket for connections
		perror("listen");
	}
	FD_SET(listener, &read_master);
	fdmax = listener;

	for(;;)
	{
		  read_fds = read_master;							//Has to be done as select changes the read_fds set
          write_fds = write_master;
		 if (select(fdmax+1, &read_fds, &write_fds, NULL, NULL) == -1)
 		    return ERROR+3;
    	 for(int i=0;i<=fdmax;i++) {
			if(FD_ISSET(i, &read_fds)) {
				if(i == listener) {
                	newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen); //Accept a new connection from a client
                	if (newfd == -1) 
                    	perror("accept");
                	else {
                    	FD_SET(newfd, &read_master);									 // add to master set
                    	if (newfd > fdmax)    											 // keep track of the max fd number till now
                    	    fdmax = newfd;
						update_client_list(remoteaddr, newfd, client_list);				//Puts the client into the list
						list_output(client_list, newfd);								//Sends the list of clients to the client just logged in
						log_output(get_client_for_fd(newfd, client_list));				// If any logs are pending, send it to the client	
			 		}

				}
				else if(FD_ISSET(STDIN_FILENO, &read_fds)){
					char *token,*save_ptr;
					fgets(buffer, sizeof buffer, stdin);
					token = strtok_r(buffer, " ", &save_ptr); 
					if(token != NULL)
						command_str =  parse_input(command_str, token);
					if(strcmp(command_str, "PORT") == 0) 
						print_and_log_output(command_str, &port_integer, "PORT");
					else if(strcmp(command_str, "AUTHOR") == 0) 
						print_and_log_output(command_str,(void *) &name[0] , "AUTHOR");
					else if(strcmp(command_str, "IP") == 0) 
						print_and_log_output(command_str, &ip_addr[0], "IP");
					else if(strcmp(command_str, "LIST") == 0) {
						cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
						list_output(client_list, STDIN_FILENO);
						cse4589_print_and_log("[%s:END]\n", command_str);
					}
					else if(strcmp(command_str, "STATISTICS") == 0) {				//Display statistics
						cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
						display_statistics(client_list);
						cse4589_print_and_log("[%s:END]\n", command_str);
					}
			     	else if(strcmp(command_str, "BLOCKED") == 0) {
						token = strtok_r(NULL, " ", &save_ptr);		
						char *ip;
						if(token != NULL)
							ip = parse_input(command_str, token);
						Client *client_ip = get_client_for_ip(ip, client_list);
					  	if(client_ip == NULL) {			//If client not found print error to console
							cse4589_print_and_log("[%s:ERROR]\n", command_str);
							cse4589_print_and_log("[%s:END]\n", command_str);
						}		
						else {							//Else print the list for the current client
							display_blocked_clients(client_ip, client_list);
						}
						fflush(stdout);
						free(ip);
					}
					else if(strcmp(command_str, "EXIT") == 0) {
					    print_and_log_output(command_str, &ip_addr[0], "EXIT");
						return 0;	 
					}
					else {
						cse4589_print_and_log("[%s:ERROR]\n", command_str);
						cse4589_print_and_log("[%s:END]\n", command_str);
					}
					memset(buffer, '\0', BUFFER_SIZE);
					free(command_str);	 
				}		//Ends conditional loop for STDIN_FILENO
				else {
					 Client *client =  get_client_for_fd(i, client_list);
					 if ((nbytes = recv(i, client->buffer, sizeof buffer, 0)) <= 0) { // got error or connection closed by client
                    	 if (nbytes <= 0) {  // connection closed
                         } else {
                     	      perror("recv");
                         }
                        close(i); // bye!
						FD_CLR(i, &read_master);
						if(client!= NULL) {
							client->status = OFFLINE;
						}
                        FD_CLR(i, &read_master); 		// remove from both  master sets
					}
					else {								//Else some data has been received on the FD, handle the message
						handle_message_received(client->buffer, nbytes, i, client_list);
					}
				}
			}			//Ends conditional loop for any FD_ISSET
						//
		}				//Ends looping through all file descriptors checking for active ones 
	}					//Ends the infinite for loop
}
/*get the presentation side IP of a client from struct addrinfo */
void get_hosted_address(struct addrinfo *servinfo, char ip_addr[])
{
	void *addr;
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)servinfo->ai_addr;
    addr = &(ipv4->sin_addr);
    inet_ntop(servinfo->ai_family, addr, ip_addr, INET_ADDRSTRLEN);
}

/*Put the data into the addrinfo structure for future use */
void put_addrinfo(struct addrinfo *addr_info,int flags, int family, int socktype, int protocol )
{
	addr_info->ai_family = family;
	addr_info->ai_flags = flags;
	addr_info->ai_protocol = protocol;
	addr_info->ai_socktype = socktype;
}
/*Initializes the master and temp read and write fd data sets
 * Also adds STDIN and STDOUT to these sets */ 
void init_file_fd_data(fd_set *read_master,fd_set *read_fds, fd_set *write_master, fd_set *write_fds)
{
 	FD_ZERO(read_master);           // clear the master and temp sets
    FD_ZERO(read_fds);
    FD_SET(STDIN_FILENO, read_master);

	FD_ZERO(write_master);           // clear the master and temp sets for write as well
    FD_ZERO(write_fds);
    FD_SET(STDOUT_FILENO, write_master);
} 
/*Parses input upto carraige return or new line and returns pointer to a new string constructed uptil that */ 
char *parse_input(char *command_str, char *buffer)
{
	int count = -1;
	while(buffer[++count] != '\n'&& buffer[count] != '\r' );
	command_str = (char *)malloc(sizeof(char)*(count+1));
	command_str[count--] = '\0';
	while(count >= 0) {
		command_str[count] = buffer[count--];
	}
	return command_str;
}
/*Print and log output according to the input given
 *States if a command is successful or not */  
void print_and_log_output(char *command_str, void *str, const char *cons_output)
{
	cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
	if(!strcmp(command_str, "PORT")) {
          cse4589_print_and_log("%s:%d\n",cons_output, *(int *)str);
	}
	else if(!strcmp(command_str, "AUTHOR")) {
		char *str_out = (char *)str;
        cse4589_print_and_log("I, %s, have read and understood the course academic integrity policy.\n", str_out);
	}	
	else if(!strcmp(command_str, "IP")) {
	   char *str_out = (char *)str;	
       cse4589_print_and_log("%s:%s\n",cons_output, str_out);
	}
	cse4589_print_and_log("[%s:END]\n", command_str);	
}
/* DOes basic initialization of structures used for networking like addrinfo
 * Also gets the host IP address which is needed.
 * Parts have been taken from Beej for connection part */ 
void init_networking_structures(struct addrinfo *hints, struct addrinfo **servinfo, char *port, char *hostname, char *ip_addr)
{
	int rv;	
	memset(hints, 0, sizeof (struct addrinfo));								//Set all fields of addrinfo to zero
	put_addrinfo(hints, AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP);   	//Fill addrinfo fields with the correct data passed
	gethostname(hostname, HOSTNAME_LEN);									//Get HostName	
	 if ((rv = getaddrinfo(hostname, port, hints, servinfo)) != 0) {       //Get the IP address of the host
 	    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
     }
	get_hosted_address(*servinfo, ip_addr); 
	memset(hints, 0, sizeof hints);                                        //Reset addrinfo    
	put_addrinfo(hints, AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP);    //Fill addrinfo struct fields
    if ((rv = getaddrinfo(NULL, port, hints, servinfo)) != 0) {          //Fill struct servinfo with data for listening for any IP 
           fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }
}
/* Updates the client list whe a new connection is established. If a previously offline client has connected, then the list is updated or else a new data
 * entry is added to the client_list */
void update_client_list(struct sockaddr_storage clientaddr, int fd, list<Client >& client_list)
{
	 Client *prev = get_client_from_list_and_update(clientaddr, fd, client_list);
	 if(prev) {
		client_list.sort(compare_func);
		return;
	}
	 Client c;															//Create a new instance of a client
	 char host[NI_MAXHOST];
	 char service[20];
	 struct sockaddr_in *s = (struct sockaddr_in *)&clientaddr;
     socklen_t client_addrlen;
	 c.status = ONLINE;
	 c.fd = fd;
	 c.msg_sent = 0;
	 c.msg_rev = 0;
	 c.log_count = 0;
	 c.log_pending = false;		
	 for(int i=0;i<MAX_CLIENTS;i++)
		c.blocked_by_list[i] = NULL;
	/* printf("selectserver: new connection from %s on "
                          "socket %d\n",
                          inet_ntop(AF_INET,
                              get_in_addr((struct sockaddr*)&clientaddr),
                              c.ip_addr, INET_ADDRSTRLEN),
  fd); */  //Delete this later on 
	inet_ntop(AF_INET, get_in_addr((struct sockaddr*)&clientaddr), c.ip_addr, INET_ADDRSTRLEN);
	 getnameinfo((struct sockaddr *)&clientaddr,sizeof clientaddr, host, sizeof host, service, sizeof service, 0);	//Get name info of client logged in
	 int count = get_char_count(host);
	 c.name = (char *)malloc(sizeof(char)*count);	
	 memcpy(c.name, host, count);
	 c.port = ntohs(s->sin_port);
	 client_list.push_back(c);	
	 client_list.sort(compare_func);							//Sort into ascending based on port numbers of clients
}

/*Gets the client from the present list and if it is present, updates data like status, client_fd and port number
 * If the client is not found returns null */  
Client *get_client_from_list_and_update(struct sockaddr_storage clientaddr, int client_fd, list<Client >&client_list)
{
	 struct sockaddr_in *s = (struct sockaddr_in *)&clientaddr;
	 char host[NI_MAXHOST];
     char service[20];
	 char *name = (char *)malloc(sizeof(char)*NI_MAXHOST);
     getnameinfo((struct sockaddr *)&clientaddr,sizeof clientaddr, host, sizeof host, service, sizeof service, 0); //get name of host from address
	 memcpy(name, host, strlen(host));
	 list<Client>::iterator i;
//Traverses and finds if the list contains the same host name. If so then updates this information in the list itself
	 for(i = client_list.begin();i != client_list.end(); ++i)
	 {
		if(strcmp(name, i->name) == 0) {
			if(i->status == OFFLINE) {
				i->fd = client_fd;
				i->port = ntohs(s->sin_port);
				i->status = ONLINE;
				return &(*i); 			
			}
		}
	 }
	free(name);
	return NULL;	
}
/*Displays the output when the user enters a list command
 *Outputs only those clients who are online */
void list_output(list<Client> & client_list, int fd)
{
	list<Client>::iterator i;	
    int count = 1;
	if(fd == STDIN_FILENO) {
		for(i = client_list.begin(); i != client_list.end();++i) {
			if((*i).status == ONLINE) {						//If client is online then print it 
				cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", count, (*i).name, (*i).ip_addr, (*i).port);
				count++;	
			}
		}
	}
	else {								//Send the list to the client asking for it or on refresh command
		 int n = 0;	
		 int full_size = 0;
		 char buffer[BUFFER_SIZE*4];
		 memset(buffer, ' ', sizeof buffer);
		 for(i = client_list.begin(); i != client_list.end();++i) {
             if((*i).status == ONLINE) {
                 n = sprintf(buffer+full_size,"%-5d%-35s%-20s%-8d\n", count, (*i).name, (*i).ip_addr, (*i).port);
				 full_size+=n+1;	
				 buffer[full_size-1] = ' ';
                 count++;
             }   
         }
		 sendall(fd, buffer, &full_size);   
	}

}

int get_char_count(char *c)
{
	int count = 0;
	while(c[count++] != '\0');
	return count; 
}
/*Sends log data to the client which was received when it was offline and at the point when it has just come online again */
void log_output(Client *prev)
{
	 if(prev->log_pending) {
   	  for(int i=0;i<prev->log_count;i++) {
    	  int n = strlen(prev->log[i]);
          sendall(prev->fd, prev->log[i], &n);
          free(prev->log[i]);
          prev->msg_rev++;
       }   
      prev->log_pending = false;
      prev->log_count = 0;
	 }
}
//Copied From Beej: Used to send all the data to the fd specified 
int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
}
    *len = total; // return number actually sent here
    return n==-1?-1:0; // return -1 on failure, 0 on success
}
/*Display list of clients connected till date with message sent and received statistics */  
void display_statistics(list<Client> & client_list)
{
	 list<Client>::iterator i;
     int count = 1;
     const char *stat[] = {"offine", "online"};		
     for(i = client_list.begin(); i != client_list.end();++i) {
             cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", count, (*i).name, (*i).msg_sent, (*i).msg_rev, stat[i->status]);
             count++;
     }
} 
/* Get client based on file descriptor */
Client *get_client_for_fd(int fd, list<Client> &client_list)
{
	list<Client>::iterator i;
	 for(i = client_list.begin(); i != client_list.end();++i) { 
			if(fd == (*i).fd)
				return &(*i);
	}
	return NULL;
}
/*Get IP based on IP address */
Client *get_client_for_ip(char *ip_addr, list<Client> &client_list)
{
    list<Client>::iterator i;
     for(i = client_list.begin(); i != client_list.end();++i) {
            if(strcmp(ip_addr,(*i).ip_addr) == 0)
                return &(*i);
    }
    return NULL;
} 
/*Handle message received from clients.
 * Depending on BROADCAST SEND or BLOCK, respective action is taken by the server
 * If some error takes place while performing these tasks False is returned
 */ 
bool handle_message_received(char *buffer, int nbytes, int fd, list<Client>& client_list )
{
	char recv_ip_addr[INET_ADDRSTRLEN];
	char *save_ptr, *token;
	token = strtok_r(buffer, " ", &save_ptr);				//Get the command that is received from client
	if(strcmp(token, "SEND") == 0) {
		token = strtok_r(NULL, " ", &save_ptr);				//Get the IP address of the receiver
		memcpy(recv_ip_addr, token, INET_ADDRSTRLEN);
		token = strtok_r(NULL, "", &save_ptr);				//Ge the whole message sent by the sender
		Client *receiver = get_client_for_ip(recv_ip_addr, client_list);
		Client *sender = get_client_for_fd(fd, client_list);
		char *whole_message = (char *)malloc(sizeof (char ) *BUFFER_SIZE);
		char *message = parse_input(message, token);
		if( sender != NULL && receiver != NULL) {
			send_message(sender, receiver, message, whole_message, SEND_MESSAGE);
		free(message);
		}
		 else return false;			 
	}
	else if(strcmp(token, "BROADCAST") == 0) {
		token = strtok_r(NULL, "", &save_ptr);
		Client *sender = get_client_for_fd(fd, client_list);
		char *whole_message = (char *)malloc(sizeof (char ) *BUFFER_SIZE);		//Do not free this pointer as this is used by logging function	 	
		char *message = parse_input(message, token);
		list<Client>::iterator i;
		for(i=client_list.begin();i != client_list.end();++i) {
			if(i->fd != sender->fd)
				send_message(sender, &(*i), message, whole_message, BROADCAST_MESSAGE);			
		}
		cse4589_print_and_log("[%s:SUCCESS]\n", "RELAYED");
        cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", sender->ip_addr, "255.255.255.255", message);
        cse4589_print_and_log("[%s:END]\n", "RELAYED");
		free(message);
	}
	else if(strcmp(token, "BLOCK") == 0) {
	   token = strtok_r(NULL, "", &save_ptr);
	   char *c = parse_input(c, token);
       memcpy(recv_ip_addr, c, INET_ADDRSTRLEN);
	   free(c);	  
	   Client *blocker = get_client_for_fd(fd, client_list);	
	   Client *blockee = get_client_for_ip(recv_ip_addr, client_list);
	   if(blocker != NULL && blockee != NULL) {
		 for(int i=0;i<=MAX_CLIENTS;i++) {
			if(i == MAX_CLIENTS) 
				 break;
			if(blockee->blocked_by_list[i] == NULL) {
				int n = get_char_count(blocker->name);
			    char *name = (char *)malloc(sizeof(char)*NI_MAXHOST);		//Create a char with size = Max_Host
				name[n] = '\0';
				memcpy(name, blocker->name, n);	
				blockee->blocked_by_list[i] = name;
				break;
			}			
		 }	
	   }
	}
	else if(strcmp(token, "UNBLOCK") == 0) {
		token = strtok_r(NULL, "", &save_ptr);
		char *c;
		if(token != NULL)	
	    	c = parse_input(c, token);				//Parses input as there is a \n at the end
        memcpy(recv_ip_addr, c, INET_ADDRSTRLEN);
        free(c);
        Client *blocker = get_client_for_fd(fd, client_list);
        Client *blockee = get_client_for_ip(recv_ip_addr, client_list);	
		if(blocker != NULL && blockee != NULL) {
			for(int i=0;i<=MAX_CLIENTS;i++) {
				if(i == MAX_CLIENTS) 
					 break;
	            if(blockee->blocked_by_list[i] != NULL) {							
					if(strcmp(blockee->blocked_by_list[i], blocker->name) == 0) {
						free(blockee->blocked_by_list[i]);					//Free this memory and make it null for the next data  
						blockee->blocked_by_list[i] = NULL;
						break;
					}
				}
			}
		}
	}
	else if(strncmp(token, "REFRESH", 7) == 0) {
		list_output(client_list, fd);
	}
	memset(buffer,'\0', BUFFER_SIZE); 

}

/*Compare function used to sort lists */
bool compare_func(Client &a, Client &b)
{
	if(a.port < b.port)
		return true;
	return false;
}  
/*Send message to another client. The last parameter indicates if the message is a broadcast message or a sigle sender message */
void send_message(Client *sender, Client *receiver, char *message, char *whole_message, int msg_type)
{
    int n = sprintf(whole_message, "msg from:%s\n[msg]:%s\n", receiver->ip_addr, message);
	if(!is_sender_blocked_by_receiver(sender, receiver)) {
    	if(receiver->status == ONLINE) {                //Should send data to client only if they or online, else log it 
		    sendall(receiver->fd, whole_message, &n);
    	    receiver->msg_rev++;                //Increment count of receiver received messages
			if(msg_type == SEND_MESSAGE) {
	   			cse4589_print_and_log("[%s:SUCCESS]\n", "RELAYED"); 
				cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", sender->ip_addr, receiver->ip_addr, message);
				cse4589_print_and_log("[%s:END]\n", "RELAYED");
				}
  		    } else {
    	    receiver->log_pending = true;
    	    receiver->log[receiver->log_count++] = whole_message;                       //Do not free memory here or else log file will be in trouble
			if(msg_type == SEND_MESSAGE) {
	       		 cse4589_print_and_log("[%s:SUCCESS]\n", "RELAYED");
	       		 cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", sender->ip_addr, receiver->ip_addr, message);
	       		 cse4589_print_and_log("[%s:END]\n", "RELAYED");
				}
	        }
	}
	sender->msg_sent++;             //Increment  message sender count of sender  
}
/*Display blocked clients list */
void display_blocked_clients(Client *client, list<Client> &client_list)
{
	list<Client>::iterator i;
	char *name = client->name;
	cse4589_print_and_log("[%s:SUCCESS]\n", "BLOCKED");
	int count = 1;
//Traverses the list and finds those ips in the blocked_list of clients that have blocked the client 
	for(i = client_list.begin();i != client_list.end();++i) {
		for(int j=0;j<MAX_CLIENTS;j++) {
			if((*i).blocked_by_list[j] != NULL)
				if(strlen((*i).blocked_by_list[j]) == strlen(name))	
					if(strcmp((*i).blocked_by_list[j], name) == 0) {
						cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", count++, (*i).name, (*i).ip_addr, (*i).port);
			}
		}
	}
	cse4589_print_and_log("[%s:END]\n", "BLOCKED");
} 
/*Check if sender is blocked by sender or not */
bool is_sender_blocked_by_receiver(Client *sender, Client *receiver)
{
	for(int i=0;i<MAX_CLIENTS;i++) {
		if(sender->blocked_by_list[i] != NULL)
			if(strcmp(sender->blocked_by_list[i], receiver->name) == 0)
				return true; 
	}
	return false;
} 
