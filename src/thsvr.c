/*
 * Implementation of the server object.
 */
#include "thornifix.h"
#include "thsvr.h"

/*
 * Below are tokens for keys required to read configuration
 * file.
 */
#define THSVR_GEO_URL "mygeo_location"
#define THSVR_ADMIN1_URL "prime_admin_url"
#define THSVR_ADMIN2_URL "http://www.valyria.co.uk:/8080/dieties/"
#define THSVR_COM_PORT "main_con_port"
#define THSVR_DEF_COM_PORT "11000"
#define THSVR_DEF_TIMEOUT "def_time_out"

/*
 * Macro for copying admin url to the internal buffers.
 */
#define _thsvr_copy_admin1(obj, val, sz)	\
    strncpy((obj)->var_admin1_url, val, sz-1);	\
    (obj)->var_admin1_url[sz-1] = '\0'
#define _thsvr_copy_admin2(obj, val, sz)	\
    strncpy((obj)->var_admin2_url, val, sz-1);	\
    (obj)->var_admin2_url[sz-1] = '\0'


/* Interupt handlers.*/
static int _thsvr_sys_interupt_callback(thsys* obj, void* self);
static int _thsvy_sys_update_callback(thsys* obj, void* self, const float64* buff, const int sz);
static int _thsvr_con_recv_callback(void* obj, void* msg, size_t sz);
static int _thsvr_con_made_callback(void* obj, void* con);
static int _thsvr_con_closed_callback(void* obj, void* con, int fd);
/*
 * Initialise the server component and get configuration settings
 * for the admin url etc.
 */
int thsvr_init(thsvr* obj, const config_t* config)
{
    /* check for arguments */
    if(obj == NULL || config == NULL)
	return -1;

    /* set and initialise internal variables */
    obj->_var_config = config;

    /* Initialise buffers */
    memset((void*) obj->var_admin1_url, 0, THCON_URL_BUFF_SZ);
    memset((void*) obj->var_admin2_url, 0, THCON_URL_BUFF_SZ);
    
    /* Initialise connection object */
    if(thcon_init(&obj->_var_con, thcon_mode_server))
	return -1;

    /* initialise system object */
    if(thsys_init(&obj->_var_sys, _thsvr_sys_interupt_callback))
	{
	    thcon_delete(&obj->_var_con);
	    return -1;
	}

    /* Set external object pointer */
    thsys_set_external_obj(&obj->_var_sys, (void*) obj);
    thcon_set_ext_obj(&obj->_var_con, (void*) obj);
    
    thsys_set_sample_rate(&obj->_var_sys, 4);

    /* Set callback methods */
    obj->_var_sys.var_callback_update = _thsvy_sys_update_callback;
    obj->_var_con._thcon_recv_callback = _thsvr_con_recv_callback;
    obj->_var_con._thcon_conn_made = _thsvr_con_made_callback;
    obj->_thcon_conn_closed = _thsvr_con_closed_callback;    
    
    obj->var_init_flg = 1;
    return 0;
}


/*
 * If the test is running, it stops and frees the resources
 * allocated to the connection and system objects.
 */
void thsvr_delete(thsvr* obj)
{
    /* Check for object */
    if(obj == NULL)
	return;
    
    obj->_var_config = NULL;
    /*
     * Call stop method to as a safe option
     */
    thsvr_stop(obj);

    /* Delete both connection and the system object */
    thcon_delete(&obj->_var_con);
    thsys_delete(&obj->_var_sys);
    return;
}


/*
 * Start server component.
 */
int thsvr_start(thsvr* obj)
{
    int _time_out = 0;
    const char* _t_buff;    
    struct config_setting_t* _setting;
    
    /* check for object */
    if(obj == NULL || obj->var_init_flg != 1)
	return -1;

    /* Read the configuration settings */
    /* Get geolocation URL */
    _setting = config_lookup(obj->_var_config, THSVR_GEO_URL);
    if(_setting)
	{
	    _t_buff = config_setting_get_string(_setting);
	    if(_t_buff)
		thcon_set_geo_ip(&obj->_var_con, _t_buff);
	}

    
    _setting = config_lookup(obj->_var_config, THSVR_ADMIN1_URL);
    if(_setting)
	{
	    _t_buff = config_setting_get_string(_setting);
	    if(_t_buff)
		_thsvr_copy_admin1(obj, _t_buff, THCON_URL_BUFF_SZ);
	}

    
    _setting = config_lookup(obj->_var_config, THSVR_ADMIN2_URL);
    if(_setting)
	{
	    _t_buff = config_setting_get_string(_setting);
	    if(_t_buff)
		_thsvr_copy_admin2(obj, _t_buff, THCON_URL_BUFF_SZ);
	}

    _setting = config_lookup(obj->_var_config, THSVR_COM_PORT);
    _t_buff = NULL;
    if(_setting)
    	{
    	    _t_buff = config_setting_get_string(_setting);
    	    if(_t_buff)
    		thcon_set_port_name(&obj->_var_con, _t_buff);
    	}
    else
	thcon_set_port_name(&obj->_var_con, THSVR_DEF_COM_PORT);

    /* Get time out for the admin query and set in connection object */
    _setting = config_lookup(obj->_var_config, THSVR_DEF_TIMEOUT);
    if(_setting)
	_time_out = config_setting_get_int(_setting);
    thcon_set_timeout(&obj->_var_con, _time_out);
    
    /*
     * Reset connection info struct.
     */
    thcon_reset_my_info(&obj->_var_con);

    /* get geo location */
    thcon_get_my_geo(&obj->_var_con);

    /*
     * Contact admins and send info on location.
     */
    if(obj->var_admin1_url[0] != 0)
	thcon_contact_admin(&obj->_var_con, obj->var_admin1_url);
    if(obj->var_admin2_url[0] != 0)
	thcon_contact_admin(&obj->_var_con, obj->var_admin2_url);


    /*
     * Start connection server first. This is important as soon as the
     * system is running, callback methods are called from the system object
     * on return of sensor data. This needs to be written to all connected app
     * clients.
     */
    if(thcon_start(&obj->_var_con))
	return -1;

    return 0;
}

/*
 * Stop server component.
 */
int thsvr_stop(thsvr* obj)
{
    /* give command to stop the system */
    thsys_stop(&obj->_var_sys);
    thcon_stop(&obj->_var_con);

    return 0;
}


/*===========================================================================*/
/******************************* Private Methods *****************************/

/*
 * Update callback method is fired from system object when sensor data is read.
 * We call connection objects multicast method to relay messages to the connected
 * app clients.
 */
static int _thsvy_sys_update_callback(thsys* obj, void* self, const float64* buff, const int sz)
{
    struct thor_msg _msg;
    char _msg_buff[THORINIFIX_MSG_SZ];
    thsvr* _obj;

    if(self == NULL || buff == NULL || sz <= 0)
	return -1;

    /* Cast self object pointer to the correct type */
    _obj = (thsvr*) self;
    
    /* initialise message buffer size */
    thorinifix_init_msg(&_msg);
    thorinifix_init_msg(_msg_buff);
    
    /*
     * Using common message struct, copy ray message to the struct and encode it
     * before multi casting.
     */
    _msg._cmd = 0;
    _msg._ao0_val = 0.0;
    _msg._ao1_val = 0.0;
    _msg._ai0_val = (double) buff[0];
    _msg._ai1_val = (double) buff[1];
    _msg._ai2_val = (double) buff[2];
    _msg._ai3_val = (double) buff[3];
    _msg._ai4_val = (double) buff[4];
    _msg._ai5_val = (double) buff[5];
    _msg._ai6_val = (double) buff[6];
    _msg._ai7_val = (double) buff[7];
    _msg._ai8_val = (double) buff[8];
    _msg._ai9_val = (double) buff[9];
    _msg._ai10_val = (double) buff[10];
    _msg._ai11_val = (double) buff[11];
    _msg._ai12_val = (double) buff[12];

    /* Digital read set to zero */
    _msg._di0_val = 0.0;
    _msg._di1_val = 0.0;

    /* encode message to string and multi cast */
    thornifix_encode_msg(&_msg, _msg_buff, THORINIFIX_MSG_SZ);
    thcon_multicast(&_obj->_var_con, _msg_buff, THORINIFIX_MSG_SZ);

    return 0;
}

/*
 * Interupt handler. This callback method is fired at a defined rate by the sys object.
 * Any interupt processing can be done here.
 */
static int _thsvr_sys_interupt_callback(thsys* obj, void* self)
{
    return 0;
}

/*
 * This callback method handles, messages from the client connections.
 * When a client application sends commands to the server, this method
 * shall be called from thcon. We shall parse the message and pass it to
 * thsys object.
 */
static int _thsvr_con_recv_callback(void* obj, void* msg, size_t sz)
{
    float64 _ao_buff[THSYS_NUM_AO_CHANNELS];					/* buffer to hold analogue out */
    struct thor_msg _msg;							/* message struct */
    thsvr* _obj;								/* self */

    /* Check for arguments */
    if(obj == NULL || msg == NULL || sz <= 0)
	return -1;

    _obj = (thsvr*) obj;

    _ao_buff[0] = 0.0;
    _ao_buff[1] = 0.0;

    /* Initialise message struct */
    thorinifix_init_msg(&_msg);
    
    /* Decode the message */
    if(thornifix_decode_msg((const char*) msg, sz, &_msg))
	return -1;

    /* Check command here */

    /*--------------------*/

    _ao_buff[0] = _msg._ao0_val;
    _ao_buff[1] = _msg._ao1_val;

    /* Call write method of system object */
    thsys_set_write_buff(&_obj->_var_sys, _ao_buff, THSYS_NUM_AO_CHANNELS);

    return 0;
}

/*
 * This callback is fired when a connection is made.
 * When the fist connection is made, we start the sys object.
 */
static int _thsvr_con_made_callback(void* obj, void* con)
{
    thsvr* _obj;

    if(obj == NULL)
       return -1;

    /* Case object to the correct type */
    _obj = (thsvr*) obj;

    thsys_start(&_obj->_var_sys);
    return 0;
}

/*
 * Callback method to indicate connection was closed.
 */
static int _thsvr_con_closed_callback(void* obj, void* con, int fd)
{
    char _err_msg[THOR_BUFF_SZ];
    thsvr* _obj;
    
    if(obj == NULL)
	return -1;

    memset((void*) _err_msg, 0, THOR_BUFF_SZ);
    
    /* Cast object to the correct pointer */
    _obj = (thsvr*) obj;

    /* Log message to indicate connection was closed */
    sprintf(_err_msg, "Connection Closed with FD: %i", fd);
    THOR_LOG_ERROR(_err_msg);

    /*
     * Call stop method of the system object.
     * The system object is aware of the number of connections,
     * therefore, if all connections were closed, the system
     * object shall be stopped.
     */
    thsys_stop(_obj->_var_sys);

    return 0;
}
