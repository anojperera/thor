/*
 * This is C++ implementation of a server component for
 * handling of logging messages.
 */
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <libconfig.h>
#include <signal.h>


#include <set>
#include <queue>

#define THSVR_DEFAULT_CONFIG_PATH1 "thor.cfg"
#define THSVR_DEFAULT_CONFIG_PATH2 "../config/thor.cfg"
#define THSVR_DEFAULT_CONFIG_PATH3 "/etc/thor.cfg"

volatile sig_atomic_t _flg = 1;

struct _thasg_msg_wrap
{
    int _fd;						/* File descriptor */
    char _msg[THORNIFIX_MSG_BUFF_SZ];			/* message buffer */
};


class _thasg
{
private:
    int err_flg;
    
    std::queue<struct _thasg_msg_wrap> _msg_queue;	/* Message queue */
    std::set<int, std::less<int>> _fds;			/* file descriptor array */
    
    thcon var_con;
    config_t var_config;
    pthread_mutex_t var_mutex;    
public:

    pthread_t var_log_thread;
    
    _thasg();
    virtual ~_thasg();

    int add_msg(void* msg_ptr, size_t sz);
    int write_file(void);
    int start(void);
};


/* Callback methods for the connection object */
static int _thasgard_con_recv_msg(void* self, void* msg, size_t sz);
static int _thasgard_con_made(void* self, void* con);
static int _thasgard_con_closed(void* self, void* con, int sock);

/* Thread function */
static void* _thasgard_thread_function(void* obj);

int main(int argc, char** argv)
{
    _thasg asg;
    void* _obj_ptr;

    /* Cast object pointer of the class */
    _obj_ptr = reinterpret_cast<void*>(&asg);

    /*
     * Start the server in another thread and shall continue to
     * execute until Ctrl+C or SIGINT is recieved.
     * Messages are checked in the queue and written to the
     * separate files.
     */
    pthread_create(&asg.var_log_thread, NULL, _thasgard_thread_function, _obj_ptr);
    
    /* Initialise pthread object */
    while(_flg)
	sleep(1);
    
    
    return 0;
}


/****************************************************************************/
/*----------------------- Implementation of the class ----------------------*/

/* Class constructor */
_thasg::_thasg():err_flg(0)
{
    int stat = 0;
    /* Initialise configuration settings */
    config_init(&this->var_config);
    
    /* Check the default paths for the configuration file and find the settings */
    while(1)
	{
	    /*
	     * If any of the following attempts were successful, we set the status
	     * and exit the loop.
	     */
	       
	    if(config_read_file(&this->var_config, THSVR_DEFAULT_CONFIG_PATH) == CONFIG_TRUE)
		goto check_path_success;

	    if(config_read_file(&this->var_config, THSVR_DEFAULT_CONFIG_PATH2) == CONFIG_TRUE)
		goto check_path_success;

	    if(config_read_file(&this->var_config, THSVR_DEFAULT_CONFIG_PATH3) == CONFIG_TRUE)
		goto check_path_success;

	    /*
	     * If this point was reached, that means no settings were found. Therefore
	     * we exit the loop and inidicate the function as failed
	     */
	    break;
	    
	check_path_success:
	    stat = 1;
	    break;
	}

    /* Set the error flag and return the constructor */
    /*
     * Connection object shall not be initialised in if this
     * condition was true.
     */
    if(stat == 0)
	{
	    err_flg = 1;
	    return;
	}
       
}
