#include "../include/simulator.h"
#include "../include/abt.h"
#include "../include/sr.h"
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <float.h>
using namespace std;
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

vector<struct pkt> bufferA;
vector<struct pkt> window;
vector<struct pkt> bufferB;
vector<struct time_data> time_windowA;

int baseA;
int baseB;
int next_seq_num;
int window_size;
float TIMER_DELAY = 25.0;

void make_my_packet(struct pkt *packet, struct msg *message, int call);
bool check_for_my_error(struct pkt *packet);
bool my_condition_for_A(struct pkt packet);
bool already_exists_in_buffer(int seq_num);
void deliver_ack_packets_to_layer5(int *);
bool comparison(struct pkt, struct pkt);
bool my_condition_for_B(struct pkt packet);
bool my_condition_for_time(struct time_data);
bool comparison_for_time_window(struct time_data a, struct time_data b);
bool comparison_for_b(struct pkt a, struct pkt b);

/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message)
{
	struct pkt packet;
    make_my_packet(&packet, &message, next_seq_num);
    if(next_seq_num < baseA+window_size)
    {
        window.push_back(packet);
	
		struct time_data packet_time;					//Have a timer window to keep track of times of various packets sent and their timeout times
		packet_time.seq_num = next_seq_num;
		packet_time.time = get_sim_time() + TIMER_DELAY;
		packet_time.acknowledged = false;
		time_windowA.push_back(packet_time);
		printf("A_output called. ");
		printf("Sending out seq number packet :%d\n", packet.seqnum);	
        tolayer3(A_SIDE, packet);
        if(next_seq_num == baseA)
            starttimer(A_SIDE, TIMER_DELAY);
        next_seq_num++;
    }
    else
    {
        if(bufferA.size() >= BUFFER_SIZE)
        {
            printf("Buffer count exceeded. Program shutting down.................\n");
            exit(0);
        }
        bufferA.push_back(packet);
        next_seq_num++;
    }
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
	if(check_for_my_error(&packet))
	{
		printf("A_input called. ");
		printf("Receving packet seq num:%d\n", packet.seqnum); 
		if(packet.seqnum == baseA)
		{
			for(int i=0;i<time_windowA.size();i++)
			{	if(time_windowA[i].seq_num == packet.seqnum)
				{
					time_windowA[i].acknowledged = true;
					break;
				}
			}
			for(int i=0;i<window.size();i++) 
			{
				if( baseA == window[i].seqnum && time_windowA[i].acknowledged )
				{
					baseA++;
					printf("Base moved forward, baseA:%d. ", baseA);
				}
			}			//Remove that sequence numbers until those packets which have been already been acked and update window
			window.erase(remove_if(window.begin(), window.end(), my_condition_for_A), window.end());
			time_windowA.erase(remove_if(time_windowA.begin(), time_windowA.end(), my_condition_for_time), time_windowA.end());
			//Buffer adding to the window here
			int count = 0;
			while(window.size() < window_size && bufferA.size() > 0)
		    {
 		        window.push_back(bufferA[0]);                            //Remove messages from buffer and put it into the window till window does not become full
				tolayer3(A_SIDE, bufferA[0]);

				struct time_data time_packet;
				time_packet.seq_num = bufferA[0].seqnum;
				time_packet.time =  get_sim_time() + TIMER_DELAY + count++;
			    time_packet.acknowledged = false;
				time_windowA.push_back(time_packet);

 		        bufferA.erase(bufferA.begin());
 		    }

		}
		else 
		{
			//Ack the time_data here
			for(int i=0;i<time_windowA.size();i++)
				if(time_windowA[i].seq_num == packet.seqnum)
					time_windowA[i].acknowledged = true;
		}
		if(baseA == next_seq_num)
			stoptimer(A_SIDE);
/*Timer logic: Take the least min time of the packets in the window and set the next interrupt time to that */
		else
		{
			stoptimer(A_SIDE);
			float least_time = FLT_MAX;
	        for(int i=0;i<time_windowA.size();i++)                              //Start the timer till the min amount of time needed for the min time that a packet 
	 	    { 
				if(!time_windowA[i].acknowledged)                               //ack needs to expire
	            	least_time = min(least_time, time_windowA[i].time);
		    }
		    if(least_time != FLT_MAX)
		        starttimer(A_SIDE, least_time - get_sim_time());
		     else
		         starttimer(A_SIDE, TIMER_DELAY);
		}
		
	}	
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
	int count = 0; 
	while(window.size() < window_size && bufferA.size() > 0)
    {   
	    window.push_back(bufferA[0]);                            //Remove messages from buffer and put it into the window till window does not become full
              
        struct time_data time_packet;
        time_packet.seq_num = bufferA[0].seqnum;
        time_packet.time =  get_sim_time() + TIMER_DELAY+count++;
        time_packet.acknowledged = false;
        time_windowA.push_back(time_packet);
                
        bufferA.erase(bufferA.begin());
    }
	printf("Timer interrupt. ");
	printf("Window.size():%d\n", window.size());
	printf("Sequence numbers that have not yet been acked and resending:\n"); 
	for(int i=0;i<window.size();i++)
	{
		/*If timeout has occured for a packet and it is unacknowledged then resend it and reset timout value to the new one */
		if(get_sim_time() >= time_windowA[i].time && !time_windowA[i].acknowledged)
		{
			printf("packet-seqnum:%d. Time_window seqnum:%d. Time:%f ", window[i].seqnum, time_windowA[i].seq_num, time_windowA[i].time);	
			tolayer3(A_SIDE, window[i]);
			time_windowA[i].time = get_sim_time()+TIMER_DELAY;
		}
	}
	/*Logic for setting the net timeout value of the timer */
	float least_time = FLT_MAX;
	for(int i=0;i<time_windowA.size();i++)								//Start the timer till the min amount of time needed for the min time that a packet 
	{
		if(!time_windowA[i].acknowledged)								//ack needs to expire
				least_time = min(least_time, time_windowA[i].time);
	}
	/* Basic error handling. If least_time has not changed, we do not want infinite timer time */ 
	if(least_time != FLT_MAX)
		starttimer(A_SIDE, least_time - get_sim_time());		
	else
		starttimer(A_SIDE, TIMER_DELAY);
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
	if(check_for_my_error(&packet))                        //Check if any error is there in the packet
    {
		//If I find a packet which lies in between base and window then I will need to bufer it 
		printf("B_input called. ");
		if(packet.seqnum > baseB && packet.seqnum < baseB + window_size)
		{
			if(!already_exists_in_buffer(packet.seqnum))
			{
				bufferB.push_back(packet);
				printf("Just pushed back bufferB seq num:%d\n", packet.seqnum);
			}
			printf("Buffering the input for future passing and buffer contents are:\n");
			for(int i=0;i<bufferB.size();i++)
				printf("Buffer seqnum:%d", bufferB[i].seqnum);
			printf("\n");
			tolayer3(B_SIDE, packet);			 		
		}
        else if(baseB == packet.seqnum)                          //If the sequence number is what we are expecting then send it to layer5, send ACK to layer 3 
		{														//If base found then update the value to most largest packet which is present in buffer
            tolayer5(B_SIDE,packet.payload);					// contiguously and update the baseB number properly
            tolayer3(B_SIDE,packet);
            baseB++;
			printf("BaseB==seqnum %d\n", packet.seqnum);
			sort(bufferB.begin(), bufferB.end(), comparison_for_b);
			for(int i=0;i<bufferB.size();i++)
	            printf("Buffer seqnum:%d", bufferB[i].seqnum);
			deliver_ack_packets_to_layer5(&baseB);				//packets delivered to upper layer and window moved forward(packet seq < baseB deleted)
        }
		else if( packet.seqnum >= baseB - window_size )
		{
			tolayer3(B_SIDE, packet);							//If seq number of packet is before baseB, resend it so that A side can update its window
			printf("Sending back ack num %d \n", packet.seqnum);
		}
    }
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
	baseB = 1;
}

/*Create a packet with the required sequence number from parameter message */
void make_my_packet(struct pkt *packet, struct msg *message, int call)
{
    packet->seqnum = call;
    packet->acknum = call;
    int character_sum = 0;
    for(int i=0;i<PACKET_SIZE;++i)
        character_sum+= message->data[i];
    packet->checksum = call+call+character_sum;
    packet->checksum = ~packet->checksum;                   //Checksum done here
    memset(packet->payload, '\0', sizeof(packet->payload) );
    memcpy(packet->payload, message->data, sizeof(message->data));
}

/* Check for any errors in the packet */
bool check_for_my_error(struct pkt *packet)
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
/*Packet condition to check if packet sequence number is equal to the window sequence numbers */
bool my_condition_for_A(struct pkt packet)
{
    return packet.seqnum < baseA;
}
/*If a seqnum already exists in buffer B, then do not add it again */
bool already_exists_in_buffer(int seq_num)
{
	for(int i=0;i<bufferB.size();i++)
	{
		printf("bufferB[i].seqnum=%d", bufferB[i].seqnum);	
		if(bufferB[i].seqnum == seq_num)
			return true;
	}
	return false;
}

/*Checking to see if this packet has a sequence less than baseB. If so then delete this from the buffer as this packet has been delivered to layer5 */
bool my_condition_for_B(struct pkt packet)
{
	return packet.seqnum < baseB;
}


/*Deliver the packets from Base to the packet until window ends or packet is missing in between */
void deliver_ack_packets_to_layer5(int *base)
{
	for(int i=0;i<bufferB.size();i++)
	{
		if(bufferB[i].seqnum == baseB) {
			tolayer5(B_SIDE, bufferB[i].payload);
			baseB++; 
		}
	}
	printf("Deleting buffer contents\n");
	bufferB.erase(remove_if(bufferB.begin(), bufferB.end(), my_condition_for_B), bufferB.end()); 
	
}
/*Remove those members whose packet seq num  value is less than baseA */
bool my_condition_for_time(struct time_data time)
{
	return time.seq_num < baseA;
}
/*Remove those packets who have a seq num less than baseB */
bool comparison_for_b(struct pkt a, struct pkt b)
{
	return a.seqnum < b.seqnum;
}
