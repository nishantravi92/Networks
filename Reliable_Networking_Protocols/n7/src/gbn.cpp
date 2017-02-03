#include "../include/simulator.h"
#include "../include/abt.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional data transfer 
   protocols (from A to B). Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
#define BUFFER_SIZE 1000

using namespace std;

vector<struct pkt> buffer;
vector<struct pkt> window;
int baseA;
int baseB;
int next_seq_num;
int window_size;
float TIMER_DELAY = 25.0;

void make_a_packet(struct pkt *packet, struct msg *message, int call);
bool condition(struct pkt); 
bool check_for_any_error(struct pkt *packet);
/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message)
{
	struct pkt packet;
    make_a_packet(&packet, &message, next_seq_num);
	printf("A_output called, Sequence number is:%d", next_seq_num); 
	if(next_seq_num < baseA+window_size)
	{
		window.push_back(packet);
		tolayer3(A_SIDE, packet);
		if(next_seq_num == baseA)
			starttimer(A_SIDE, TIMER_DELAY);
		next_seq_num++;
	}	
	else
	{
		if(buffer.size() >= BUFFER_SIZE)
		{
			printf("Buffer count exceeded. Program shutting down.................\n");
			exit(0);
		}
		buffer.push_back(packet);	
		next_seq_num++;
	}		
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
	printf("A_input called: baseA:%d, Next seqnum:%d, packet sequence number:%d \n", baseA, next_seq_num, packet.seqnum);
	if(check_for_any_error(&packet))
	{
		if(packet.seqnum >= baseA) {				//If there is a packet which has a higher seqnum than base then update the base
			baseA = packet.seqnum+1;
			window.erase(remove_if(window.begin(), window.end(), condition), window.end());		//Remove those elements who have seqnum less than new base now 
			while(window.size() < window_size && buffer.size() > 0)
 		    {
 		        window.push_back(buffer[0]);                            //Remove messages from buffer and put it into the window till window does not become full
				tolayer3(A_SIDE,buffer[0]);
 		        buffer.erase(buffer.begin());
 		    }
			if(baseA == next_seq_num)															//If base==next_seq_num then no new packets have arrived from
				stoptimer(A_SIDE);																//layer5 stop the timer.
			else
			{
				stoptimer(A_SIDE);									//Stop timer
				starttimer(A_SIDE, TIMER_DELAY);					//Start timer again so that we can send data again after timer interrupt
			}
		}
	}
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
	while(window.size() < window_size && buffer.size() > 0)
	{
		window.push_back(buffer[0]);							//Remove messages from buffer and put it into the window till window does not become full
		buffer.erase(buffer.begin());
	}
	printf("Timer Interrupt:Resending packets with window size currently being:%d\n", window.size());
	starttimer(A_SIDE, TIMER_DELAY);                            //Start the timer again
	for(int i=0;i<window.size();i++) 
	{
		printf("Sequence Number:%d, Message: %.20s \n", window[i].seqnum, window[i].payload);	//Send all packets to layer3 which are in the window
		tolayer3(A_SIDE, window[i]);
	}
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
	window_size = getwinsize();
	baseA = 1;
	next_seq_num = 1;	
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
	if(check_for_any_error(&packet))						//Check if any error is there in the packet
	{
		printf("B_input called, No error found");
		if(baseB == packet.seqnum)							//If the sequence number is what we are expecting then send it to layer5, send ACK to layer 3 
		{													//and update the base number to the new value that we want now
			tolayer5(B_SIDE,packet.payload);
			tolayer3(B_SIDE,packet);
			baseB++;		
		} else 
		{
	 		struct msg mess;
			memcpy(mess.data, packet.payload, sizeof(packet.payload));
			make_a_packet(&packet, &mess, baseB-1);
			tolayer3(B_SIDE, packet);  
		}
	} else
	 {	
		 struct msg mess;
		 memcpy(mess.data, packet.payload, sizeof(packet.payload));
		 make_a_packet(&packet, &mess, baseB-1);	
    	 tolayer3(B_SIDE, packet);								//Send the previous ack that we have received till now
	 }
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
	baseB = 1;
}

void make_a_packet(struct pkt *packet, struct msg *message, int call)
{
   packet->seqnum = call;
   packet->acknum = call;
   int character_sum = 0;
   for(int i=0;i<PACKET_SIZE;++i)
       character_sum+= message->data[i];
   packet->checksum = call+call+character_sum;
   packet->checksum = ~(packet->checksum);                   //Checksum done here
   memset(packet->payload, '\0', sizeof(packet->payload) );
   memcpy(packet->payload, message->data, sizeof(message->data));
}

/*Packet condition to check if packet sequence number is less than base number */  
bool condition(struct pkt packet)
{
	return packet.seqnum < baseA;	
}

// Check if there is any error in the packet
bool check_for_any_error(struct pkt *packet)
{
	int sum = 0;
    int check_value = ~0;
    int char_sum = 0;
    for(int i=0;i<PACKET_SIZE;++i)
	    char_sum+= packet->payload[i];
    sum+= packet->seqnum + packet->acknum + char_sum + packet->checksum;
    if( sum == check_value) return true;
	    return false;
}

