#ifndef ROUTING_TABLE_H

#define ROUTING_TABLE_H
#include <vector>
#include "router.h"
#include "n7_assignment3.h"

using namespace std;

void update_routing_table_with_new_values(Router *router, vector<Neighbour> &neighbour_list, char *update_buffer, int nbytes);
Neighbour *get_neighbour_for_id(vector<Neighbour> &neighbour_list, int id);
Neighbour *get_neighbour_for_ip(vector<Neighbour> &neighbour_list, char *ip_addr);
Neighbour *get_neighbour_for_matrix_id(vector<Neighbour> &neighbour_list, int matrix_id);
void initilize_values_into_routing_table(Router *router);
void display_routing_table(Router *router);
void display_neighbour_list(vector<Neighbour> &neighbour_list);
void find_shortest_paths(Router *router, vector<Neighbour> & neighbour_list);
void update_cost_links(char *, Router *router, vector<Neighbour> & neighbour_list);
#endif
