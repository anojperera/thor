/*
 * Implementation of main application class. All applications are
 */

#include "thapp.h"

#define THAPP_SLEEP_KEY "main_sleep"
#define THAPP_DEFAULT_SLEEP 100
/*
 * Configuration paths. The default is to look in the home
 * directory if not in /etc/
 */
#define THAPP_CONFIG_PATH1 ".config/thor.cfg"
#define THAPP_CONFIG_PATH2 "/etc/thor.cfg"

#define THAPP_QUIT_CODE1 81								/* Q */
#define THAPP_QUIT_CODE2 113								/* q */

/*
 * Method for handling the main loop.
 */
static void* _thapp_start_handler(void* obj);


/* Initialise helper method, loads extra configuration helper methods */
static int _thapp_init_helper(thapp* obj);

/*---------------------------------------------------------------------------*/
static char _thapp_get_cmd(void);


/*===========================================================================*/

/* Initialise the application object */
int thapp_init(thapp* obj)
{
    /* Check for arguments */
    if(obj == NULL)
	return -1;

    obj->var_config;
    memset(&obj->_msg_buff, 0, sizeof(struct thor_msg));

    /*
     * Initialise the connection object.
     * If failed, indicate error and exit the function
     */
    if(thcon_init(&obj->_var_con, thcon_mode_client))
	return -1;
    
    obj->var_init_flg = 1;
    obj->var_init_flg = 0;
    obj->var_sleep_time = THAPP_DEFAULT_SLEEP;
    obj->var_child = NULL;

    /*
     * Default is to run in master mode.
     */
    obj->var_op_mode = thapp_master;

    /* Initialise the function poitner array */
    THAPP_INIT_FPTR(obj);
    sem_init(&obj->_var_sem, 0, 0);
    _thapp_init_helper(obj);
    return 0;
}

/* Destructor method */
void thapp_delete(thapp* obj)
{
    if(obj == NULL)
	return;

    /* Check if the test is running and stop the test */
    thapp_stop(obj);

    /* Wait on semaphore before continuing */
    sem_wait(&obj->_var_sem);
    
    /*
     * If destructor function pointer for the child class was
     * assigned, call it before uninitialising the variables
     * in this class
     */
    if(obj->_var_fptr.var_del_ptr)
	obj->_var_fptr.var_del_ptr(obj, obj->var_child);

    
    obj->var_init_flg = 0;
    obj->var_run_flg = 0;

    /* Check configuration pointer and delete it */
    config_destroy(&obj->var_config);
    obj->var_config = NULL;
    obj->var_child = NULL;

    return;
}


/*
 * This is the main loop. The calling function shall be blocked here when
 * running in the master mode. A thread function is called in the master mode.
 * All events are handled in this method.
 */
int thapp_start(thapp* obj)
{
    /* Check for arguments */
    if(obj == NULL)
	return -1;

    /* If the test is already running, return function */
    if(obj->var_run_flg == 1)
	rerurn 0;

    /* Check the operation mode execute the start handler method */
    if(obj->var_op_mode == thapp_headless)
	{
	    /*
	     * Its running in headless mode, therefore
	     * we call the handling method in another thread
	     * to give control back to the calling function.
	     */
	    pthread_create(&obj->_var_thread, NULL, _thapp_start_handler, (void*) obj);
	    return 0;
	}
    else
	_thapp_statt_handler((void*) obj);

    return 0;
}

/*===========================================================================*/
/**************************** Private Methods ********************************/

/* Main loop method */
static void* _thapp_start_handler(void* obj)
{
    char _cmd;
    thapp* _obj;

    /* Check object pointer and cast to the correct type */
    if(obj == NULL)
	return NULL;

    _obj = (thapp*) obj;

    /*
     * First thing we do is to check if the any derived child
     * classes has set initialise and start methods and call
     * them if they were set. This gives a chance to initialise
     * any variables before loop start.
     */
    if(_obj->_var_fptr.var_init_ptr)
	_obj->_var_fptr.var_init_ptr(_obj, _obj->var_child);
    if(_obj->_var_fptr.var_start_ptr)
	_obj->_var_fptr.var_start_ptr(_obj, _obj->var_child);

    
    /* Main loop */
    while(1)
	{
	    /*
	     * Check for keyboard input. Apart, from the quit and stop signals,
	     * all others are passed to the derived classes to handle.
	     */
	    _cmd = _thapp_get_cmd(void);
	    if(_cmd == THAPP_QUIT_CODE1 || _cmd == THAPP_QUIT_CODE2)
		{
		    /* Handle quit event */
		    _obj->_var_fptr.var_stop_ptr(_obj, _obj->var_child);
		    break;
		}

	    /* Passed Command handling to the child class */
	    if(_obj->_var_fptr.var_cmdhnd_ptr)
		_obj->_var_fptr.var_cmdhnd_ptr(_obj, _obj->var_child, _cmd);
	    
	    usleep(_obj->var_sleep_time);
	}

    obj->var_run_flg = 0;
    return NULL;
}

/* Configuration loader */
static int _thapp_init_helper(thapp* obj)
{
    const config_setting_t* _setting;
    /* Initialise the configuration object */
    config_init(&obj->var_config);

    /* Try and read the file from the first path */
    if(config_read_file(&obj->var_config, THAPP_CONFIG_PATH1) != CONFIG_TRUE)
	{
	    /* Read from the /etc directory  */
	    if(config_read_file(&obj->var_config, THAPP_CONFIG_PATH2) != CONFIG_TRUE)
		return -1;
	}

    _setting = config_lookup(obj->var_config, THAPP_SLEEP_KEY);
    if(_setting == NULL)
	return -1;

    /* Set sleep time as defined in the config file */
    obj->var_sleep_time = config_setting_get_int(_setting);

    return 0;
}
