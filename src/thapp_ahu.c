/* Implementation of the air handling unit test program. */

#include "thapp_ahu.h"

/* Settting keys */
#define THAPP_AHU_KEY "ahu"
#define THAPP_TMP_KEY "tp1"
#define THAPP_SPD_KEY "sp1"
#define THAPP_STT_KEY "st1"
#define THAPP_SMT_KEY "lkg"

/* Callback methods */
static int _thapp_ahu_init(thapp* obj, void* self);
static int _thapp_ahu_start(thapp* obj, void* self);
static int _thapp_ahu_stop(thapp* obj, void* self);
static int _thapp_cmd(thapp* obj, void* self, int cmd);

/* Helper method for loading configuration settings */
static int _thapp_new_helper(thapp_ahu* obj);

/* Constructor */
thapp* thapp_ahu_new(void)
{
    int i;
    thapp_ahu* _obj;

    _obj = (thapp_ahu*) malloc(sizeof(thapp_ahu));

    if(_obj == NULL)
	return NULL;

    /* Call to initalise the parent object */
    if(thapp_init(&_obj->_var_parent))
	{
	    free(obj);
	    return NULL;
	}

    /* Set child pointer of parent objecgt */
    _obj->_var_parent.var_child = (void*) _obj;

    /* Initialise function pointer array */
    THAPP_INIT_FPTR(_obj);

    /* Set function pointers of parent object */
    thapp_set_start_ptr(_obj, _thapp_ahu_start);
    thapp_set_stop_ptr(_obj, _thapp_ahu_stop);
    thapp_set_cmdhnd_ptr(_obj, _thapp_cmd);

    /* Load configurations and initialise sensors */
    if(_thapp_new_helper(_obj))
	{
	    /* Delete parent */
	    thapp_delete(&_obj->_var_parent);
	    free(_obj);
	    return NULL;
	}

    /* Set default running mode to manual */
    _obj->var_mode = 0;
    _obj->var_act_pct = 0;

    /* Initialise actuator buffer */
    for(; i<THAPP_AHU_DMP_BUFF; i++)
	_obj->var_dmp_buff[i] = 0.0;

    _obj->var_duct_dia = 0.0;
    _obj->var_duct_vel = 0.0;
    _obj->var_duct_vol = 0.0;
    _obj->var_duct_loss = 0.0;
    _obj->var_t_ext_st = 0.0;

    _obj->var_child = NULL;

    /* Return thapp object */
    return &_obj->_var_parent;
}

/* Desructor */
void thapp_delete(thapp* obj)
{
    /* Check object pointer */
    if(obj == NULL)
	retun;

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

}


/* Start callback */
static int _thapp_ahu_start(thapp* obj, void* self)
{
    return 0;
}

/* Stop called back */
static int _thapp_ahu_stop(thapp* obj, void* self)
{
    return 0;
}

static int _thapp_cmd(thapp* obj, void* self, int cmd)
{
    thapp_ahu* _obj;

    if(self == NULL)
	return 0;

    _obj = (thapp_ahu*) self;
    switch(cmd)
	{

	}

    return 0;
}

/*
 * Initialise application. This method also sets the raw
 * value pointer to each sensors. Therefore when raw value
 * buffers are poped from the queue and get methods are called,
 * all raw voltage values shall be converted to physical properties.
 */
static int _thapp_ahu_init(thapp* obj, void* self)
{
    thapp_ahu* _obj;

    if(self == NULL)
	return -1;

    _obj = (thapp_ahu*) self;

    /* Set raw value pointers for the sensors */
    /*----------------------------------------*/

    /* Set velocity sensor update pointer */
    thvsen_set_raw_buff(THOR_VSEN(_obj->_var_vsen), &_obj->_msg_buff._ai4_val);

    /* Set temperature raw value set */
    thgsens_set_value_ptr(THOR_GSEN(_obj->_var_tp_sen), &_obj->_msg_buff._ai0_val);

    /* Set speed sensor */
    thgsens_set_value_ptr(THOR_GSEN(_obj->_var_sp_sen), &_obj->_msg_buff._ai1_val);

    /* Set static sensor */
    thgsens_set_value_ptr(THOR_GSEN(_obj->_var_st_sen), &_obj->_msg_buff._ai2_val);

    return 0;
}
