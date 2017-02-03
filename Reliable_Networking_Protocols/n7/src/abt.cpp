#include "../include/simulator.h"
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include "../include/abt.h"
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

float TIMER_DELAY = 15.0;
bool message_in_transit; 
int call_numberA;
int call_numberB;
struct msg *prev_message;
int message_recv;
int prev_numberB;

void make_packet(struct pkt *packet, struct msg *message, int call);
bool check_for_error(struct pkt *packet);
/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message)
{
	if(!message_in_transit)
	{
		message_in_transit = true;
		memcpy(prev_message->data, message.data, sizeof(message.data));
		struct pkt packet;
		make_packet(&packet, &message, call_numberA);					//Create a packet, send it to layer 3 and start the timer 
 	   	tolayer3(A_SIDE, packet);
 	  	starttimer(A_SIDE, TIMER_DELAY);
		printf("A_output called: Callnumber=%d, message=%.20s\n", call_numberA, message.data); 
	}
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
	printf("A_input called: Callnumber=%d, packet.seqnum = %d, message=%.20s, corruption=%d\n", call_numberA, packet.seqnum,  packet.payload,check_for_error(&packet) );
	if( check_for_error(&packet) )
	{
		if(packet.seqnum == call_numberA)				//If the ack is what we are expecting then move on to the next state
		{
			stoptimer(A_SIDE);
			message_in_transit = false;
			call_numberA = (call_numberA+1)%2; 	
		}	
	}
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
	if(message_in_transit)
	{
		printf("Timer called: Callnumber=%d\n", call_numberA);
		struct pkt packet;
		make_packet(&packet, prev_message, call_numberA);			//Make a prev packet and send it to layer 3 after which we	
		tolayer3(A_SIDE, packet);									//start the timer
		starttimer(A_SIDE, TIMER_DELAY);
	}
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
	message_in_transit = false;
	call_numberA = CALL0;
	prev_message = (struct msg *)malloc(sizeof (struct msg));
	memset(prev_message->data, '\0', sizeof(prev_message->data));	

}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
	printf("B_Input has received:\n Call_numberB=%d, packet.seqnum = %d, data: %.20s, corrupted=%d\n", call_numberB, packet.seqnum, packet.payload, check_for_error(&packet));
	struct msg message;
	memset(message.data, '\0', sizeof(message.data) );	
	if( check_for_error(&packet))
	{
		if(call_numberB == packet.seqnum)
		{	
			tolayer3(B_SIDE, packet);
			prev_numberB = call_numberB;	 
			call_numberB = (call_numberB+1) % 2;				//Move forward to next state
			tolayer5(B_SIDE, packet.payload);
			message_recv++;
			printf("Message received properly and message counter=%d", message_recv);
		}
		else
		{
			tolayer3(B_SIDE, packet);							//If wrong number sent then send it back to move state of A forward 	
		}
	}
	else
	{
		make_packet(&packet,&message, prev_numberB);
		tolayer3(B_SIDE, packet);								//If checksum is wrong then resend prev ACK
	}
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
	call_numberB = CALL0;
	prev_numberB = ~0;
	message_recv = 0;		
}

/*Create a packet with the required sequence number from parameter message */
void make_packet(struct pkt *packet, struct msg *message, int call)
{
	packet->seqnum = call;
	packet->acknum = call;
	int character_sum = 0;
	for(int i=0;i<PACKET_SIZE;++i)
		character_sum+= message->data[i];
	packet->checksum = call+call+character_sum;
	packet->checksum = ~packet->checksum;					//Checksum done here
	memset(packet->payload, '\0', sizeof(packet->payload) );	
    memcpy(packet->payload, message->data, sizeof(message->data));
}

/*Simple checksum to check for error */
bool check_for_error(struct pkt *packet)
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
