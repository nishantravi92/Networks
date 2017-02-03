/**
 * @n7_assignment1
 * @author  Nishant Ravichandran <n7@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <iostream>
#include <stdio.h>

#include "../include/global.h"
#include "../include/logger.h"
#include "../include/server.h"
#include "../include/client.h"
using namespace std;

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
int main(int argc, char **argv)
{
	/*Init. Logger*/
	cse4589_init_log(argv[2]);

	/* Clear LOGFILE*/
    fclose(fopen(LOGFILE, "w"));
	if(*argv[1] == 's')
		run_server_application(argv[2]);
	else if(*argv[1] == 'c')
		run_client_application(argv[2]);
	else {
		cse4589_print_and_log("Wrong input format entered. Application exiting.....");
	} 
	/*Start Here*/
	
	return 0;
}
