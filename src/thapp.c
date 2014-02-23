/*
 * Implementation of main application class. All applications are
 */
#include <unistd.h>
#include <signal.h>
#include <ncurses.h>
#include <time.h>
#include "thapp.h"

#define THAPP_DEFAULT_LOG_FILE_NAME "/tmp/%Y-%m-%d-%I-%M-%S.txt"

#define THAPP_START_MSG "Press 'Shift + s' to begin"
#define THAPP_PAUSED_MSG "<============================== Paused ===================================>"
#define THAPP_MSG_BLANK "                                                                           "
#define THAPP_GEO_LOC_DISPLAY "My IP Address: %s\n"

#define THAPP_GEOLOC_KEY "mygeo_location"
#define THAPP_SLEEP_KEY "main_sleep"
#define THAPP_PORT_KEY "main_con_port"
#define THAPP_SERVER_NAME_KEY "self_url"
#define THAPP_QUEUE_LIMIT_KEY "app_queue_limit"
#define THAPP_LOG_URL_KEY "main_log_url"
#define THAPP_LOG_URL_PORT_KEY "sec_con_port"

#define THAPP_DEFAULT_PORT "11000"
#define THAPP_DEFAULT_SLEEP 100000
#define THAPP_DEFAULT_WAIT_TIME 1
#define THAPP_DEFAULT_TRY_COUNT 5
#define THAPP_DEFAULT_CMD_MSG_TIME 6000000
#define THAPP_DEFAULT_QUEUE_LIMIT 10
/*
 * Configuration paths. The default is to look in the home
 * directory if not in /etc/
 */
#define THAPP_CONFIG_PATH1 ".config/thor.cfg"
#define THAPP_CONFIG_PATH2 "/etc/thor.cfg"
#define THAPP_CONFIG_PATH3 "thor.cfg"

#define THAPP_QUIT_CODE1 81								/* Q */
#define THAPP_QUIT_CODE2 113								/* q */
#define THAPP_START_CODE 83								/* S */
#define THAPP_STOP_CODE 115								/* s */
#define THAPP_PAUSE_CODE1 112								/* p */
#define THAPP_PAUSE_CODE2 32								/* p */

/* Display lines for the messages */
#define THAPP_VAL_LINE 2
#define THAPP_CMD_LINE 0
#define THAPP_HDD_LINE 1
#define THAPP_SP_LINE 11

#define THAPP_CLOSE_LOG(obj)			\
    if((obj)->var_def_log)			\
	fclose((obj)->var_def_log);		\
    (obj)->var_def_log = NULL

#define THAPP_SEND_MSG(obj_ptr, msg_ptr, msg_sz)			\
    if((obj_ptr)->_var_con_sec_flg)					\
	thcon_send_info(&(obj_ptr)->_var_con_sec, (void*) (msg_ptr), (msg_sz))

volatile sig_atomic_t _flg = 1;
static void _thapp_sig_handler(int signo);

/* Open file with time stamp */
static void _thapp_open_log_file(thapp* obj);

/*
 * Method for handling the main loop.
 */
static void* _thapp_start_handler(void* obj);


/* Initialise helper method, loads extra configuration helper methods */
static int _thapp_init_helper(thapp* obj);

/*---------------------------------------------------------------------------*/
static char _thapp_get_cmd(void);

/* Start the secondary connection in three atempts */
static void* _thapp_start_secon(void* obj);

/*---------------------------------------------------------------------------*/
/* Callback methods for handling connection related messages */
static int _thapp_con_recv_callback(void* obj, void* msg, size_t sz);
static int _thapp_con_recv_url_callback(void* obj, void* msg, size_t sz);

static void _thapp_queue_del_helper(void* data);
/*===========================================================================*/

/* Initialise the application object */
int thapp_init(thapp* obj)
{
    int _stat = 0;
    /* Check for arguments */
    if(obj == NULL)
	return -1;

    memset(&obj->_msg_buff, 0, sizeof(struct thor_msg));

    /* Set signal handler */
    signal(SIGINT, _thapp_sig_handler);

    /*
     * Initialise the connection object.
     * If failed, indicate error and exit the function
     */
    if(thcon_init(&obj->_var_con, thcon_mode_client))
	return -1;

    /*
     * Set external object of connection and
     * callback method for handling recv values.
     */
    thcon_set_ext_obj(&obj->_var_con, ((void*) obj));
    thcon_set_recv_callback(&obj->_var_con, _thapp_con_recv_callback);

    /*
     * Initialise the url logging connection. All test data
     * processed by the application shall be relayed to this
     * server. Since this connection is not critical to the operation
     * we can proceed with the reset of the initialisation.
     */
    obj->var_sec_con_init_flg = 0;
    if(!thcon_init(&obj->_var_con_sec, thcon_mode_client))
	{
	    /* Set external object and callback pointer */
	    thcon_set_ext_obj(&obj->_var_con_sec, ((void*) obj));
	    thcon_set_recv_callback(&obj->_var_con_sec, _thapp_con_recv_url_callback);
	    obj->var_sec_con_init_flg = 1;

	    /* Port number and server address are set in the config read method */
	}
    

    obj->var_init_flg = 1;
    obj->var_run_flg = 0;
    obj->var_max_opt_rows = 0;
    obj->_msg_cnt = 0;
    obj->_var_con_sec_flg = 0;
    obj->var_sleep_time = THAPP_DEFAULT_SLEEP;
    obj->var_queue_limit = THAPP_DEFAULT_QUEUE_LIMIT;
    obj->var_child = NULL;
    obj->var_def_log = NULL;

    obj->_var_con_sec_flg = 0;
    obj->var_sec_con_start_flg = 0;

    /* Call initialisation helper method to load configuration settings */
    /*
     * Add messaging for all child classes.
     */
    _stat = _thapp_init_helper(obj);
    if(_stat)
	{
	    thcon_delete(&obj->_var_con);
	    return -1;
	}
    /*
     * Default is to run in master mode.
     */
    obj->var_op_mode = thapp_master;

    /* Initialise the function poitner array */
    THAPP_INIT_FPTR(obj);
    gqueue_new(&obj->_var_msg_queue, _thapp_queue_del_helper);
    sem_init(&obj->_var_sem, 0, 0);
    pthread_mutex_init(&obj->_var_mutex, NULL);
    return _thapp_init_helper(obj);
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


    obj->var_init_flg = 0;
    obj->var_run_flg = 0;

    /* Delete config object */
    thcon_stop(&obj->_var_con);
    thcon_delete(&obj->_var_con);

    /*
     * If the secondary connection was successfully initialised
     * we stop the connection and delete the object.
     */
    if(obj->var_sec_con_init_flg)
	{
	    /* Join the thread if it was created */
	    if(obj->var_sec_con_start_flg)
		pthread_join(obj->_var_sec_thread, NULL);
	    thcon_stop(&obj->_var_con_sec);
	    thcon_delete(&obj->_var_con_sec);
	    obj->var_sec_con_init_flg = 0;	    
	}

    /* Check configuration pointer and delete it */
    config_destroy(&obj->var_config);

    /* If default file pointer was still open free it */
    if(obj->var_def_log)
	fclose(obj->var_def_log);
    
    obj->var_def_log = NULL;
    obj->var_child = NULL;
    sem_destroy(&obj->_var_sem);
    pthread_mutex_destroy(&obj->_var_mutex);

    /* Destroy queue */
    gqueue_delete(&obj->_var_msg_queue);
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
	return 0;

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
	_thapp_start_handler((void*) obj);

    return 0;
}

/* Stops the test */
int thapp_stop(thapp* obj)
{
    if(obj == NULL)
	return -1;

    if(obj->var_op_mode == thapp_headless)
	{
	    pthread_cancel(obj->_var_thread);
	    pthread_join(obj->_var_thread, NULL);

	}
    else
	_flg = 0;

    if(obj->var_run_flg)
	endwin();

    obj->var_run_flg = 0;


    sem_post(&obj->_var_sem);
    return 0;
}

/*===========================================================================*/
/**************************** Private Methods ********************************/

/* Main loop method */
static void* _thapp_start_handler(void* obj)
{
    int cnt = THAPP_DEFAULT_TRY_COUNT;
    char _cmd;
    char _start_msg[] = THAPP_START_MSG;
    const char* _geo_loc;
    thapp* _obj;
    unsigned int _st_flg = 0;
    int _max_row, _max_col, _t_msg_pos;
    int _sec_cnt = 0, _msg_cnt_max = 0;
    unsigned int _p_flg = 0;						/* pause flag */
    size_t _sz;

    
    struct thor_msg* _msg = NULL;

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

    /* Initialisation of the curses system shall be by external */

    keypad(stdscr, TRUE);

    /* Disable line buffering and keyboard echo */
    raw();
    noecho();
    /* set time out to zero */
    timeout(0);

    /* Calculate number of second divisions */
    _sec_cnt = THAPP_SEC_DIV(_obj);
    _msg_cnt_max = THAPP_DEFAULT_CMD_MSG_TIME / _obj->var_sleep_time;

    /* Main loop */
    while(_flg)
	{
	    /*
	     * Check for keyboard input. Apart, from the quit and stop signals,
	     * all others are passed to the derived classes to handle.
	     */
	    _cmd = _thapp_get_cmd();
	    if(_cmd == THAPP_QUIT_CODE1 || _cmd == THAPP_QUIT_CODE2)
		{
		    /* Handle quit event */
		    _obj->_var_fptr.var_stop_ptr(_obj, _obj->var_child);
		    break;
		}

	    /* If program is not running display message to start */
	    getmaxyx(stdscr, _max_row, _max_col);
	    _t_msg_pos = _max_row-3;
	    if(_st_flg == 0)
		{
		    clear();
		    mvprintw(_max_row/2, (_max_col-strlen(_start_msg))/2, "%s", _start_msg);
		    refresh();
		}

	    /* Check for commands */
	    switch(_cmd)
		{
		case THAPP_START_CODE:
		    /*
		     * If the start method program have been
		     * started no further processing is required.
		     */
		    if(_st_flg > 0)
			break;

		    /*
		     * Start connection object in a loop.
		     * Try for 5seconds with a wait of 2seconds.
		     */
		    while(thcon_start(&_obj->_var_con))
			{
			    if(--cnt < 1)
				break;
			    sleep(THAPP_DEFAULT_WAIT_TIME);
			}

		    
		    /* Open log file */
		    _thapp_open_log_file(_obj);

		    /*
		     * Disable line buffering as interactive session is
		     * about to be begin by a derrived child class.
		     */
		    noraw();
		    echo();
		    timeout(-1);
		    clear();

		    /* Initialise message buffers */
		    memset(_obj->var_disp_opts, 0, THOR_BUFF_SZ);
		    memset(_obj->var_disp_header, 0, THAPP_DISP_BUFF_SZ);
		    memset(_obj->var_disp_vals, 0, THAPP_DISP_BUFF_SZ);
		    memset(_obj->var_disp_opts, 0, THOR_BUFF_SZ);
		    memset(_obj->var_disp_sp, 0, THOR_BUFF_SZ);
		    memset(_obj->var_job_num, 0, THAPP_DISP_BUFF_SZ);
		    memset(_obj->var_tag_num, 0, THAPP_DISP_BUFF_SZ);
		    thorinifix_init_msg(&_obj->_msg_buff);
		    /*
		     *  The user has entered start therefore we call the
		     *  derived classes start callback method to start the
		     *  interactive configuration session.
		     */
		    if(_obj->_var_fptr.var_start_ptr)
			_obj->_var_fptr.var_start_ptr(_obj, _obj->var_child);

		    /*
		     * After getting derrived classes to handle their start up methods,
		     * get location and display in app.
		     */
		    if(!thcon_get_my_geo(&_obj->_var_con))
			{
			    _geo_loc = thcon_get_my_addr(&_obj->_var_con);
			    /* Add to the display options list */
			    _sz = strlen(_obj->var_disp_opts);
			    sprintf(_obj->var_disp_opts+_sz, THAPP_GEO_LOC_DISPLAY, _geo_loc);
			}

		    /* Disable line buffering and keyboard echo */
		    raw();
		    noecho();
		    timeout(0);
		    _st_flg = 1;

		    /* Temporary print statements for the display values */
		    if(_obj->var_disp_opts != NULL || _obj->var_disp_opts[0] != 0)
			mvprintw(0, 0, "%s", _obj->var_disp_opts);
		    mvprintw(_t_msg_pos+THAPP_HDD_LINE, 0,"%s", _obj->var_disp_header);
		    break;
		case THAPP_STOP_CODE:

		    /*  Check if the program has already been stopped. */
		    if(_st_flg == 0)
			break;

		    /* Handle quit event */
		    if(_obj->_var_fptr.var_stop_ptr)
			_obj->_var_fptr.var_stop_ptr(_obj, _obj->var_child);

		    /* Stop connection */
		    thcon_stop(&_obj->_var_con);

		    /* Reset the queue */
		    gqueue_delete(&_obj->_var_msg_queue);
		    gqueue_new(&_obj->_var_msg_queue, _thapp_queue_del_helper);

		    /* If the log file is open, close and set to NULL */
		    THAPP_CLOSE_LOG(_obj);
		    
		    _st_flg = 0;
		    break;
		case THAPP_PAUSE_CODE1:
		case THAPP_PAUSE_CODE2:		    
		    /*
		     * If the application is paused,
		     * Indicate to the user that its paused.
		     */
		    if(_p_flg == 0)
			{
			    _p_flg = 1;
			    mvprintw(_t_msg_pos-1, 0, THAPP_PAUSED_MSG);
			}
		    else
			{
			    _p_flg = 0;
			    mvprintw(_t_msg_pos-1, 0, THAPP_MSG_BLANK);
			}
		    break;
		default:
		    break;
		}


	    /* If the program is not started continue here */
	    if(_st_flg < 1)
		goto thapp_main_loop_cont;

	    /* Check the queue and get any elements */
	    pthread_mutex_lock(&_obj->_var_mutex);
	    if(gqueue_count(&_obj->_var_msg_queue) > 0)
		{
		    gqueue_out(&_obj->_var_msg_queue, (void*) &_msg);
		    /* Copy message to buffer */
		    if(_msg != NULL)
			memcpy((void*) &_obj->_msg_buff, (void*) _msg, sizeof(struct thor_msg));

		}
	    pthread_mutex_unlock(&_obj->_var_mutex);

	    /* Free message element */
	    if(_msg != NULL)
		free(_msg);
	    _msg = NULL;

	    /*
	     * Passed Command handling to the child class.
	     * Check if the return value is true. By using the return
	     * value of the derived class's command routine, we give
	     * a chance to indicate any derived class to terminate
	     * the program in a safe mannar.
	     * The pause code is checked. Messages from the system
	     * is popped from the message queue. However processing on
	     * the popped messages shall not be performed.
	     *
	     * Logging is also done while the system is not in pause mode.
	     */
	    if(_obj->_var_fptr.var_cmdhnd_ptr && !_p_flg)
		{
		    _flg = _obj->_var_fptr.var_cmdhnd_ptr(_obj, _obj->var_child, _cmd);
		    
		    if(_obj->var_def_log)
			fprintf(_obj->var_def_log, "%s\n", _obj->var_disp_vals);
		}

	    /*
	     * If any commands were executed by the derived classes, the
	     * command message buffer would be populated with a message.
	     * we check for message and display to the user by flashing
	     * the message for 3 seconds.
	     */

	    if(_obj->var_cmd_vals[0] != 0)
		{
		    if(_obj->_msg_cnt%_sec_cnt)
			mvprintw(_t_msg_pos-THAPP_CMD_LINE, 0,"%s", _obj->var_cmd_vals);
		    else
			mvprintw(_t_msg_pos-THAPP_CMD_LINE, 0,"%s", THAPP_MSG_BLANK);
		    
		    if(++_obj->_msg_cnt >= _msg_cnt_max && _flg != 2)
			{
			    _obj->_msg_cnt = 0;
			    memset((void*) _obj->var_cmd_vals, 0, THAPP_DISP_BUFF_SZ);
			    mvprintw(_t_msg_pos-THAPP_CMD_LINE, 0, "%s", THAPP_MSG_BLANK);
			}
		}

	    /* Display special message */
	    if(_obj->var_disp_sp != 0)
		mvprintw(_t_msg_pos-THAPP_SP_LINE, 0,"%s", _obj->var_disp_sp);


	    /* Print the result  values */
	    mvprintw(_t_msg_pos+THAPP_VAL_LINE, 0,"%s", _obj->var_disp_vals);

	    /* Send message to the loging server */
	    THAPP_SEND_MSG(_obj, _obj->var_disp_vals, THAPP_DISP_BUFF_SZ);

	    refresh();

	    memset(_obj->var_disp_vals, 0, THAPP_DISP_BUFF_SZ);

	    /*
	     * Label to continue the loop.
	     */
	thapp_main_loop_cont:
	    usleep(_obj->var_sleep_time);
	}

    /* Check if the file pointer is open and close it */
    THAPP_CLOSE_LOG(_obj);
    
    _obj->var_run_flg = 0;
    return NULL;
}

/* Configuration loader */
static int _thapp_init_helper(thapp* obj)
{
    const char* _t_buff = NULL;
    config_setting_t* _setting = NULL;

    /* Initialise the configuration object */
    config_init(&obj->var_config);

    /* Try and read the file from the first path */
    while(1)
	{
	    if(config_read_file(&obj->var_config, THAPP_CONFIG_PATH1) == CONFIG_TRUE)
		break;

	    if(config_read_file(&obj->var_config, THAPP_CONFIG_PATH2) == CONFIG_TRUE)
		break;

	    if(config_read_file(&obj->var_config, THAPP_CONFIG_PATH3) == CONFIG_TRUE)
		break;

	    /*
	     * If all the paths were exhausted, return the function
	     * indicating an error.
	     */
	    return -1;
	}

    /* Get sleep time */
    _setting = config_lookup(&obj->var_config, THAPP_SLEEP_KEY);
    if(_setting != NULL)
      obj->var_sleep_time = config_setting_get_int(_setting);


    /* Get applications sleep time */
    _setting = config_lookup(&obj->var_config, THAPP_SLEEP_KEY);
    if(_setting != NULL)
	{
	    /* Set sleep time as defined in the config file */
	    obj->var_sleep_time = config_setting_get_int(_setting);
	}

    /* Get server port number */
    _setting = config_lookup(&obj->var_config, THAPP_PORT_KEY);
    if(_setting != NULL)
	{
	    _t_buff = config_setting_get_string(_setting);
	    if(_t_buff)
		{
		    thcon_set_port_name(&obj->_var_con, _t_buff);
		}
	    else
		{
		    thcon_set_port_name(&obj->_var_con, THAPP_DEFAULT_PORT);
		}
	}

    /* Get server name */
    _setting = config_lookup(&obj->var_config, THAPP_SERVER_NAME_KEY);
    if(_setting != NULL)
	{
	    _t_buff = config_setting_get_string(_setting);
	    if(_t_buff)
		thcon_set_server_name(&obj->_var_con, _t_buff);
	}

    /* Get geolocation ip address */
    _setting = config_lookup(&obj->var_config, THAPP_GEOLOC_KEY);
    if(_setting != NULL)
	{
	    _t_buff = config_setting_get_string(_setting);
	    if(_t_buff)
		thcon_set_geo_ip(&obj->_var_con, _t_buff);
	}

    /* Get queue limit */
    _setting = config_lookup(&obj->var_config, THAPP_QUEUE_LIMIT_KEY);
    if(_setting != NULL)
      obj->var_queue_limit = (unsigned int) config_setting_get_int(_setting);

    /* Get secondary server port and address */
    if(obj->var_sec_con_init_flg)
	{
	    _setting = config_lookup(&obj->var_config, THAPP_LOG_URL_KEY);
	    if(_setting)
		{
		    _t_buff = config_setting_get_string(_setting);
		    if(_t_buff)
			thcon_set_server_name(&obj->_var_con_sec, _t_buff);
		}

	    _setting = config_lookup(&obj->var_config, THAPP_LOG_URL_PORT_KEY);
	    if(_setting)
		{
		    _t_buff = config_setting_get_string(_setting);
		    if(_t_buff)
			thcon_set_port_name(&obj->_var_con_sec, _t_buff);
		}

	    /* Start server and set the flag to indicate that thread needs joining */
	    pthread_create(&obj->_var_sec_thread, NULL, _thapp_start_secon, (void*) obj);
	    obj->var_sec_con_start_flg = 1;

	}

    return 0;
}

/* Delete helper for message queue */
static void _thapp_queue_del_helper(void* data)
{
    if(data)
	free(data);
    return;
}

/*
 * This method is called by the connection object when data is recieved.
 * The method adds data to the message queue. Messages received are in plain text.
 * Therefore, we encode the messages to default message struct.
 */
static int _thapp_con_recv_callback(void* obj, void* msg, size_t sz)
{
    struct thor_msg* _msg;
    thapp* _obj;

    /* Check for arguments */
    if(obj == NULL || msg == NULL || sz <= 0)
	return -1;

    /* Cast object to the correct pointer */
    _obj = (thapp*) obj;

    if(strlen((char*) msg) <= 1)
	return 0;

    /* Create memory */
    _msg = (struct thor_msg*) malloc(sizeof(struct thor_msg));

    /* decode message */
    thornifix_decode_msg((char*) msg, sz, _msg);

    /*
     * Lock mutex and add to the queue if the queue
     * count hasn't exceeded.
     */
    pthread_mutex_lock(&_obj->_var_mutex);
    if(gqueue_count(&_obj->_var_msg_queue) < _obj->var_queue_limit)
      gqueue_in(&_obj->_var_msg_queue, (void*) _msg);
    pthread_mutex_unlock(&_obj->_var_mutex);

    return 0;
}

/* Signal handler for the quit method */
static void _thapp_sig_handler(int signo)
{
    if(signo == SIGINT)
	_flg = 0;
    return;
}

/* Get command */
static char _thapp_get_cmd(void)
{
    int _cmd = 0;
    _cmd = getch();
    return (char) _cmd;
}

/* Open file based on time */
static void _thapp_open_log_file(thapp* obj)
{
    char _file_name[THAPP_DISP_BUFF_SZ];
    char _time_sig[THAPP_DISP_BUFF_SZ];
    time_t _tm;
    struct tm* _tm_info;

    /* Initialise buffers */
    memset(_file_name, 0, THAPP_DISP_BUFF_SZ);
    memset(_time_sig, 0, THAPP_DISP_BUFF_SZ);
    
    time(&_tm);
    _tm_info = localtime(&_tm);
    strftime(_time_sig, THAPP_DISP_BUFF_SZ, THAPP_DEFAULT_LOG_FILE_NAME, _tm_info);
    if(obj->var_job_num[0] != 0 && obj->var_tag_num[0] != 0)
	sprintf(_file_name, "%s-%s-%s", obj->var_job_num, obj->var_tag_num, _time_sig);
    else
	sprintf(_file_name, "%s", _time_sig);
    obj->var_def_log = fopen(_time_sig, "w+");
    return;
}

/* Starts the secondary connection in three attempts */
static void* _thapp_start_secon(void* obj)
{
    thapp* _obj;
    int _cnt;

    /* Cast object poitner */
    if(obj == NULL)
	return NULL;

    _obj = (thapp*) obj;
    _obj->_var_con_sec_flg = 1;
    while(thcon_start(&_obj->_var_con_sec))
	{
	    _obj->_var_con_sec_flg = 0;
	    if(++_cnt > 3)
		break;

	    _obj->_var_con_sec_flg =1;
	    sleep(THAPP_DEFAULT_WAIT_TIME);
	}
    
    return NULL;
}

/* Recieved messages from the secondary server */
static int _thapp_con_recv_url_callback(void* obj, void* msg, size_t sz)
{
    return 0;
}
