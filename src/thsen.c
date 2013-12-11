#include "thsen.h"

/*
 * Initialises the class
 */
int thsen_init(thsen* obj)
{
    it(obj == NULL)
	return -1;

    obj->var_setting = NULL;
    obj->var_child = NULL;
    
    obj->_var_num_config = 0;
    obj->_var_configs = NULL;

    /* initialise function pointers */
    thsen_self_init_vtable(obj);

    obj->var_init_flg = 1;
    return 0;
}

/*
 * Release any memory consumed by this class and calls,
 * destructors of any child classes which are derived from
 * this.
 */
void thsen_delete(thsen* obj)
{
    int i = 0;
    
    if(obj == NULL)
	return;
    
    for(i=0; i< obj->_var_num_config; i++)
	{
	    /* delete sensor configuration details */
	    free(obj->_var_configs[i].var_calib_x);
	    free(obj->_var_configs[i].var_calib_y);

	    obj->_var_configs[i].var_calib_x = NULL;
	    obj->_var_configs[i].var_calib_y = NULL;
	}
    
    if(obj->_var_num_config)
	free(obj->_var_configs);
    obj->_var_configs = NULL;

    if(obj->var_fptr.var_thsen_del_fptr)
	obj->var_fptr.var_thsen_del_fptr(obj->var_child);

    return 0;
}

/*===========================================================================*/
/***************************** Private Methods *******************************/

/* Read configuration settings */
int thsen_read_config(thsen* obj)
{
    int i, j, _num, _num2, _num3;
    config_setting_t* _setting, *_t_setting, *_t2_setting;
    const char* _name;
    
    /* check if the configuration settings has been set */
    if(!obj->var_setting)
	return -1;

    /* Get number of settings in the class. */
    _num = config_setting_length(obj->var_setting);
    obj->_var_num_config = _num;

    /* Allocate buffer and initialise elements of each struct */
    obj->_var_configs = (struct senconfig*) calloc(_num, sizeof(struct senconfig));
    for(i=0; i<_num; i++)
	{
	    memset((void*) &obj->_var_configs[i], 0, sizeof(struct senconfig));
	    obj->_var_configs[i]._val_calib_elm_cnt = 0;
	    obj->_var_configs[i].var_calib_x = NULL;
	    obj->_var_configs[i].var_calib_y = NULL;
	}

    /*
     * Iterate over the settings collection and obtain the setting.
     * for each statndard type. Calibration arrays are.
     */
    for(i=0; i<_num; i++)
	{
	    /* Get setting based on index */
	    _setting = config_setting_get_elem(obj->var_setting, i);

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
			    /* Check if a valid name is returned */
			    if(_name == NULL)
				break;
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
			    thvsen_read_array(_t2_setting, _num3, &obj->_var_configs[i].var_calib_x);
			    break;
			case 4:
			    _t2_setting = config_setting_get_member(_setting, THOR_CONFIG_CALY);
			    if(_t2_setting == NULL)
				break;			    
			    thvsen_read_array(_t2_setting, _num3, &obj->_var_configs[i].var_calib_y);
			    break;
			default:
			}
		}
	    
	}

    return 0;
}

/*
 * Loads array elements defined in a configuration file to the pointer pointed by arr.
 * The size of the array must be greater than zeor. A new arr of size sz shall be allocated
 */
int thsen_read_array(const config_setting_t* setting, size_t sz, double** arr)
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
