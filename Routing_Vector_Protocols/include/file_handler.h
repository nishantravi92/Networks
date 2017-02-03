#ifndef FILE_HANDLER_H

#define FILE_HANDLER_H
#define N_1024 1024
#define N_12 12
#include <vector>
#include "router.h"

struct File_handler {
	uint8_t transfer_id;
	std::vector<int> sequence_numbers;
	uint16_t sequence_number;
	int packets_to_send;
	uint8_t time_to_live;	
	int fd;
	char file_name[512];	
	char dest_ip_addr[INET_ADDRSTRLEN];
	FILE *fp;
	bool file_to_send;
};


void send_file(Router *router,std::vector<Neighbour> & neighbour_list, char *buffer, char *send_buffer);
void handle_data_received(Router *router, std::vector<Neighbour> &neighbour_list, char *buffer, int nbytes);
int create_stats(char *, uint8_t);	
int create_last_data_packet(char *);
int create_penultimate_data_packet(char *);
int get_total_bytes_to_send(uint8_t transfer_id);
bool data_is_to_be_sent();
void send_remaining_packets();

#endif 
