/**
 * @n7_assignment3
 * @author  Nishant Ravichandran <n7@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include "../include/n7_assignment3.h"
#include "../include/router.h"
#include "../include/routing_table.h"
#include "../include/file_handler.h"

using namespace std;

static int run_router_application(char *c_port);
void perform_router_functions(Router *router, fd_set read_master, fd_set read_fds);
static void process_controller_commands(Router *, char *, int, int *, fd_set *);
static int create_header(char *, int, int, char *);
static void create_routing_table(Router *, char *, char *, int);
static void enter_values_into_matrix(Router *);
static int get_control_fd(Router *router);
static void create_ports_and_update_fdset(Router *router, int *fdmax,fd_set *read_master, int port, int sock_type, int proto_type);
static int create_packet(Router *, char * send_buffer);
static void send_routing_table_to_controller(char *, vector<Neighbour> &neighbour_list, int fd);
static void update_timeouts_of_neighbours(Router *router, vector<Neighbour> & neighbour_list);
static void dummy();


bool router_is_intialized;
vector<Neighbour> neighbour_list;
struct timeval time_holder;
int TIMEOUT_VALUE = 1;
int TIMEOUT_INTERVAL = 3;
static uint16_t INFINITY = ~0;
struct timeval now;

int time_passed = 0;

int main(int argc, char **argv)
{
	/*Start Here*/
	if(argc != 2)
		printf("Wrong parameters entered. Usage is -- Control Port number --\n");
	else
		run_router_application(argv[1]);		
	return 0;
}
/* Runs the router application */
int run_router_application(char *c_port)
{
	Router router;
	router_is_intialized = false;
	router.control_port = atoi(c_port);
	if( get_host_ip_address(router.ip_addr) != 0)		//If some error occurred during obtaining host IP, output error
		cout<<"Error getting host address"<<endl;
	 fflush(stdout);	
	 fd_set read_master;
     fd_set read_fds;
     init_file_fd_data(&read_master,&read_fds);		//Add input and output streams to the sets and initialize them
	 perform_router_functions(&router, read_master, read_fds);
  	 return 0;
}
/*Perform main router operations like setting up structures needed for connection, sending data and receiving, sending data and sending distance
 *vectors to other routers
 *Note:Write_master is not actually used
 *Logic for socket programming from Beej */ 
void perform_router_functions(Router *router, fd_set read_master, fd_set read_fds)
{
	int fdmax;
	char buffer[BUFFER_SIZE];
	char data_buffer[BUFFER_SIZE-8];									//1036 size fixed for receiving data packets
	char update_buffer[BUFFER_SIZE];
	int yes = 1;
	int control_recv_fd = -1;
	int data_recv_fd;
	struct sockaddr_storage remoteaddr; 								// client address Testing
	socklen_t addrlen = sizeof remoteaddr;
	int nbytes;
	router->data_port_fd = -1;
	router->router_port_fd = -1;	
	int control_fd = get_control_fd(router);
	FD_SET(control_fd, &read_master);
	fdmax = control_fd;	
	time_holder.tv_sec = 0;
	time_holder.tv_usec = 10000;
	int timeout;
	gettimeofday(&router->send_update_time, NULL);
	//timeval_print(&now);
	for(;;)
	{
		read_fds = read_master;							//Has to be done as select changes the fds set
		timeout = select(fdmax+1, &read_fds, NULL, NULL,&time_holder);		//Make Timeout here
		if(time_to_send_updates_has_expired(router))
		{
			send_updates_to_neighbours(router);
			cout<<"Update Sent"<<endl;
			fflush(stdout);		
		}
		update_timeouts_of_neighbours(router, neighbour_list);
		dummy();
		//Send packets if there are packets to be sent
		if(data_is_to_be_sent())
		{
			send_remaining_packets();
		}
		time_holder.tv_usec = 10000;
			for(int i=0;i<=fdmax;i++) 
			{
				//Look for incoming connections on control port
				if(FD_ISSET(i, &read_fds))
				{
					if(i == control_fd)
					{
						control_recv_fd = accept(control_fd, (struct sockaddr *)&remoteaddr, &addrlen);
						if (control_recv_fd  == -1) perror("accept");
						else
						{
							router->controller_fd_list.push_back(control_recv_fd);
							FD_SET(control_recv_fd , &read_master);									 // add to master set
                    		if (control_recv_fd  > fdmax)    										 // keep track of the max fd number till now
                    		    fdmax = control_recv_fd;
						    inet_ntop(AF_INET, get_in_addr((struct sockaddr*)&remoteaddr), router->controller_ip_addr, INET_ADDRSTRLEN);	//Put Controller IP	
						}
					}
					//Look for data in the data port
					else if(i == router->data_port_fd)
					{
						int data_fd = accept(router->data_port_fd, (struct sockaddr *)&remoteaddr, &addrlen);	
						if(data_fd == -1) 
						{
							cout<<"Error Connecting"<<endl;
							continue;
						}	
						FD_SET(data_fd, &read_master);
						if(data_fd > fdmax)
							fdmax = data_fd;
					}
					//Look for data in the router port(UDP) for incoming updates from other routers	 
					else if(i == router->router_port_fd)
					{
						nbytes =  recvfrom(router->router_port_fd, update_buffer, sizeof update_buffer, 0,(struct sockaddr*)&remoteaddr, &addrlen); 
						if(nbytes > 0)
						{
							update_routing_table_with_new_values(router, neighbour_list,  update_buffer, nbytes);
							//display_routing_table(router);
							struct timeval now;
							gettimeofday(&now, NULL);	
							timeval_print(&now);
							find_shortest_paths(router, neighbour_list);
							display_neighbour_list(neighbour_list);
						}
					}	//Processing controller commands 
					else if(fd_is_controller_fd(router, i))
						{
                      		 if ((nbytes = recv(i, buffer, sizeof buffer, 0)) <= 0)
                       		 {  
                        	    close(i); // bye!
                        	    FD_CLR(i, &read_master);
								remove_from_router_fd_list(router, i);
								control_recv_fd = -1;	
                        	    //printf("\nController Connection closed\n");
                        	    //fflush(stdout);
                        	 }  
                        	 else
                        	 {  
                        	    process_controller_commands(router,buffer, i, &fdmax,  &read_master);
                        	 }
						}
					else
					{		//Data being received from routers
						if ((nbytes = recv(i, data_buffer, sizeof data_buffer, 0)) <= 0)
						{
							 close(i); // bye!
                             FD_CLR(i, &read_master);
							 //printf("\nData Port Connection closed\n");
							 //fflush(stdout);							
						}
						else
						{
							handle_data_received(router, neighbour_list, data_buffer, nbytes);
						}
					}
				}
			}
	}		//For Loop ends here
}

/*Process controller commands which are received from the control port.
 *Sends response messages back to it  */
static void process_controller_commands(Router *router, char *buffer, int fd, int *fdmax,  fd_set *read_master)
{
	enum Command command = get_command_from_controller((int)buffer[4]);
	char send_buffer[BUFFER_SIZE];
	switch(command)
	{
		case AUTHOR:
		{
			const char *author = "I, n7, have read and understood the course academic integrity policy.";
			int n = create_header(router->controller_ip_addr, AUTHOR, R_SUCCESS, send_buffer); 
			packi16(send_buffer+n, strlen(author));						//Payload Value
			n+=2;
			memcpy(send_buffer+n, author, strlen(author));
			n+=strlen(author);
			sendall(fd, send_buffer, &n); 
		}
			break;
		case INIT:
			create_routing_table(router, buffer, send_buffer, fd);
			create_ports_and_update_fdset(router, fdmax, read_master, router->data_port, SOCK_STREAM, IPPROTO_TCP); //Create the data port
            create_ports_and_update_fdset(router, fdmax, read_master, router->router_port, SOCK_DGRAM, IPPROTO_UDP); //Create the router port for updates
			find_shortest_paths(router, neighbour_list);
			send_updates_to_neighbours(router);		
			display_routing_table(router);
			gettimeofday(&router->send_update_time, NULL);
			timeval_print(&router->send_update_time); 
			break;
		case ROUTING_TABLE:
			send_routing_table_to_controller(router->controller_ip_addr, neighbour_list, fd);
			break;
		case UPDATE:
		{
			update_cost_links(buffer, router, neighbour_list);
			int n = create_header(router->controller_ip_addr, UPDATE, R_SUCCESS, send_buffer);
			packi16(send_buffer+n, 0);									//Payload Value
			n+=2;
			sendall(fd, send_buffer, &n);
			find_shortest_paths(router, neighbour_list);
			display_neighbour_list(neighbour_list);
		}
			break;
		case CRASH:
		{
			int n = create_header(router->controller_ip_addr, CRASH, R_SUCCESS, send_buffer);
			packi16(send_buffer+n, 0);							//Payload value
			n+=2;	
			sendall(fd, send_buffer,&n);
			cout<<"\n..............Router Exiting...............\n";
			exit(0);
		}
			break;
		case SENDFILE:
		{		
			send_file(router, neighbour_list, buffer, send_buffer);
			int n = create_header(router->controller_ip_addr, SENDFILE, R_SUCCESS, send_buffer);
			packi16(send_buffer+n, 0);							//Payload value
			n+=2;
			sendall(fd, send_buffer, &n);	
		}
			break;
		case SENDFILE_STATS:
		{	
			uint8_t transfer_id = (uint8_t)buffer[8]; 
			int total_bytes_to_send = get_total_bytes_to_send(transfer_id);
			char special_send_buffer[total_bytes_to_send+8];
			int n = create_header(router->controller_ip_addr, SENDFILE_STATS, R_SUCCESS, special_send_buffer);
			int i = create_stats(special_send_buffer, transfer_id);
		 	//int i = create_transfer_id__stats(special_send_buffer+n+2,transfer_id);				//Plus 2 to leave out space for payload value
			//packi16(send_buffer+n, i);													//Payload Value
			//n +=2+i;
			//sendall(fd, special_send_buffer, &n);	
		}
			break;
		case LAST_DATA_PACKET:
		{
			int n = create_header(router->controller_ip_addr, LAST_DATA_PACKET, R_SUCCESS, send_buffer);
			int i = create_last_data_packet(send_buffer+n+2);
			packi16(send_buffer+n, i);													//Plus 2 to leave out space for payload value  
			n += 2+i;
			sendall(fd, send_buffer, &n);								
		}
			break;
		case PENULTIMATE_DATA_PACKET:
		{	
			int n = create_header(router->controller_ip_addr, PENULTIMATE_DATA_PACKET, R_SUCCESS, send_buffer);
			int i = create_penultimate_data_packet(send_buffer+n+2);
			packi16(send_buffer+n, i);
			n += 2+i;																			//Payload Value
			sendall(fd, send_buffer, &n);
			break;
		}
	}
}
/* Creates header to be sent to the controller
 * Returns the next position of the buffer.
 * Puts the IP of controller, controller code and response code  */
int create_header(char *controller_ip, int comm_code, int response_code,  char *send_buffer)
{
	pack_ip_address(send_buffer, controller_ip);
	send_buffer[4] = (unsigned char)comm_code;
	send_buffer[5] = (unsigned char)response_code;	 
	return 6;
}
/* Creates the routing table from the INIT message and then sends a response message back
 * Also sets up the neighbour list to keep track of data about neighbours */  
void create_routing_table(Router *router, char *recv_buffer, char *send_buffer, int fd)
{
	int num_of_routers = unpacku16(recv_buffer+8);
	int interval = unpacku16(recv_buffer+10); 
	TIMEOUT_INTERVAL = interval;											//Set up the time interval
	int i = 12;
	while(num_of_routers > 0)
	{
		Neighbour neighbour;
		neighbour.router_id   = unpacku16(recv_buffer+i);
		neighbour.router_port = unpacku16(recv_buffer+i+2);
		neighbour.data_port   = unpacku16(recv_buffer+i+4);
		neighbour.cost        =	unpacku16(recv_buffer+i+6);			
		neighbour.first_update_message_received = false;
		if(neighbour.cost != INFINITY && neighbour.cost != 0)					//Check if not self or neighbour which is not connected
		{
			neighbour.is_neighbour = true;
			neighbour.status = ONLINE;
		}
		else 
		{
			neighbour.is_neighbour = false;
			neighbour.next_hop_id = neighbour.router_id;
		}
		convert_bits_to_ip(recv_buffer+i+8, neighbour.ip_addr);  					//Put the IP address string into ip_addr of neighbour	
		num_of_routers--;
		i+=12;
		neighbour_list.push_back(neighbour);
	}	
	int n = create_header(router->controller_ip_addr, INIT, R_SUCCESS,  send_buffer);	
	packi16(send_buffer+n,0);														//Payload size is 0
	n+=2;
	sendall(fd, send_buffer, &n); 	
	enter_values_into_matrix(router);
}
/*Updates the routing table and adds corresponding costs to the matrix */
void enter_values_into_matrix(Router *router)
{
	vector<Neighbour>::iterator i, j;  
	initilize_values_into_routing_table(router);
	for( i=neighbour_list.begin();i!= neighbour_list.end();++i )
	{
		if(i->cost == 0)														//Selct the host matrix and set its matrix id to 0
		{
			i->matrix_id = 0;
			router->router_port = i->router_port;							//Enter router and data port values
			router->data_port = i->data_port;
			router->id = i->router_id;
			break;
		}
	}
	if( i == neighbour_list.end()) return;
	int count = 1;
	for(j = neighbour_list.begin();j!= neighbour_list.end();++j)
	{
		if( i != j )
		{
			j->matrix_id = count++;												//Map the matrix and the neighbour
			router->routing_table[i->matrix_id][j->matrix_id] = j->cost;		//Make an entry in the table	
		}	 			
	}
}

/*Get the router control port fd to receive connections from the controller */
int get_control_fd(Router *router)
{
	int yes = 1;
	struct addrinfo hints, *servinfo;	
	char *control_port = convert_int_to_string(router->control_port);
    init_networking_structures(&hints, &servinfo, control_port);        //Get IP address as well as set up basic structures for use
    free(control_port);
    int control_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if(control_fd == -1) return -1;
    setsockopt(control_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));   //Multiple connections for same socket option yes
    if (bind(control_fd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {   //Bind the socket, if something is wrong return ERROR
        close(control_fd);
        return -1;
    }   
    if (listen(control_fd, MAX_ROUTERS*MAX_ROUTERS) == -1) {                  //Start listening on this socket for connections
        perror("listen");
		return -1; 
   }   
	return control_fd;
}
/*Sends the cost of the Router's neighbours to all other neighbours */
void send_updates_to_neighbours(Router *router)
{
	vector<Neighbour>::iterator i;
	char send_buffer[BUFFER_SIZE];
	int bytes_to_send = create_packet(router, send_buffer);
	for(i = neighbour_list.begin();i!= neighbour_list.end();++i)
	{
		if(i->cost != 0 && i->is_neighbour )			//Check if member is neighbour and not self also or not
		{
			struct addrinfo hints, *servinfo;	
			memset(&hints, 0, sizeof (struct addrinfo)); 	
			put_addrinfo(&hints, AI_PASSIVE, AF_INET, SOCK_DGRAM, IPPROTO_UDP);    //Fill addrinfo struct fields	
			char *port = convert_int_to_string(i->router_port);	
			if ((getaddrinfo(i->ip_addr, port, &hints, &servinfo)) != 0) 
			{         																	 //Fill struct servinfo with IP 
	 			printf("Error creating address structures\n"); 
				fflush(stdout); 
  			}
			free(port);
			int fd = socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol);
			sendto(fd, send_buffer, sizeof(send_buffer), 0, servinfo->ai_addr, servinfo->ai_addrlen); 
			close(fd);
		}
	}
}
/*Create UDP Router ports and TCP data port numbers and puts them into the master fd set.
 *Updates fdmax as well */
void create_ports_and_update_fdset(Router *router, int *fdmax,fd_set *read_master, int port, int sock_type, int proto_type)
{
	struct addrinfo hints, *servinfo;
	memset(&hints, 0, sizeof( struct addrinfo) );
	put_addrinfo(&hints, AI_PASSIVE, AF_INET, sock_type, proto_type);				
	char *data_port = convert_int_to_string(port);
	if ((getaddrinfo(NULL, data_port, &hints, &servinfo)) != 0)
    {                                                                            //Fill struct servinfo with IP 
	    printf("Error creating address structures for data_port\n");
    }
	free(data_port);	
	int fd = socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol);
	if(sock_type == SOCK_DGRAM)
		router->router_port_fd = fd;
	else
		router->data_port_fd = fd; 
	if(*fdmax < fd)
		*fdmax = fd; 	 
	FD_SET(fd, read_master);
	if (bind(fd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) 
	{   																	//Bind the socket, if something is wrong return ERROR
	     close(fd);
	     return;
     }
	 if(proto_type == IPPROTO_TCP)
	 {	
    	 if (listen(fd, MAX_ROUTERS*MAX_ROUTERS) == -1) 
		 {                  													//Start listening on this socket if a TCP socket for connections
		     perror("listen");
    	     return;
    	 }
	}
}

/* Create a packet with the data of the cost paths to other neighbours. */
int create_packet(Router *router, char *send_buffer)
{
	int i = 0;
	packi16(send_buffer, neighbour_list.size());
	i+=2;
	packi16(send_buffer+i, router->router_port);
	i+=2;
	pack_ip_address(send_buffer+i, router->ip_addr);
	//Pack the router IP address
	i+=4;
	vector<Neighbour>::iterator j;
	for( j = neighbour_list.begin(); j != neighbour_list.end();j++ )		
	{
			pack_ip_address(send_buffer+i, j->ip_addr);
			packi16(send_buffer+i+4, j->router_port);					
			packi16(send_buffer+i+6, 0);						//PADDING
			packi16(send_buffer+i+8, j->router_id);
			packi16(send_buffer+i+10, j->cost);
			i+=12;
	}
	return i;
}

void send_routing_table_to_controller(char *controller_ip_addr, vector<Neighbour> &neighbour_list, int fd)
{
	uint16_t INFINITY = ~0;	
	char send_buffer[BUFFER_SIZE];
	int i = create_header(controller_ip_addr, ROUTING_TABLE, R_SUCCESS, send_buffer);
	int payload_position = i;
	i+=2;
	vector<Neighbour>::iterator neigh;
	for(neigh = neighbour_list.begin(); neigh!= neighbour_list.end(); ++neigh)
	{
		packi16(send_buffer+i, neigh->router_id);		//Pack Router X ID
		i+=2;
		packi16(send_buffer+i, 0);					//PADDING
		i+=2;
		if(neigh->cost < INFINITY)
			packi16(send_buffer+i, neigh->next_hop_id);	//Pack Router X Next Hop ID 
		else
			packi16(send_buffer+i, INFINITY);		     //Infinity if next hop does not exist
		i+=2;
		packi16(send_buffer+i, neigh->cost);			//Cost to the router X
		i+=2;   		
	}
	packi16(send_buffer+payload_position, i - payload_position -2 );                  //Total size of data to be sent
	sendall(fd, send_buffer, &i);													  //-2 because we also consider payload in the total	
}

void dummy(void)
{
}

void update_timeouts_of_neighbours(Router *router, vector<Neighbour> &neighbour_list)
{
	vector<Neighbour>::iterator i;
	struct timeval result;
    gettimeofday(&now, NULL);
	bool router_has_gone_offline = false;
	for( i = neighbour_list.begin(); i!= neighbour_list.end(); ++i)
	{
		if(i->cost != 0)					//I do not want to set myself as offline!!!
		{
			if(i->first_update_message_received && i->status == ONLINE)
			{
				timeval_subtract(&result, &now, &i->time_to_receive_update);
				if( result.tv_sec+result.tv_usec/1000000 >= TIMEOUT_INTERVAL*NUM_OF_INTERVALS) 
				{
					timeval_print(&now);
					router->routing_table[0][i->matrix_id] = ~0;
					i->status = OFFLINE; 
					router_has_gone_offline = true;
				} 
			}
		}
	}
	if(router_has_gone_offline)
	{
		find_shortest_paths(router, neighbour_list);
		cout<<"Router has gone offline"<<endl;
		display_neighbour_list(neighbour_list);		
	}
}

bool time_to_send_updates_has_expired(Router *router)
{
	struct timeval result;
	gettimeofday(&now, NULL);
	timeval_subtract(&result, &now, &router->send_update_time);		
	if(result.tv_sec+result.tv_usec/1000000 >= TIMEOUT_INTERVAL)
	{
		//timeval_print(&now);
		gettimeofday(&router->send_update_time, NULL);
		return true;
	}
	return false;
}
