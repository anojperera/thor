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
static int _thsvy_sys_update_callback(thsys* obj, const float64* buff, const int sz);

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
    if(thsys_init(&obj->_var_sys, _thsvr_sys_interupt))
	{
	    thcon_delete(&obj->_var_con);
	    return -1;
	}

    /* Set external object pointer */
    thsys_set_external_obj(&obj->_var_sys, (void*) obj);
    obj->_var_sys.var_callback_update = _thsvy_sys_update_callback;
    
    obj->var_init_flg = 1;
    return 0;
}


/*
 * If the test is running, it stops and frees the resources
 * allocated to the connection and system objects.
 */
void thsvr_delete(thsys* obj)
{
    obj->_var_config = NULL;

    return;
}


/*
 * Start server component.
 */
int thsvr_start(thsvr* obj)
{
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

    /* Start system */
    if(thsys_start(&obj->_var_sys))
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
static int _thsvy_sys_update_callback(thsys* obj, const float64* buff, const int sz)
{
    struct thor_msg _msg;
    char _msg_buff[THORINIFIX_MSG_SZ];

    /* initialise message buffer size */
    thorinifix_init_msg(&_msg);
    thorinifix_init_msg(_msg_buff);
    
    /*
     * Using common message struct, copy ray message to the struct and encode it
     * before multi casting.
     */
    _msg._cmd = 0;
    _msg._ao0 = 0;
    _msg._ao1 = 0;
    _msg._ai0 = (double) buff[0];
    _msg._ai1 = (double) buff[1];
    _msg._ai2 = (double) buff[2];
    _msg._ai3 = (double) buff[3];
    _msg._ai4 = (double) buff[4];
    _msg._ai5 = (double) buff[5];
    _msg._ai6 = (double) buff[6];
    _msg._ai7 = (double) buff[7];
    _msg._ai8 = (double) buff[8];
    _msg._ai9 = (double) buff[9];
    _msg._ai10 = (double) buff[10];

    /* Digital read set to zero */
    _msg._di0 = 0.0;
    _msg_di1 = 0.0;

    /* encode message to string and multi cast */
    thornifix_encode_msg(&_msg, _msg_buff, THORINIFIX_MSG_SZ);
    thcon_multicast(&obj->_var_con, _msg_buff, THORINIFIX_MSG_SZ);

    return 0;
}
