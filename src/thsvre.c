/*
 * Executable for the server component.
 */
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <libconfig.h>
#include "thsvr.h"

#define THSVR_DEFAULT_CONFIG_PATH1 "thor.cfg"
#define THSVR_DEFAULT_CONFIG_PATH2 "/etc/thor.cfg"
    
thsvr _var_svr;							/* Server component */
config_t _var_config;						/* Configuration settings object */

/*
 * Loads the configuration setting from either home directory
 * or the /etc/ directory.
 */
static int _thsvre_load_config(void);

/* Signal handler */
static void _thsvre_sigterm_handler(int signo);

int main(int argc, char** argv)
{


    if(_thsvre_load_config())
	exit(1);

    /* Initialise the server component object */
    if(thsvr_init(&_var_svr, &_var_config))
	goto thsvre_exit;

    /* Attach a signal handler */
    signal(SIGTERM, _thsvre_sigterm_handler);
    
    /* Start the program */
    thsvr_start(&_var_svr);
    
    /*
     * This is the main loop. run for ever
     * until terminated.
     */
    while(1)
	sleep(1);

 thsvre_exit:
    config_destroy(&_var_config);
    return 0;
}


static int _thsvre_load_config(void)
{
    config_init(&_var_config);

    /*
     * First we read from the home directory. If failed,
     * we will continue to read from /etc/
     */
    if(config_read_file(&_var_config, THSVR_DEFAULT_CONFIG_PATH1) == CONFIG_TRUE)
	return 0;

    if(config_read_file(&_var_config, THSVR_DEFAULT_CONFIG_PATH2) == CONFIG_TRUE)
	return 0;

    /*
     * If failed at this point, we indicate failure and not to continue
     * with the program.
     */

    return -1;
}

/* Signal handler */
static void _thsvre_sigterm_handler(int signo)
{
    if(signo == SIGTERM || signo == SIGINT)
	{
	    /* Stop the server */
	    thsvr_stop(&_var_svr);
	    thsvr_delete(&_var_svr);
	}

    /* Destroy the configuration object */
    config_destroy(&_var_config);
    return;
}
