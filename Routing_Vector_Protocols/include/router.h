
#ifndef ROUTER_H
#define ROUTER_H

#include <netinet/in.h>
#include <vector>
#include <sys/time.h>

#define MAX_ROUTERS 5
#define BUFFER_SIZE 1044
#define BYTE_SIZE 8
#define ERROR -1
#define R_SUCCESS 0
#define NUM_OF_INTERVALS 3

enum Command
{
	AUTHOR,
	INIT,
	ROUTING_TABLE,
	UPDATE,
	CRASH,
	SENDFILE,	
	SENDFILE_STATS,
	LAST_DATA_PACKET,
	PENULTIMATE_DATA_PACKET,
	ERROR_COMMAND
};

enum STATUS {
	OFFLINE,
	ONLINE
}; 
struct Router {
	uint16_t id;
	unsigned int control_port;
	unsigned int router_port;
	unsigned int router_port_fd;
	uint16_t data_port;
	std::vector<int> controller_fd_list;
	unsigned int data_port_fd;
	char ip_addr[INET_ADDRSTRLEN];
	char controller_ip_addr[INET_ADDRSTRLEN];
	uint16_t routing_table[MAX_ROUTERS][MAX_ROUTERS];
	struct timeval send_update_time;
};

struct Neighbour {
	int router_id;
	struct timeval time_to_receive_update;
	bool first_update_message_received;
	enum STATUS status;
	int next_hop_id;
	int matrix_id;
	int router_port;
	int data_port;
	uint16_t cost;
	char ip_addr[INET_ADDRSTRLEN];
	bool is_neighbour;
	struct timeval update_received;
};

enum Command get_command_from_controller(int);
int get_host_ip_address(char *ip_addr);
void init_file_fd_data(fd_set *read_master,fd_set *read_fds);
void init_networking_structures(struct addrinfo *hints, struct addrinfo **servinfo, char *port);
char *convert_int_to_string(int);
void *get_in_addr(struct sockaddr *sa);
int sendall(int s, char *buf, int *len);
void convert_bits_to_ip(char *, char *);
unsigned int unpacku16(char *buf);
void put_addrinfo(struct addrinfo *addr_info, int flags, int family, int socktype, int protocol);
void packi16(char *buf, int i);
void packi8(char *buf, int i);
void pack_ip_address(char *buffer, char *ip_addr);
void pack_ip_address_into_buffer(char send_buffer, uint32_t ip);
bool fd_is_controller_fd(Router *router, int fd);
void remove_from_router_fd_list(Router *router, int i);
uint32_t convert_ip_to_int(char *buffer);
int timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1);
void timeval_print(struct timeval *tv);


#endif
