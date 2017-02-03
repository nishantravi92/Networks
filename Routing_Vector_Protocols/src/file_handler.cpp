#include <stdio.h>
#include <stdlib.h>
#include "../include/router.h"
#include "../include/file_handler.h"
#include "../include/routing_table.h"
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>
#include <iostream>
#include <string.h>
using namespace std;

static vector<File_handler> file_handler_list;
static char penultimate_data_packet[BUFFER_SIZE-8];
static char last_data_packet[BUFFER_SIZE-8]; 

static int get_file_size(FILE *fp);
static File_handler *get_file_handler_for_id(vector<File_handler> & list, int id);		//Not yet implemented 
static int get_fd_for_destination_ip(char *ip_addr, int port);
static void create_file_packet(FILE *fp, char * ip, uint8_t transfer_id,uint8_t  time_to_live, uint8_t end_bit, uint16_t sequence_number, char *send_buffer);
static File_handler *get_file_handler_for_id(vector<File_handler> &, int ); 
static void forward_packet(Neighbour *dest, char *buffer); 
static void display_file_transfer_statistics(File_handler *fh);

void send_file(Router *router,vector<Neighbour> & neighbour_list, char *buffer, char *send_buffer)
{
	uint16_t INFINITY = ~0;
	int i = 6;
	uint16_t payload_size = unpacku16(buffer+i);
	i+=2;
	char dest_ip_addr[INET_ADDRSTRLEN];
	convert_bits_to_ip(buffer+i, dest_ip_addr);
	Neighbour *dest = get_neighbour_for_ip(neighbour_list, dest_ip_addr);
	if(dest == NULL) return;
	if(dest->cost == INFINITY) return;													//Cost to this router is infinity, do not try to send  
	Neighbour *next_hop = get_neighbour_for_id(neighbour_list, dest->next_hop_id);		//If next Hop ID is -1 that means cost to that router is 
	if(next_hop == NULL) return;														//infinity and the packet is dropped
	i+=4;		
	uint8_t time_to_live = (uint8_t)buffer[i++];
	uint8_t transfer_id = (uint8_t)buffer[i++];
	uint16_t sequence_number = unpacku16(buffer+i);
	i+=2;
	int file_name_len = payload_size - 8;
	char file_name[512];
	memcpy(file_name, buffer+i, file_name_len);
	file_name[file_name_len] = '\0';
	int fd = get_fd_for_destination_ip(next_hop->ip_addr, next_hop->data_port);		//Connect to the destination
	if(fd == -1) return; 
	FILE *fp = fopen((const char *)file_name, "r");
	if(fp == NULL) return;
	File_handler file_handler;
	file_handler.transfer_id = transfer_id;
	file_handler.time_to_live = time_to_live;
	file_handler.sequence_number = sequence_number;
	file_handler.fp = fp;
	file_handler.fd = fd;
	int file_size = get_file_size(fp); 
	int n = file_size/N_1024;
	file_handler.packets_to_send = n;
	file_handler.file_to_send = true;	
	memcpy(file_handler.dest_ip_addr,dest_ip_addr, sizeof(dest_ip_addr));
	file_handler_list.push_back(file_handler);	
}
/* Get the file size
 * Logic from http://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c */
int get_file_size(FILE *fp)
{
	fseek(fp, 0L, SEEK_END);
	int n = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	return n;
}

int get_fd_for_destination_ip(char *ip_addr, int port)
{
	struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof( struct addrinfo) );
    put_addrinfo(&hints, AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    char *data_port = convert_int_to_string(port);
    if ((getaddrinfo(ip_addr, data_port, &hints, &servinfo)) != 0)
    {                                                                            //Fill struct servinfo with IP 
        printf("Error creating address structures for data_port\n");
		return -1;
    }
    free(data_port);
    int fd = socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol);
	if( connect(fd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {     //Try to connect
		close(fd);
		return -1; 	
	}	 
	return fd;
}

void create_file_packet(FILE *fp, char * ip, uint8_t transfer_id,uint8_t  time_to_live, uint8_t end_bit, uint16_t sequence_number, char *send_buffer)
{
	pack_ip_address(send_buffer, ip);
	int i=4;
	send_buffer[i++] = (unsigned char)transfer_id;
	send_buffer[i++] = (unsigned char)time_to_live;
	packi16(send_buffer+i, sequence_number);
	i+=2;
	uint8_t fin;
	if(end_bit == 1)
		fin = 0 | 1<<7;
	else fin = 0;  
	send_buffer[i++] = (unsigned char) fin;  		
	send_buffer[i++] = (unsigned char)0;			//Padding
	packi16(send_buffer+i, 0);						//Padding
	i+=2;
	size_t newLen = fread(send_buffer+i, sizeof(char), N_1024, fp);
	return; 			
}

void handle_data_received(Router *router, vector<Neighbour> &neighbour_list, char *buffer, int nbytes)
{   
    char ip_addr[INET_ADDRSTRLEN];
    convert_bits_to_ip(buffer, ip_addr);
	int i = 4; 
    Neighbour *neigh = get_neighbour_for_ip(neighbour_list, ip_addr);
    if(neigh == NULL) return;
	uint8_t id = (uint8_t)buffer[i++];
	uint8_t time_to_live = (uint8_t)buffer[i++];
	time_to_live--;
	if(time_to_live == 0 && neigh->router_id != router->id) return;
	buffer[i-1] = (uint8_t)time_to_live;					//Decrement ttl value  
	uint16_t sequence_number = unpacku16(buffer+i);
	i+=2;
	uint8_t end_bit = (uint8_t)buffer[i++];
	i+=3;
 	File_handler *f_handler_ptr = get_file_handler_for_id(file_handler_list, id);
	if(f_handler_ptr != NULL)
	{
		f_handler_ptr->sequence_numbers.push_back(sequence_number);		
	}
	else
	{
		File_handler f_handler;
		f_handler.file_to_send = false; 
		f_handler.fp = NULL;
		f_handler.transfer_id = id;
		f_handler.time_to_live = time_to_live;
		f_handler.sequence_numbers.push_back(sequence_number);
		file_handler_list.push_back(f_handler);
	}
	File_handler *handler = get_file_handler_for_id(file_handler_list, id);
	if(neigh->router_id == router->id)
	{
		char file_name[10];
		sprintf(file_name, "file-%d", handler->transfer_id);
		FILE *fp;
		if( (fp = fopen(file_name, "a")) != NULL )
		{
			int bytes_wrote = fwrite(buffer+i, sizeof(char), nbytes-N_12,fp);
			fclose(fp);
		}
			
	}
	else 
	{
		forward_packet(neigh, buffer);
		memcpy(penultimate_data_packet, last_data_packet, sizeof (penultimate_data_packet));
		memcpy(last_data_packet, buffer, sizeof(last_data_packet));
	}		 
}

static File_handler *get_file_handler_for_id(vector<File_handler> & file_handler_list, int transfer_id )
{
	vector<File_handler>::iterator i;
	for(i = file_handler_list.begin(); i != file_handler_list.end(); ++i)
	{
		if(i->transfer_id == transfer_id)
			return &(*i);
	}	
	return NULL;
}

void forward_packet(Neighbour *dest, char *buffer)
{
	int fd = get_fd_for_destination_ip(dest->ip_addr, dest->data_port);
	if(fd == -1) return;
	int bytes_to_send = N_1024 + 12;	
	sendall(fd, buffer, &bytes_to_send); 
	close(fd);	
}

int create_last_data_packet(char *buffer)
{
	memcpy(buffer, last_data_packet, sizeof(last_data_packet));
	return N_1024 + N_12;
}

int create_penultimate_data_packet(char *buffer)
{
    memcpy(buffer, penultimate_data_packet, sizeof(penultimate_data_packet));
    return N_1024 + N_12;
}

void display_file_transfer_statistics(File_handler *fh)
{
	cout<<"Transfer ID: "<<fh->transfer_id;
	cout<<"Sequence numbers start with:"<<fh->sequence_numbers[0]<<" Sequence numbers total:"<<fh->sequence_numbers.size();
	cout<<"Transfer TTL:"<<fh->time_to_live<<endl;

}	 


int get_total_bytes_to_send(uint8_t transfer_id)
{
	 int total = 0;
	 File_handler *handle = get_file_handler_for_id(file_handler_list, transfer_id);	
	 if(handle = NULL) return 0;
	 cout<<handle->sequence_numbers.size();
	 int vector_size = handle->sequence_numbers.size();	
	 total += vector_size*2 + 4;
	 return total;
}

bool data_is_to_be_sent()
{
	vector<File_handler>::iterator i;
    for(i = file_handler_list.begin(); i != file_handler_list.end(); ++i)
		if(i->file_to_send) return true;
	return false; 	
}

void send_remaining_packets()
{
	char send_buffer[BUFFER_SIZE-8];
	vector<File_handler>::iterator i;
	 for(i = file_handler_list.begin(); i != file_handler_list.end(); ++i)
	 {	
		if(i->file_to_send)
		{
			int end_bit;
			if(i->packets_to_send == 1)			
					end_bit = 0;
				else end_bit = 1;
			create_file_packet(i->fp, i->dest_ip_addr, i->transfer_id, i->time_to_live, end_bit, i->sequence_number, send_buffer);
			i->sequence_numbers.push_back(i->sequence_number);
			i->sequence_number++;
			memcpy(penultimate_data_packet, last_data_packet, sizeof (penultimate_data_packet));
			memcpy(last_data_packet, send_buffer, sizeof (last_data_packet));  			
			int bytes_to_send = N_1024 + N_12;
			sendall(i->fd, send_buffer, &bytes_to_send); 
			if(i->packets_to_send == 1)
			{
				i->packets_to_send--;
				i->file_to_send = false;
				close(i->fd);	
				fclose(i->fp);	
			}
			else i->packets_to_send--; 			
		}		
	 }
}

int create_stats(char *buffer, uint8_t transfer_id)
{
   File_handler *handle = get_file_handler_for_id(file_handler_list, transfer_id);
   if(handle == NULL) return 0;
   display_file_transfer_statistics(handle);
   int i = 0;
   packi8(buffer+i, handle->transfer_id);
   i++;
   packi8(buffer+i, handle->time_to_live);
   i++;
   packi16(buffer+i, 0);                           //Padding
   i+=2;
   for(int j=0;j<handle->sequence_numbers.size();++j)
   {
       packi16(buffer+i, handle->sequence_numbers[j]);
       i+=2;
   }
   return i;
}
