/* Implementation of the air handling unit test program. */
#include <math.h>
#include <string.h>
#include <ncurses.h>
#include "thgsensor.h"
#include "thvsen.h"
#include "thsmsen.h"
#include "thapp_ahu.h"

/* Options */
#define THAPP_AHU_OPT_1 "Enter Job Number: "
#define THAPP_AHU_OPT_2 "Enter Tag Number: "
#define THAPP_AHU_OPT1 "Enter Duct Diameter (200/300/600/1120): "
#define THAPP_AHU_OPT2 "Enter number of sensors (4/2): "
#define THAPP_AHU_OPT3 "Enter external static pressure: "
#define THAPP_AHU_OPT4 "Add pulley ratio for motor speed (Y/n): "
#define THAPP_AHU_OPT5 "Fan pulley diameter: "
#define THAPP_AHU_OPT6 "Motor pulley diameter: "
#define THAPP_AHU_OPT7 "Pulley Ratio: "

#define THAPP_AHU_OPT8 "<==================== Calibration in Progress - ACT %i%% ====================>"

/* Settting keys */
#define THAPP_AHU_KEY "ahu"
#define THAPP_TMP_KEY "tp1"
#define THAPP_SPD_KEY "sp1"
#define THAPP_STT_KEY "st1"
#define THAPP_SMT_KEY "lkg"


/* Control Keys */
#define THAPP_AHU_ACT_INCR_CODE 43							/* + */
#define THAPP_AHU_ACT_DECR_CODE 45							/* - */
#define THAPP_AHU_ACT_INCRF_CODE 42							/* * */
#define THAPP_AHU_ACT_DECRF_CODE 47							/* / */
#define THAPP_AHU_YES_CODE 121								/* y */
#define THAPP_AHU_YES2_CODE 89								/* Y */
#define THAPP_AHU_CALIBRATION_CODE 67							/* c */
#define THAPP_AHU_RAW_VALUES 82								/* R */

#define THAPP_AHU_MAX_ACT_PER 99
#define THAPP_AHU_MIN_ACT_PER 0

#define THAPP_AHU_INCR_PER 5
#define THAPP_AHU_INCRF_PER 1
#define THAPP_MAX_OPT_MESSAGE_LINES 6
/* Callback methods */
static int _thapp_ahu_init(thapp* obj, void* self);
static int _thapp_ahu_start(thapp* obj, void* self);
static int _thapp_ahu_stop(thapp* obj, void* self);
static int _thapp_cmd(thapp* obj, void* self, char cmd);
static int _thapp_act_ctrl(thapp_ahu* obj, int incr, int* per, int flg);

/* Helper method for loading configuration settings */
static int _thapp_new_helper(thapp_ahu* obj);

/* Constructor */
thapp* thapp_ahu_new(void)
{
    int i=0;
    thapp_ahu* _obj;

    _obj = (thapp_ahu*) malloc(sizeof(thapp_ahu));

    if(_obj == NULL)
	return NULL;

    /* Call to initalise the parent object */
    if(thapp_init(&_obj->_var_parent))
	{
	    free(_obj);
	    return NULL;
	}

    /* Set child pointer of parent objecgt */
    _obj->_var_parent.var_child = (void*) _obj;

    /* Initialise function pointer array */
    THAPP_INIT_FPTR(_obj);

    /* Set function pointers of parent object */
    thapp_set_init_ptr(_obj, _thapp_ahu_init);
    thapp_set_start_ptr(_obj, _thapp_ahu_start);
    thapp_set_stop_ptr(_obj, _thapp_ahu_stop);
    thapp_set_cmdhnd_ptr(_obj, _thapp_cmd);

    /* Initialise dp value pointer */
    _obj->_var_dp_val_ptr = NULL;
    
    /* Load configurations and initialise sensors */
    if(_thapp_new_helper(_obj))
	{
	    /* Delete parent */
	    thapp_delete(&_obj->_var_parent);
	    free(_obj);
	    return NULL;
	}

    /* Set addr pointers of message struct */
    _obj->_var_msg_addr[0] = &_obj->_var_parent._msg_buff._ai4_val;
    _obj->_var_msg_addr[1] = &_obj->_var_parent._msg_buff._ai5_val;
    _obj->_var_msg_addr[2] = &_obj->_var_parent._msg_buff._ai6_val;
    _obj->_var_msg_addr[3] = &_obj->_var_parent._msg_buff._ai7_val;
    
    /* Set default running mode to manual */
    _obj->var_mode = 0;
    _obj->var_act_pct = 0;

    /* Initialise actuator buffer */
    for(; i<THAPP_AHU_DMP_BUFF; i++)
	_obj->var_dmp_buff[i] = 0.0;
    _obj->var_raw_flg = 0;
    _obj->var_calib_flg = 0;
    _obj->var_def_static = 0.0;
    _obj->var_duct_dia = 0.0;
    _obj->var_duct_vel = 0.0;
    _obj->var_duct_vol = 0.0;
    _obj->var_duct_loss = 0.0;
    _obj->var_t_ext_st = 0.0;
    _obj->var_fm_ratio = 1.0;


    _obj->var_child = NULL;

    /* Return thapp object */
    return &_obj->_var_parent;
}

/* Desructor */
void thapp_ahu_delete(thapp_ahu* obj)
{
    int i;
    /* Check object pointer */
    if(obj == NULL)
	return;

    /* Free dp value pinters if set. */
    if(obj->_var_dp_val_ptr)
	{
	    for(i=0; i<thvsen_get_num_sensors(THOR_VSEN(obj->_var_vsen)); i++)
		obj->_var_dp_val_ptr[i] = NULL;
	    free(obj->_var_dp_val_ptr);
	}
    
    /* Delete parent object */
    thapp_delete(&obj->_var_parent);

    /* Remove velocity sensor */
    thvsen_delete(THOR_VSEN(obj->_var_vsen));

    /* Remove temperature sensor */
    thgsensor_delete(THOR_GSEN(obj->_var_tp_sen));
    thgsensor_delete(THOR_GSEN(obj->_var_sp_sen));
    thgsensor_delete(THOR_GSEN(obj->_var_st_sen));

    /* Delete smart sensor */
    thsmsen_delete(THOR_SMSEN(obj->_var_stm_sen));

    /* Set all pointers to NULL */
    obj->_var_vsen = NULL;
    obj->_var_tp_sen = NULL;
    obj->_var_sp_sen = NULL;
    obj->_var_st_sen = NULL;

    obj->_var_stm_sen = NULL;

    THAPP_INIT_FPTR(obj);
    obj->var_child = NULL;

    free(obj);
    return;
}


/*===========================================================================*/
/****************************** Private methods ******************************/

/*
 * Initialisation helper methods.
 * This method also creates sensros.
 */

static int _thapp_new_helper(thapp_ahu* obj)
{
    int _num = 0;
    const config_setting_t* _setting;

    /*
     * Initialise sensor variables of the class to NULL first
     */
    obj->_var_vsen = NULL;
    obj->_var_tp_sen = NULL;
    obj->_var_sp_sen = NULL;
    obj->_var_st_sen = NULL;
    obj->_var_stm_sen = NULL;

    /* Get Configuration settings for the velocity sensor array */
    _setting = config_lookup(&obj->_var_parent.var_config, THAPP_AHU_KEY);
    /* Get number of elements */
    if(_setting)
	{
	    /* Count the number of sensors available */
	    _num = config_setting_length(_setting);
	    obj->_var_vsen = thvsen_new(NULL, _setting, _num);
	   
	    /* Check if sensor object was created */
	    if(obj->_var_vsen == NULL)
		return -1;

	    /* Create dp value pointer array */
	    obj->_var_dp_val_ptr = (double**) calloc(_num, sizeof(double*));
	    thvsen_get_dp_values(THOR_VSEN(obj->_var_vsen), obj->_var_dp_val_ptr, &_num);
	}
    else
	{
	    /*
	     * If Errors occured, we shall quit the program.
	     */
	    return -1;
	}
    if(_setting)
	thsen_set_config(obj->_var_vsen, _setting);

    /* Create temperature sensor */
    obj->_var_tp_sen = thgsensor_new(NULL, obj);
    if(obj->_var_tp_sen)
	{
	    /* Get temperature sensor settings */
	    _setting = config_lookup(&obj->_var_parent.var_config, THAPP_TMP_KEY);

	    if(_setting)
		thsen_set_config(obj->_var_tp_sen, _setting);
	}

    /* Create speed sensor */
    obj->_var_sp_sen = thgsensor_new(NULL, obj);
    if(obj->_var_sp_sen)
	{
	    /* Get speed sensor data */
	    _setting = config_lookup(&obj->_var_parent.var_config, THAPP_SPD_KEY);
	    if(_setting)
		thsen_set_config(obj->_var_sp_sen, _setting);
	}

    /* Create static sensor */
    obj->_var_st_sen = thgsensor_new(NULL, obj);
    if(obj->_var_st_sen)
	{
	    /* Get static sensor data */
	    _setting = config_lookup(&obj->_var_parent.var_config, THAPP_STT_KEY);
	    if(_setting)
		thsen_set_config(obj->_var_st_sen, _setting);
	}

    /* Create smart sensor for measuring the static losses in test setup */

    /* Get smart sensor data */
    _setting = config_lookup(&obj->_var_parent.var_config, THAPP_SMT_KEY);
    if(_setting)
	obj->_var_stm_sen = thsmsen_new(NULL, _setting);

    return 0;
}


/* Start callback */
static int _thapp_ahu_start(thapp* obj, void* self)
{
    unsigned int _num_sensors=0;
    char _def_flg=0;
    float _f_dia=0.0, _m_dia=0.0;
    double _ratio=0.0;
    char _scr_input_buff[THAPP_DISP_BUFF_SZ];    
    thapp_ahu* _obj;
    int _pos = 0;

    if(self == NULL)
	return -1;
    _obj = (thapp_ahu*) self;
    
    /* Reset all sensors before start */
    thsen_reset_sensor(_obj->_var_vsen);
    thsen_reset_sensor(_obj->_var_tp_sen);
    thsen_reset_sensor(_obj->_var_sp_sen);
    thsen_reset_sensor(_obj->_var_st_sen);
    thsen_reset_sensor(_obj->_var_stm_sen);

    /*
     * If the app is not running in headless mode, query for
     * other options.
     */
    if(obj->var_op_mode != thapp_headless)
	{
	    /* Get job number and tag number */
	    printw(THAPP_AHU_OPT_1);
	    refresh();
	    getnstr(obj->var_job_num, THAPP_DISP_BUFF_SZ-1);

	    printw(THAPP_AHU_OPT_2);
	    refresh();
	    getnstr(obj->var_tag_num, THAPP_DISP_BUFF_SZ-1);
	    
	    memset(_scr_input_buff, 0, THAPP_DISP_BUFF_SZ);
	    printw(THAPP_AHU_OPT1);
	    refresh();
	    getnstr(_scr_input_buff, THAPP_DISP_BUFF_SZ-1);

	    _obj->var_duct_dia = atof(_scr_input_buff);
	    _pos += sprintf(obj->var_disp_opts, "%s%i\n", THAPP_AHU_OPT1, (int) _obj->var_duct_dia);
	    clear();

    	    memset(_scr_input_buff, 0, THAPP_DISP_BUFF_SZ);
	    printw(THAPP_AHU_OPT2);
	    refresh();
	    getnstr(_scr_input_buff, THAPP_DISP_BUFF_SZ-1);

	    _num_sensors = atoi(_scr_input_buff);
	    _pos += sprintf(&obj->var_disp_opts[_pos], "%s%i\n", THAPP_AHU_OPT2, _num_sensors);
	    /* Adjust for actual number of sensors */
	    if(_num_sensors > 2)
		_num_sensors = 4;
	    else
		_num_sensors = 2;
	    
	    clear();

	    memset(_scr_input_buff, 0, THAPP_DISP_BUFF_SZ);
	    printw(THAPP_AHU_OPT3);
	    refresh();
	    getnstr(_scr_input_buff, THAPP_DISP_BUFF_SZ-1);

	    _obj->var_def_static = atof(_scr_input_buff);
	    clear();
	    _pos += sprintf(&obj->var_disp_opts[_pos], "%s%i\n", THAPP_AHU_OPT3, (int) _obj->var_def_static);
	    
	    printw(THAPP_AHU_OPT4);
	    refresh();
	    getnstr(_scr_input_buff, THAPP_DISP_BUFF_SZ-1);	    
	    _def_flg = _scr_input_buff[0];
	    clear();
	    
	    if(_def_flg == THAPP_AHU_YES_CODE || _def_flg == THAPP_AHU_YES2_CODE)
		{
		    memset(_scr_input_buff, 0, THAPP_DISP_BUFF_SZ);
		    printw(THAPP_AHU_OPT5);
		    refresh();
		    getnstr(_scr_input_buff, THAPP_DISP_BUFF_SZ-1);
		    
		    _f_dia = atof(_scr_input_buff);
		    clear();
		    _pos += sprintf(&obj->var_disp_opts[_pos], "%s%i\n", THAPP_AHU_OPT5, (int) _f_dia);
		    
    		    memset(_scr_input_buff, 0, THAPP_DISP_BUFF_SZ);
		    printw(THAPP_AHU_OPT6);
		    refresh();
		    getnstr(_scr_input_buff, THAPP_DISP_BUFF_SZ-1);
		    _m_dia = atof(_scr_input_buff);
		    clear();

		    _pos += sprintf(&obj->var_disp_opts[_pos], "%s%i\n", THAPP_AHU_OPT6, (int) _m_dia);
		    if(_m_dia > 0.0)
			_ratio = (double) _f_dia / _m_dia;

		    _pos += sprintf(&obj->var_disp_opts[_pos], "%s%.1f\n", THAPP_AHU_OPT7, _ratio);
		    _obj->var_fm_ratio = _ratio;
		    refresh();
		}
	}
    obj->var_max_opt_rows = THAPP_MAX_OPT_MESSAGE_LINES;
    thvsen_configure_sensors(THOR_VSEN(_obj->_var_vsen), _num_sensors);    

    /* Add header information */
    memset(_obj->_var_parent.var_disp_header, 0, THAPP_DISP_BUFF_SZ);
    sprintf(_obj->_var_parent.var_disp_header,
	    "\n"
	    "DP1\t"
	    "DP2\t"
	    "DP3\t"
	    "DP4\t"
	    "Vel\t"
	    "Vol\t"
	    "ST\t"
	    "F_SP\t"
	    "M_SP\t"
	    "TMP\r");
    return 0;
}


/* Stop called back */
static int _thapp_ahu_stop(thapp* obj, void* self)
{
    return 0;
}

/*
 * Command handler.
 * We always return a true if the program were to continue.
 */
static int _thapp_cmd(thapp* obj, void* self, char cmd)
{
#define THAPP_SEN_BUFF_SZ 4
    double _vel, _vol, _f_sp;
    thapp_ahu* _obj;
    int _rt_val, _act_per=0;
    int _cnt = 0;

    _vel = 0.0;
    _vol = 0.0;
    _f_sp = 0.0;
    _rt_val = 1;
    
    if(self == NULL)
	return _rt_val;

    _obj = (thapp_ahu*) self;
    switch(cmd)
	{
	case THAPP_AHU_ACT_INCR_CODE:
	    _thapp_act_ctrl(_obj, THAPP_AHU_INCR_PER, NULL, 0);
	    break;
	case THAPP_AHU_ACT_DECR_CODE:
	    _thapp_act_ctrl(_obj, -1*THAPP_AHU_INCR_PER, NULL, 0);	    
	    break;
	case THAPP_AHU_ACT_INCRF_CODE:
	    _thapp_act_ctrl(_obj, THAPP_AHU_INCRF_PER, NULL, 0);
	    break;
	case THAPP_AHU_ACT_DECRF_CODE:
	    _thapp_act_ctrl(_obj, -1*THAPP_AHU_INCRF_PER, NULL, 0);	    
	    break;
	case THAPP_AHU_YES_CODE:
	    break;
	case THAPP_AHU_YES2_CODE:
	    break;
	case THAPP_AHU_CALIBRATION_CODE:
	    if(!_obj->var_calib_flg)
		{
		    _obj->var_calib_flg = 1;
		    _cnt = 0;

		    /* Message to indicate calibration in progress */
		    memset(_obj->_var_parent.var_cmd_vals, 0, THAPP_DISP_BUFF_SZ);
		    sprintf(_obj->_var_parent.var_cmd_vals, THAPP_AHU_OPT8, 0);
			    
		}
	    
	    break;
	case THAPP_AHU_RAW_VALUES:
	    if(_obj->var_raw_flg > 0)
		_obj->var_raw_flg = 0;
	    else
		{
		    _obj->var_raw_flg = 1;

		    /* Message to indicate raw voltage values instead of physical values */
    		    memset(_obj->_var_parent.var_cmd_vals, 0, THAPP_DISP_BUFF_SZ);
		    sprintf(_obj->_var_parent.var_cmd_vals,
			    "<==================== Raw Voltage Values ====================>");
		}
	    break;
	default:
	    break;
	}

    /*
     * If the raw flag was set, continue and return flag 2 to
     * display message continuously. We increment the _msg_cnt of
     * parent class to flash the message instead of continuous display.
     */
    if(_obj->var_raw_flg || _obj->var_calib_flg)
	{
	    _rt_val = 2;
	    obj->_msg_cnt++;
	}
    else
	_rt_val = 1;

    /*
     * Handle calibration
     */
    if(_obj->var_calib_flg)
	{
	    _thapp_act_ctrl(_obj, _obj->var_dmp_buff[_cnt], &_act_per, 1);
	    sprintf(_obj->_var_parent.var_cmd_vals, THAPP_AHU_OPT8, _act_per);
	    if(++_cnt >= THAPP_AHU_DMP_BUFF)
		{
		    _obj->var_calib_flg = 0;
		    _cnt = 0;
		}
	}
	

    /* Get Values */
    _vel = thsen_get_value(_obj->_var_vsen);
    if(_obj->var_duct_dia > 0.0)
	{
	    _vol = _vel * M_PI* pow((_obj->var_duct_dia/2), 2);
	    _vol /= 1000000;
	}

    _f_sp = thsen_get_value(_obj->_var_sp_sen);
    /* Temporary message buffer */
    memset(_obj->_var_parent.var_disp_vals, 0, THAPP_DISP_BUFF_SZ);
    sprintf(_obj->_var_parent.var_disp_vals,
	    "%.2f\t"
	    "%.2f\t"
	    "%.2f\t"
	    "%.2f\t"
	    "%.2f\t"
	    "%.2f\t"
	    "%.2f\t"
	    "%.2f\t"
    	    "%.2f\t"
	    "%.2f\t\r",
	    _obj->var_raw_flg? _obj->_var_parent._msg_buff._ai4_val : (_obj->_var_dp_val_ptr? *_obj->_var_dp_val_ptr[0] : 0.0),
	    _obj->var_raw_flg? _obj->_var_parent._msg_buff._ai5_val : (_obj->_var_dp_val_ptr? *_obj->_var_dp_val_ptr[1] : 0.0),
	    _obj->var_raw_flg? _obj->_var_parent._msg_buff._ai6_val : (_obj->_var_dp_val_ptr? *_obj->_var_dp_val_ptr[2] : 0.0),
	    _obj->var_raw_flg? _obj->_var_parent._msg_buff._ai7_val : (_obj->_var_dp_val_ptr? *_obj->_var_dp_val_ptr[3] : 0.0),
	    _obj->var_raw_flg? 0.0 :_vel,
	    _obj->var_raw_flg? 0.0 :_vol,
	    _obj->var_raw_flg? _obj->_var_parent._msg_buff._ai2_val : thsen_get_value(_obj->_var_st_sen),
	    _obj->var_raw_flg? _obj->_var_parent._msg_buff._ai1_val : _f_sp,
	    _obj->var_raw_flg? 0.0 :_obj->var_fm_ratio*_f_sp,
	    _obj->var_raw_flg? _obj->_var_parent._msg_buff._ai0_val : thsen_get_value(_obj->_var_tp_sen));

    return _rt_val;
}

/*
 * Initialise application. This method also sets the raw
 * value pointer to each sensors. Therefore when raw value
 * buffers are poped from the queue and get methods are called,
 * all raw voltage values shall be converted to physical properties.
 */
static int _thapp_ahu_init(thapp* obj, void* self)
{
    int i;

    thapp_ahu* _obj;

    if(self == NULL)
	return -1;

    _obj = (thapp_ahu*) self;

    /* Generate actuator control values */
    for(i=0; i<THAPP_AHU_DMP_BUFF; i++)
	_obj->var_dmp_buff[i] = sin(M_PI*(double)i/THAPP_AHU_DMP_BUFF)*100.0;

    /* Set raw value pointers for the sensors */
    /*----------------------------------------*/

    /* Set velocity sensor update pointer */
    thvsen_set_raw_buff(THOR_VSEN(_obj->_var_vsen), _obj->_var_msg_addr[0], THAPP_AHU_NUM_MAX_PROBES);

    /* Set temperature raw value set */
    thgsens_set_value_ptr(THOR_GSEN(_obj->_var_tp_sen), &obj->_msg_buff._ai0_val);

    /* Set speed sensor */
    thgsens_set_value_ptr(THOR_GSEN(_obj->_var_sp_sen), &obj->_msg_buff._ai1_val);

    /* Set static sensor */
    thgsens_set_value_ptr(THOR_GSEN(_obj->_var_st_sen), &obj->_msg_buff._ai2_val);

    return 0;
}

/*
 * Controls the actuator position and sends the control signal to the
 * server. This method handles both increment and decremnt methods.
 * Uses percentages to calculate the voltage to be sent.
 */
static int _thapp_act_ctrl(thapp_ahu* obj, int incr, int* per, int flg)
{
    struct thor_msg _msg;						/* message */
    char _msg_buff[THORNIFIX_MSG_BUFF_SZ];
    double _val;

    /* Increment the value temporarily. */
    obj->var_act_pct += incr;

    /* Check if its within bounds. */
    if(obj->var_act_pct > THAPP_AHU_MAX_ACT_PER || obj->var_act_pct < THAPP_AHU_MIN_ACT_PER)
	{
	    /* Reset the value to the original and set the external value */
	    obj->var_act_pct -= incr;
	    return 0;
	}
    if(per != NULL)
	*per = obj->var_act_pct;

    /* Update command message */
    if(flg == 0)
	{
	    sprintf(obj->_var_parent.var_cmd_vals,
		    "<==================== Actuator - %i%% ====================>",
		    (int) obj->var_act_pct);
	}

    _val = obj->var_act_pct;
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
