#include <stdlib.h>
#include <stdio.h>
#include "../include/routing_table.h"
#include "../include/router.h"
#include <iostream>
#include <vector>
#include <string.h>
using namespace std;


/* Update the routing table with the new values received from the routers */
void update_routing_table_with_new_values(Router *router, vector<Neighbour> &neighbour_list, char *update_buffer, int nbytes)
{
    int i = 0;
    int num_of_routers = unpacku16(update_buffer+i);
	i+=4;
	char source_ip_addr[INET_ADDRSTRLEN];
	convert_bits_to_ip(update_buffer+i, source_ip_addr);
	i+=12;							//i+=8 as router 1 ip address is also skipped along with ppadding and port
	Neighbour *source = get_neighbour_for_ip(neighbour_list, source_ip_addr);
	if(source == NULL) return; 
	gettimeofday(&source->time_to_receive_update, NULL);
	source->first_update_message_received = true;
	while(num_of_routers > 0)
	{
		int router_id = unpacku16(update_buffer+i);
		i+=2;
		Neighbour *router_destination = get_neighbour_for_id(neighbour_list, router_id);
		int cost = unpacku16(update_buffer+i);
		if(source != NULL && router_destination != NULL)
			router->routing_table[source->matrix_id][router_destination->matrix_id] = cost;	
		i+=10;	
		num_of_routers--;	
	}
}

/* Bellman Ford algorithm from https://en.wikipedia.org/wiki/Bellman-Ford_algorithm and
 *  https://www.topcoder.com/community/data-science/data-science-tutorials/introduction-to-graphs-and-their-data-structures-section-3/
 *  We only need the shortest path from one node which is our source to all other nodes */
void find_shortest_paths(Router *router, vector<Neighbour> & neighbour_list)
{
	uint16_t INFINITY = ~0;
	vector<Neighbour>::iterator i;
	//Setting everyting to zero initially 
    for(i = neighbour_list.begin(); i != neighbour_list.end();++i)
 	  {
       if(i->cost != 0)
       {
 	      i->cost = ~0;
          i->next_hop_id = -1;
        }
      }
	
	Neighbour *source = get_neighbour_for_id(neighbour_list, router->id);
	for(int i=1;i<MAX_ROUTERS;i++)
	{
		Neighbour *neigh_i = get_neighbour_for_matrix_id(neighbour_list, i); 
		for(int j=1;j<MAX_ROUTERS;j++)
		{
			Neighbour *neigh_j = get_neighbour_for_matrix_id(neighbour_list, j);
			uint16_t cost_from_source_to_j;
			if(neigh_j != NULL && neigh_i != NULL)
			{ 	
				cost_from_source_to_j = router->routing_table[source->matrix_id][neigh_j->matrix_id];
				uint32_t total_cost = cost_from_source_to_j + router->routing_table[j][i]; 
				if( total_cost <= neigh_i->cost ) 
				{
					neigh_i->cost = total_cost;
					neigh_i->next_hop_id = neigh_j->router_id;					
				}
			}
		}	
	}
} 

void update_cost_links(char *buffer, Router *router, vector<Neighbour> &neighbour_list)
{
	uint16_t id = unpacku16(buffer+8);
	Neighbour *neigh = get_neighbour_for_id(neighbour_list, id);
	Neighbour *source = get_neighbour_for_id(neighbour_list, router->id);
	uint16_t cost = unpacku16(buffer+10);
	if(neigh != NULL && source != NULL)
	{
		router->routing_table[source->matrix_id][neigh->matrix_id] = cost;		
	}	
}

void initilize_values_into_routing_table(Router *router)
{
    for(int m=0;m<MAX_ROUTERS;m++)                              //Fill routing table
    {
        for(int n=0; n<MAX_ROUTERS; n++)
        {
            if(m == n)
                router->routing_table[m][n] = 0;                //Cost from any router to itself will be zero
            else
            {
                router->routing_table[m][n] = ~0;               //Set all elements to infinity for the timebeing
            }
        }
    }
}

/* Helper functions start here */

Neighbour *get_neighbour_for_id(vector<Neighbour> &neighbour_list, int id)
{
    vector<Neighbour>::iterator i;
    for(i = neighbour_list.begin(); i!= neighbour_list.end(); ++i)
    {
        if(i->router_id == id)
            return &(*i);
    }
    return NULL;
}

Neighbour *get_neighbour_for_matrix_id(vector<Neighbour> &neighbour_list, int matrix_id)
{
    vector<Neighbour>::iterator i;
    for(i = neighbour_list.begin(); i!= neighbour_list.end(); ++i)
    {
        if(i->matrix_id == matrix_id)
            return &(*i);
    }
    return NULL;
}

Neighbour *get_neighbour_for_ip(vector<Neighbour> &neighbour_list, char *ip_addr)
{
    vector<Neighbour>::iterator i;
    for(i = neighbour_list.begin(); i!= neighbour_list.end(); ++i)
    {
        if(strcmp(ip_addr, i->ip_addr) == 0)
            return &(*i);
    }
    return NULL;
}

void display_routing_table(Router *router)
{
    for(int i=0;i<MAX_ROUTERS;i++)
    {
        for(int j=0;j<MAX_ROUTERS;j++)
        {
            cout<<router->routing_table[i][j]<<" ";
        }
            cout<<endl;
            fflush(stdout);
    }
}

void display_neighbour_list(vector<Neighbour> & neighbour_list)
{
	vector<Neighbour>::iterator i;
	for(i = neighbour_list.begin(); i != neighbour_list.end();i++)
	{
		cout<<"id:"<<i->router_id<<" cost:"<<i->cost<<" next-Hop ID:"<<i->next_hop_id;
		cout<<endl;
		fflush(stdout);
	}
}
