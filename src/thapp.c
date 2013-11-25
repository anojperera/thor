/*
 * Implementation of main application class. All applications are
 */

#include "thapp.h"

/*
 * Method for handling the main loop.
 */
static void* _thapp_statt_handler(void* obj);


/* Initialise helper method, loads extra configuration helper methods */
static int _thapp_init_helper(thapp* obj);

/* Initialise the application object */
int thapp_init(thapp* obj)
{
    /* Check for arguments */
    if(obj == NULL)
	return -1;

    obj->var_config = NULL;
    memset(&obj->_msg_buff, 0, sizeof(struct thor_msg));

    /*
     * Initialise the connection object.
     * If failed, indicate error and exit the function
     */
    if(thcon_init(&obj->_var_con, thcon_mode_client))
	return -1;
    
    obj->var_init_flg = 1;
    obj->var_init_flg = 0;
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
    if(obj->var_config)
	config_destroy(obj->var_config);
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
	    pthread_create(&obj->_var_thread, NULL, _thapp_statt_handler, (void*) obj);
	    return 0;
	}
    else
	_thapp_statt_handler((void*) obj);

    return 0;
}
