#ifndef N7_ASSIGNMENT3_H
#define N7_ASSIGNMENT3_H

#include <stdlib.h>
#include "router.h"


bool time_to_send_updates_has_expired(Router *router);
void send_updates_to_neighbours(Router *);


#endif
