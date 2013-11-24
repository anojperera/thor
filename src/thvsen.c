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
static int _thvsen_read_array(const config_setting_t* setting, size_t sz, double** arr);

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
    int i, j, _num, _num2, _num3;
    config_setting_t* _setting, *_t_setting, *_t2_setting;
    const char* _name;
    
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
    for(i=0; i<_num; i++)
	{
	    /* Get setting based on index */
	    _setting = config_setting_get_elem(obj->_var_setting, i);

	    /* Check for setting if null, continue with loop */
	    if(!_setting)
		continue;

	    /* Get number of settings */
	    _num2 = config_setting_length(_setting);
	    for(j=0; j<_num2; j++)
		{
		    switch(j)
			{
			case 0:
			    _name = config_setting_get_string_elem(_setting, j);
			    strncpy(obj->_var_configs[i].var_sen_name_buff, _name, THVSEN_SEN_NAME_BUFF-1);
			    obj->_var_configs[i].var_sen_name_buff[THVSEN_SEN_NAME_BUFF-1] = '\0';
			    break;
			case 1:
			    obj->_var_configs[i].var_range_min = config_setting_get_float_elem(_setting, j);
			    break;
			case 2:
			    obj->_var_configs[i].var_range_max = config_setting_get_float_elem(_setting, j);
			    break;
			case 3:
			    /*
			     * Load calibration settings if exists. Index 3 and 4 loads calibration
			     * settings as defined in the configuration file.
			     */
			    _t2_setting = config_setting_get_member(_setting, THOR_CONFIG_CALX);
			    if(_t2_setting == NULL)
				break;
			    
			    _num3 = config_setting_length(_t2_setting);
			    _thvsen_read_array(_t2_setting, _num3, &obj->_var_configs[i].var_calib_x);
			    break;
			case 4:
			    _t2_setting = config_setting_get_member(_setting, THOR_CONFIG_CALY);
			    if(_t2_setting == NULL)
				break;			    
			    _thvsen_read_array(_t2_setting, _num3, &obj->_var_configs[i].var_calib_y);
			    break;
			default:
			};
		}
	    
	}

}

/*
 * Loads array elements defined in a configuration file to the pointer pointed by arr.
 * The size of the array must be greater than zeor. A new arr of size sz shall be allocated
 */
static int _thvsen_read_array(const config_setting_t* setting, size_t sz, double** arr)
{
    int i;

    /* check for arguments */
    if(setting == NULL || sz <= 0 || arr == NULL)
	return -1;

    /* Allocate memory to the buffer */
    (*arr) = (double*) calloc(sz, sizeof(double));

    /*
     * Iterate through the settings collection and set the values
     * defined in the configuration file to the buffer elements
     */
    for(i=0; i<sz; i++)
	(*arr)[i] = config_setting_get_float_elem(setting, i);

    return 0;
}
