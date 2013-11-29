/*
 * Executable for the server component.
 */
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "thsvr.h"


thsvr _var_svr;							/* Server component */

/* Signal handler */
static void _thsvre_sigterm_handler(int signo);

int main(int argc, char** argv)
{

    /* Initialise the server component object */
    
    
    /*
     * This is the main loop. run for ever
     * until terminated.
     */
    while(1)
	sleep(1);
    return 0;
}

