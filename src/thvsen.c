/*
 * Implementation of the velocity class.
 * The velocity sensor contains array of sensor probes,
 * derrived from parent generic sensor class.
 * An average velocity is worked out between the sensors.
 */

#include "thvsen.h"

/*
 * Read settings file and store calibration buffers
 * into internal networks.
 */
static int _thvsen_read_config(thvsen* obj);


/* Constructor */
int thvsen_new(thvsen* obj, config_setting_t* setting, size_t num)
{
    int i;
    /* check for arguments */
    if(obj == NULL || config == NULL || num <= 0)
	return -1;

    /* Set configuration file and read arrays */
    obj->_var_setting = setting;
    obj->var_config_sen = num;
    
    if(_thvsen_read_config(obj))
	return -1;

    /* create array of velocity probes */
    obj->var_sens = (thgsensor**) calloc(obj->var_config_sen, sizeof(thgsensor*));

    /* Configure sensors */
    for(i=0; i<obj->var_config_sen; i++)
	{
	    obj->var_sens[i] = NULL;
	    obj->var_sens[i] = thvprb_new(NULL);

	    /* Set range, calibration buffers and other parameters */
	    thgsens_set_calibration_buffers(obj->var_sens[i],						/* Velocity probe */
					    obj->var_configs[i].var_calib_x,				/* X calibration buffer */
					    obj->var_configs[i].var_calib_y,				/* Y calibration buffer */
					    obj->var_configs[i]._val_calib_elm_cnt);			/* Element count */

	    /* Set range */
	    thgsensor_set_range(obj->var_sens[i],							/* Sensor */
				obj->var_configs[i].var_range_min,					/* Minimum range */
				obj->var_configs[i].var_range_max);					/* Maximum range */
	}

    obj->var_raw_buff = NULL;
    obj->var_val = 0.0;
    
    /* Set initialisation flag */
    obj->var_init_flg = 1;
    return 0;
}


/* Delete sensor collection */
void thvsen_delete(thvsen* obj)
{
    int i = 0;
    
    /* All internally alocated buffers and objects */

    if(obj == NULL)
	return;

    /* Delete sensor collection */
    for(i=0; i<obj->var_config_sen; i++)
	{
	    thgsensor_delete(obj->var_sens[i]);
	    obj->var_sens[i] = NULL;

	    /* delete sensor configuration details */
	    free(obj->var_configs[i].var_calib_x);
	    free(obj->var_configs[i].var_calib_y);

	    obj->var_configs[i].var_calib_x = NULL;
	    obj->var_configs[i].var_calib_y = NULL;
	}

    free(obj->var_sens);
    obj->var_sens = NULL;

    free(obj->var_configs);
    obj->var_configs = NULL;

    obj->var_init_flg = 0;
    return;
}

/*
 * Get value method calculates the velocity of the air stream measured between the sensors.
 */
double thvsen_get_val(thvsen* obj)
{
    int i;
    double const* _val = obj->var_raw_buff;

    obj->var_val = 0.0;
    for(i=0; i<obj->var_config_sen; i++)
	{
	    if(i >= obj->var_buff_sz)
		break;
	    thgsens_add_value(obj->var_sens[i], *_val);
	    obj->var_val += thgsens_get_value(obj->var_sens[i]);
	    _val++;
	}

    obj->var_val /= i-1;
    return obj->var_val;
}

/*===========================================================================*/
/***************************** Private Methods *******************************/

/* Read configuration settings */
static int _thvsen_read_config(thvsen* obj)
{
    int i, _num;
    /* check if the configuration settings has been set */
    if(!obj->_var_setting)
	return -1;

    /* Get number of settings in the class. */
    _num = config_setting_length(obj->_var_setting);

    /* Allocate buffer */
    obj->_var_configs = (struct senconfig*) calloc(_num, sizeof(struct senconfig));

    /*
     * Iterate over the settings collection and obtain the setting.
     * for each statndard type. Calibration arrays are.
     */


}
