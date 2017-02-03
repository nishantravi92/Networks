#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>
#include "../include/router.h"
using namespace std;

int latest_fd_value;

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
		}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

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

/* Logic for getting host IP address from http://jhshi.me/2013/11/02/how-to-get-hosts-ip-address*/
int get_host_ip_address(char *ip_addr)
{
    const char *target_name = "8.8.8.8";
    const char *target_port = "53";
    int ret = 0;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct addrinfo hints;
    struct addrinfo* info;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if ((ret = getaddrinfo(target_name, target_port, &hints, &info)) != 0)  return -1;
    if (connect(sock, info->ai_addr, info->ai_addrlen) < 0) return -1;
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(sock, (struct sockaddr*)&local_addr, &addr_len) < 0) 
	{
        close(sock);
        return -1;
    }
    if (inet_ntop(local_addr.sin_family, &(local_addr.sin_addr), ip_addr, INET_ADDRSTRLEN) == NULL)
        return -1;
    return 0;
}

/*Initializes the master and temp read and write fd data sets
 *  * Also adds STDIN and STDOUT to these sets */ 
void init_file_fd_data(fd_set *read_master,fd_set *read_fds)
{
 	FD_ZERO(read_master);           // clear the master and temp sets
    FD_ZERO(read_fds);
    FD_SET(STDIN_FILENO, read_master);
}

/* DOes basic initialization of structures used for networking like addrinfo
 *  * Also gets the host IP address which is needed.
 *   * Parts have been taken from Beej for connection part */ 
void init_networking_structures(struct addrinfo *hints, struct addrinfo **servinfo, char *port)
{
	int rv;	
	memset(hints, 0, sizeof (struct addrinfo));								//Set all fields of addrinfo to zero
	put_addrinfo(hints, AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP);    //Fill addrinfo struct fields
    if ((rv = getaddrinfo(NULL, port, hints, servinfo)) != 0) {          //Fill struct servinfo with data for listening for any IP 
           fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }
}

/*Put the data into the addrinfo structure for future use */
void put_addrinfo(struct addrinfo *addr_info,int flags, int family, int socktype, int protocol )
{
	addr_info->ai_family = family;
	addr_info->ai_flags = flags;
	addr_info->ai_protocol = protocol;
	addr_info->ai_socktype = socktype;
}

char *convert_int_to_string(int port) 
{
	int n = 0;
	int port1 = port;
	while(port1 > 0)
	{
		 port1 /=10;
		 n++;			
	}
	n++;
	char *port_string = (char *)malloc(sizeof(char)*n);
	snprintf(port_string, n, "%d", port );
	return port_string;
}

enum Command get_command_from_controller(int command)
{
	if(command == 0) return AUTHOR ;
	if(command == 1) return INIT;
	if(command == 2) return ROUTING_TABLE;
	if(command == 3) return UPDATE;
	if(command == 4) return CRASH;
	if(command == 5) return SENDFILE;
	if(command == 6) return SENDFILE_STATS;
	if(command == 7) return LAST_DATA_PACKET;
	if(command == 8) return PENULTIMATE_DATA_PACKET;
	return ERROR_COMMAND;
}

void convert_bits_to_ip(char *recv_buffer,char *ip_addr)
{
	int part1 = (uint8_t)recv_buffer[0]; 
	int part2 = (uint8_t)recv_buffer[1];    
	int part3 = (uint8_t)recv_buffer[2];    
	int part4 = (uint8_t)recv_buffer[3];    			
	sprintf(ip_addr, "%d.%d.%d.%d", part1, part2, part3, part4);
}

/*
 * ** unpacku16() -- unpack a 16-bit unsigned from a char buffer (like ntohs())
 * This function has been Copied from Beej
 * */ 
unsigned int unpacku16(char *buf)
{
    return ((uint8_t)buf[0]<<BYTE_SIZE) | (uint8_t)buf[1];
}

/*
 * ** packi16() -- store a 16-bit int into a char buffer (like htons())
 * */ 
void packi16(char *buf, int i)
{
    *buf++ = i>>8; *buf++ = i;
}

void packi8(char *buf, int i)
{
	*buf = (unsigned char)i; 
}	


void pack_ip_address(char *buffer, char *ip_addr)
{
	 char copy_of_ip_addr[INET_ADDRSTRLEN];
	 memcpy(copy_of_ip_addr, ip_addr, INET_ADDRSTRLEN);	
	 char *save_ptr, *token;
     token =  strtok_r(copy_of_ip_addr, ".", &save_ptr);
     uint8_t ip_part1 = atoi(token);
     token =  strtok_r(NULL, ".", &save_ptr);
     uint8_t ip_part2 = atoi(token);
     token =  strtok_r(NULL, ".", &save_ptr);
     uint8_t ip_part3 = atoi(token);
     token =  strtok_r(NULL, ".", &save_ptr);
	 int ip_part4 = atoi(token);
 	 buffer[0] = (unsigned char)ip_part1;
     buffer[1] = (unsigned char)ip_part2;
     buffer[2] = (unsigned char)ip_part3;
     buffer[3] = (unsigned char)ip_part4;
}

uint32_t convert_ip_to_int(char *buffer)
{
	uint8_t part1 = (unsigned char)buffer[0];
	uint8_t part2 = (unsigned char)buffer[1];
	uint8_t part3 = (unsigned char)buffer[2];
	uint8_t part4 = (unsigned char)buffer[3];
	uint32_t ip = part1 <<24 | part2 << 16 | part3 << 8 | part4;
	return ip;
}

bool fd_is_controller_fd(Router *router, int fd)
{
	for(int i=0;i<router->controller_fd_list.size();++i)
	{
		if(router->controller_fd_list[i] == fd) return true;
	}
	return false;
}

bool condition(int i)
{
	return i == latest_fd_value;
}

void remove_from_router_fd_list(Router *router, int fd)
{
	latest_fd_value = fd;	
	router->controller_fd_list.erase(remove_if(router->controller_fd_list.begin(), router->controller_fd_list.end(), condition), router->controller_fd_list.end()); 
}

/*Copied from http://stackoverflow.com/questions/1468596/calculating-elapsed-time-in-a-c-program-in-milliseconds */
int timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1)
{
    long int diff = (t2->tv_usec + 1000000 * t2->tv_sec) - (t1->tv_usec + 1000000 * t1->tv_sec);
    result->tv_sec = diff / 1000000;
    result->tv_usec = diff % 1000000;
    return (diff<0);
}

void timeval_print(struct timeval *tv)
{
    char buffer[30];
    time_t curtime;

    printf("%ld.%06ld", tv->tv_sec, tv->tv_usec);
    curtime = tv->tv_sec;
    strftime(buffer, 30, "%m-%d-%Y  %T", localtime(&curtime));
    printf(" = %s.%06ld\n", buffer, tv->tv_usec);
}

