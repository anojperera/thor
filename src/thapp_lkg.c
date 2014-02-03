/* Implementation of the damper leakage class */
#include <math.h>
#include <ncurses.h>
#include "thgsensor.h"
#include "thtmp.h"
#include "thsmsen.h"
#include "thapp_lkg.h"

/* Options */
#define THAPP_LKG_OPT1 "Enter Job Number: "
#define THAPP_LKG_OPT2 "Enter Tag Number: "
#define THAPP_LKG_OPT3 "Select Operation Mode: \n"	\
    "(0 - Manual)\n"					\
    "(1 - Static Pressure Based)\n"			\
    "(2 - Leakage Based)\n"				\
    "Mode: "
#define THAPP_LKG_OPT4 "Enter required static pressure: "
#define THAPP_LKG_OPT5 "Enter required leakage rate: "
#define THAPP_LKG_OPT6 "Select Orifice Plate Size: \n"	\
    "(0 - 20mm dia)\n"					\
    "(1 - 30mm dia)\n"					\
    "(2 - 40mm dia)\n"					\
    "(3 - 60mm dia)\n"					\
    "(4 - 80mm dia)\n"					\
    "(5 - 100mm dia)\n"					\
    "Size: "
#define THAPP_LKG_OPT7 "Enter product type: \n" \
    "(0 - Dampers)\n"				\
    "(1 - AHU)\n"				\
    "Product type: "
#define THAPP_LKG_OPT8 "Enter Width: "
#define THAPP_LKG_OPT9 "Enter Height: "
#define THAPP_LKG_OPT10 "Enter Depth: "

/* Control Keys */
#define THAPP_LKG_FAN_INCR_CODE 43							/* + */
#define THAPP_LKG_FAN_DECR_CODE 45							/* - */
#define THAPP_LKG_FAN_INCRF_CODE 42							/* * */
#define THAPP_LKG_FAN_DECRF_CODE 47							/* / */
#define THAPP_LKG_RAW_VALUES 82								/* R */

/* Configuration keys */
#define THAPP_LKG_SM_KEY "lkg"
#define THAPP_LKG_ST_KEY "st1"
#define THAPP_LKG_TMP_KEY "tp1"
#define THAPP_LKG_WAIT_EXT_KEY "ahu_calib_wait_ext"
#define THAPP_LKG_SETTLE_KEY "ahu_calib_settle_time"

#define THAPP_LKG_MAX_FAN_PER 99
#define THAPP_LKG_MIN_FAN_PER 0

#define THAPP_LKG_INCR_PER 5.0
#define THAPP_LKG_INCRF_PER 1.0
#define THAPP_LKG_MAX_OPT_MESSAGE_LINES 6

/* Callback methods */
static int _thapp_lkg_init(thapp* obj, void* self);
static int _thapp_lkg_start(thapp* obj, void* self);
static int _thapp_lkg_stop(thapp* obj, void* self);
static int _thapp_lkg_cmd(thapp* obj, void* self, char cmd);

/* Helper method for loading configuration settings */
static int _thapp_new_helper(thapp_lkg* obj);

/* Fan control method */
static int _thapp_fan_ctrl(thapp_lkg* obj, double incr, double* incr_val, int* per, int flg)


thapp* thapp_lkg_new(void)
{
    int i = 0;
    thapp_lkg* _obj;

    _obj = (thapp_lkg*) malloc(sizeof(thapp_lkg));
    _obj->var_init_flg = 0;

    if(_obj == NULL)
	return NULL;

    /* Call to initialise the parent object */
    if(thapp_init(&_obj->_var_parent))
	{
	    free(_obj);
	    return NULL;
	}

    /* Set child pointer of the parent object */
    _obj->_var_parent.var_child = (void*) _obj;

    /* Initialise function pointer array */
    THAPP_INIT_FPTR(_obj);

    /* Help new helper */
    if(_thapp_new_helper(_obj))
	{
	    thapp_delete(&_obj->_var_parent);
	    free(_obj);
	    return NULL;
	}

    /* Set function pointers of the parent object */
    thapp_set_init_ptr(_obj, _thapp_lkg_init);
    thapp_set_start_ptr(_obj, _thapp_lkg_start);
    thapp_set_stop_ptr(_obj, _thapp_lkg_stop);
    thapp_set_cmdhnd_ptr(_obj, _thapp_lkg_cmd);

    /* Initialise class variables */
    _obj->var_test_type = thapp_lkg_tst_man;
    _obj->var_prod_type = thapp_lkg_dmp;
    
    _obj->var_calib_wait_ext = 0;

    _obj->var_or_ix = 0;
    _obj->var_calib_flg = 0;

    _obj->var_raw_flg = 0;
    _obj->var_fan_pct = 0;
    _obj->var_raw_act_ptr = NULL;
    for(i=0; i<THAPP_LKG_BUFF; i++)
	_obj->var_fan_buff[i] = 0.0;

    _obj->_var_msg_addr[0] = &_obj->_var_parent._msg_buff._ai8_val;
    _obj->_var_msg_addr[1] = &_obj->_var_parent._msg_buff._ai9_val;
    _obj->_var_msg_addr[2] = &_obj->_var_parent._msg_buff._ai10_val;
    _obj->_var_msg_addr[3] = &_obj->_var_parent._msg_buff._ai11_val;

    _obj->_var_dp = 0.0;
    _obj->_var_ext_static = 0.0;
    _obj->_var_lkg = 0.0;
    _obj->_var_lkg_m2 = 0.0;
    _obj->_var_lkg_f700 = 0.0;

    /* Maximum values for auto modes */
    _obj->var_max_static = 0.0;
    _obj->var_max_leakage = 0.0;

    _obj->var_width = 0.0;
    _obj->var_height = 0.0;
    _obj->var_depth = 0.0;
    _obj->var_s_area = 0.0;

    return &_obj->_var_parent;
}

/* Destructor */
void thapp_lkg_delete(thapp_lkg* obj)
{
    if(obj == NULL)
	return;

    /* Delete sensors */
    thsmsen_delete(THOR_SMSEN(obj->_var_sm_sen));
    thgsensor_delete(THOR_GSEN(obj->_var_st_sen));

    thtmp_delete(THOR_GSEN(obj->_var_tp_sen));

    THAPP_INIT_FPTR(obj);
    obj->var_child = NULL;
    obj->var_raw_act_ptr = NULL;

    if(obj->var_init_flg)
	free(obj);
    return;
}

/*===========================================================================*/
/****************************** Private methods ******************************/

/*
 * Initialisation helper methods.
 * This method also creates sensros.
 */
static int _thapp_new_helper(thapp_lkg* obj)
{
    const config_setting_t* _setting;

    /* Initialise sensor variables */
    obj->_var_sm_sen = NULL;
    obj->_var_st_sen = NULL;
    obj->_var_tmp_sen = NULL;

    /* Get configuration sensor array for the smart sensor */
    _setting = config_lookup(&obj->_var_parent.var_config, THAPP_LKG_KEY);
    if(_setting)
	obj->_var_sm_sen = thsmsen_new(NULL, _setting);

    /* Set the raw value */
    thsmsen_get_raw_value_ptr(THOR_SMSEN(obj->_var_sm_sen), obj->var_raw_act_val);

    /* Create static sensor */
    obj->_var_st_sen = thgsensor_new(NULL, obj);
    if(obj->_var_st_sen)
	{
	    /* Get static sensor data */
	    _setting = config_lookup(&obj->_var_parent.var_config, THAPP_LKG_ST_KEY);

	    if(_setting)
		thsen_set_config(obj->_var_st_sen, _setting);
	}

    /* Create temperature sensor */
    obj->_var_tmp_sen = thgsensor_new(NULL, obj);
    if(obj->_var_tmp_sen)
	{
	    _setting = config_lookup(&obj->_var_parent.var_config, THAPP_LKG_TMP_KEY);

	    if(_setting)
		thsen_set_config(obj->_var_st_sen, _setting);
	}


    /* Get extra wait time during calibration */
    _setting = config_lookup(&obj->_var_parent.var_config, THAPP_LKG_WAIT_EXT_KEY);
    if(_setting)
	obj->var_calib_wait_ext = config_setting_set_int(_setting);

    return 0;
}

/*
 * Initialise method is called by the parent class before the main loop starts.
 * Here we set the raw value pointers to respective sensors and smart sensor.
 * Fan control voltages are generated as well.
 */
static int _thapp_lkg_init(thapp* obj, void* self)
{
    int i;

    thapp_lkg* _obj;

    if(self == NULL)
	return -1;

    /* Cast self pointer */
    _obj = (thapp_lkg*) self;

    /* Generate fan control voltages */
    for(i=0; i<THAPP_LKG_BUFF; i++)
	_obj->var_fan_buff[i] = sin(M_PI*(double)i/THAPP_LKG_BUFF)*96.0;

    /* Set temperature sensor raw value */
    thgsens_set_value_ptr(THOR_GSEN(_obj->_var_tmp_sen), &obj->_msg_buff._ai0_val);

    /* Set statuc sensor raw value */
    thgsens_set_value_ptr(THOR_GSEN(_obj->_var_st_sen), &obj->_msg_buff._ai3_val);

    /* Set value pointer for sensor array */
    thsmsen_set_value_array(THOR_SMSEN(_obj->_var_sm_sen),
			    _obj->_var_msg_addr[0],
			    THAPP_LKG_MAX_SM_SEN);

    return 0;

}

/* Start callback method */
static int _thapp_lkg_start(thapp* obj, void* self)
{
    thapp_lkg* _obj;
    char _scr_input_buff[THAPP_DISP_BUFF_SZ];

    /* Cast object pointer */
    if(self == NULL)
	return -1;
    _obj = (thapp_lkg*) self;


    /* Reset all sensors before start */
    thsen_reset_sensor(_obj->_var_sm_sen);
    thsen_reset_sensor(_obj->_var_st_sen);
    thsen_reset_sensor(_obj->_var_tmp_sen);

    _obj->_var_dp = 0.0;
    _obj->_var_ext_static = 0.0;
    _obj->_var_lkg = 0.0;
    _obj->_var_lkg_m2 = 0.0;
    _obj->_var_lkg_f700 = 0.0;    

    _obj->var_max_static = 0.0;
    _obj->var_max_leakage = 0.0;

    _obj->var_width = 0.0;
    _obj->var_height = 0.0;
    _obj->var_depth = 0.0;
    _obj->var_s_area = 0.0;
    /*
     * If the app is not running in headless mode, query for
     * other options.
     */
    if(obj->var_op_mode != thapp_headless)
	{
	    /* Get job number and tag number */
	    printw(THAPP_LKG_OPT1);
	    refresh();
	    getnstr(obj->var_job_num, THAPP_DISP_BUFF_SZ-1);

	    printw(THAPP_LKG_OPT2);
	    refresh();
	    getnstr(obj->var_tag_num, THAPP_DISP_BUFF_SZ-1);
	    clear();

	    /* Display option to select mode */
	    THAPP_DISP_MESG(THAPP_LKG_OPT3, _scr_input_buff);
	    
	    _obj->var_test_type = atoi(_scr_input_buff);

	    
	    /* Provide extra options depending on the selected mode */
	    switch(_obj->var_test_type)
		{
		case thapp_lkg_tst_static:
		    /* Display message to obtain the maximum static pressure */
		    THAPP_DISP_MESG(THAPP_LKG_OPT4, _scr_input_buff);
		    _obj->var_max_static = atof(_scr_input_buff);
		    break;
		    
		case thapp_lkg_tst_leak:
		    THAPP_DISP_MESG(THAPP_LKG_OPT5, _scr_input_buff);
		    _obj->var_max_leakage = atof(_scr_input_buff);
		    break;
		    
		default:
		    /*
		     * If all the options were exhausted force manual
		     * control.
		     */
		    _obj->var_test_type = thapp_lkg_tst_man;
		}

	    /*
	     * Select orifice plate type.
	     * This may be replaced by venturi meter.
	     */
	    THAPP_DISP_MESG(THAPP_LKG_OPT6, _scr_input_buff);
	    _obj->var_or_ix = atoi(_scr_input_buff);
	    
	    

	    /* Get product type */
	    THAPP_DISP_MESG(THAPP_LKG_OPT7, _scr_input_buff);
	    _obj->var_prod_type = atoi(_scr_input_buff);
	    if(_obj->var_prod_type != thapp_lkg_dmp && _obj->var_prod_type !=  thapp_lkg_ahu)
		_obj->var_prod_type = thapp_lkg_dmp;

	    /*
	     * Get product dimensions.
	     */
	    /* Get width */
	    THAPP_DISP_MESG(THAPP_LKG_OPT8, _scr_input_buff);
	    _obj->var_width = atof(_scr_input_buff);

	    /* Get height */
	    THAPP_DISP_MESG(THAPP_LKG_OPT9, _scr_input_buff);
	    _obj->var_height = atof(_scr_input_buff);

	    /* Get depth */
	    THAPP_DISP_MESG(THAPP_LKG_OPT10, _scr_input_buff);
	    _obj->var_depth = atof(_scr_input_buff);
	    
	}

    obj->var_max_opt_rows = THAPP_LKG_MAX_OPT_MESSAGE_LINES;
    if(_obj->var_prod_type == thapp_lkg_ahu)
	{
	    sprintf(_obj->_var_parent.var_disp_header,
		    "\n\t"
		    "DP\t"
		    "ST\t"
		    "LKG\t"
		    "F700\t"
		    "TMP\r");

	    /* Calculate surface area */
	    _obj->var_s_area = _obj->var_height * _obj->var_depth * 2 +
		_obj->var_width * _obj->var_height * 2 +
		_obj->var_depth * _obj->var_width * 2;
	}
    else
	{
	    sprintf(_obj->_var_parent.var_disp_header,
		    "\n\t"
		    "DP\t"
		    "ST\t"
		    "LKG\t"
		    "LKG_m2\t"
		    "TMP\r");

	    _obj->var_s_area = _obj->var_width * _obj->var_height;
	}

    /* Convert to m^2 */
    _obj->var_s_area /= pow(10, -6);
    
    return 0;
}


static int _thapp_lkg_stop(thapp* obj, void* self)
{
    return 0;
}

/*
 * Command handler.
 * This is where we handle all data processing from sensors.
 * Always return true if the program were to continue.
 */
static int _thapp_lkg_cmd(thapp* obj, void* self, char cmd)
{
    int _rt_val;
    thapp_lkg* _obj;

    _rt_val = 1;


    if(self == NULL)
	return _rt_val;

    /* Cast object to the correct pointer */
    _obj = (thapp_lkg*) self;
    switch(cmd)
	{
	case THAPP_LKG_FAN_INCR_CODE:
	    _thapp_fan_ctrl(_obj, THAPP_LKG_INCR_PER, NULL, NULL, 0);
	    break;
	case THAPP_LKG_FAN_DECR_CODE:
	    _thapp_fan_ctrl(_obj, -1*THAPP_LKG_INCR_PER, NULL, NULL, 0);
	    break;
	case THAPP_LKG_FAN_INCRF_CODE:
	    _thapp_fan_ctrl(_obj, THAPP_LKG_INCRF_PER, NULL, NULL, 0);
	    break;
	case THAPP_LKG_FAN_DECRF_CODE:
	    _thapp_fan_ctrl(_obj, -1*THAPP_LKG_INCRF_PER, NULL, NULL, 0);
	    break;
	    
	default:
	    break;
	}


    /* Get values */
   _obj->_var_dp = thsen_get_value(_obj->_var_sm_sen);
   _obj->_var_st_sen = thsen_get_value(_obj->_var_st_sen);
   
    /* Calculate leakage */
    switch(_obj->var_or_ix)
	{
	case thapp_lkg_20:
	    _obj->_var_lkg = 0.0;
	    break;
	case thapp_lkg_30:
	    _obj->_var_lkg = 0.0;
	    break;
	case thapp_lkg_40:
	    _obj->_var_lkg = 0.0;
	    break;
	case thapp_lkg_60:
	    _obj->_var_lkg = 0.0;
	    break;
	case thapp_lkg_80:
	    _obj->_var_lkg = 0.0;
	    break;
	case thapp_lkg_100:
	    _obj->_var_lkg = 0.0;
	    break;
	default:
	    _obj->_var_lkg = 0.0;
	    break;
	}

    /* Calculate leakge per unit area */
    _obj->_var_lkg_m2 = _obj->_var_lkg / _obj->var_s_area;

    /* Temporary message buffer */
    memset(_obj->_var_parent.var_disp_vals, 0, THAPP_DISP_BUFF_SZ);
    sprintf(_obj->_var_parent.var_disp_vals,
	    "\t"
	    "%.2f\t"
	    "%.2f\t"
	    "%.2f\t"
	    "%.2f\t"
	    "%.2f\t\r",
	    _obj->var_raw_flg? *_obj->var_raw_act_ptr : _obj->_var_dp,
	    _obj->var_raw_flg? _obj->_var_parent._msg_buff._ai3_val : _obj->_var_ext_static,
	    _obj->var_raw_flg? 0.0 : _obj->_var_lkg,
	    _obj->var_raw_flg? 0.0 : _obj->_var_lkg_m2,
	    _obj->var_raw_flg? _obj->_var_parent._msg_buff._ai0_val : thsen_get_value(_obj->_var_tmp_sen));
    
    return _rt_val;
}

/*
 * Controls the actuator position and sends the control signal to the
 * server. This method handles both increment and decremnt methods.
 * Uses percentages to calculate the voltage to be sent.
 */
static int _thapp_fan_ctrl(thapp_ahu* obj, double incr, double* incr_val, int* per, int flg)
{
    struct thor_msg _msg;						/* message */
    char _msg_buff[THORNIFIX_MSG_BUFF_SZ];
    double _val;

    /* Increment the value temporarily. */
    if(incr_val != NULL)
	    obj->var_fan_pct = *incr_val;
    else
	obj->var_fan_pct += incr;

    /* Check if its within bounds. */
    if(obj->var_fan_pct > THAPP_AHU_MAX_ACT_PER || obj->var_fan_pct < THAPP_AHU_MIN_ACT_PER)
	{
	    /* Reset the value to the original and set the external value */
	    obj->var_fan_pct -= incr;
	    if(per != NULL)
		*per = obj->var_fan_pct;
	    return 0;
	}

    
    if(per != NULL)
	*per = (int) obj->var_fan_pct;

    /* Update command message */
    if(flg == 0)
	{
	    sprintf(obj->_var_parent.var_cmd_vals,
		    "<==================== Actuator - %i%% ====================>",
		    (int) obj->var_fan_pct);
	}

    _val = obj->var_fan_pct;
    /* memset buffer */
    thorinifix_init_msg(&_msg);
    thorinifix_init_msg(&_msg_buff);

    _msg._ao0_val = 0.0;
    _msg._ao1_val = 9.95 * _val / 100;

    /* Encode the message and send to the server */
    thornifix_encode_msg(&_msg, _msg_buff, THORNIFIX_MSG_BUFF_SZ);
    thcon_send_info(thapp_get_con_obj_ptr(&obj->_var_parent), (void*) &_msg_buff, THORNIFIX_MSG_BUFF_SZ);    
    return 0;
}