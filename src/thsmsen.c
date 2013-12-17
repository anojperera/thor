/*
 * Implementation of the smart sensor class.
 */
#include "thsmsen.h"

/* Callback methods */
static double _thsmsen_get_val(void* obj);
static int _thsmsen_set_config(void* obj);

/* Constructor */
thsen* thsmsen_new(thsmsen* obj, const config_setting_t* settings)
{
    int i = 0;
    
    /* Check for settings pointer */
    if(settings == NULL)
	return NULL;

    if(obj == NULL)
	{
	    obj = (thsmsen*) malloc(sizeof(thsmsen));
	    obj->var_int_flg = 1;
	}
    else
	obj->var_int_flg = 0;

    /* Create parent object */
    if(thsen_init(&obj->var_parent))
	{
	    if(obj->var_int_flg)
		free(obj);
	    return NULL;
	}
    /* Set the child pointer of parent object */
    obj->var_parent.var_child = (void*) obj;

    /* Set function pointers of parent object */
    thsen_set_parent_get_fptr(obj, _thsmsen_get_val);
    thsen_set_parent_setconfig_fptr(obj, _thsmsen_set_config);

    obj->var_err_flg = 0;
    obj->var_next_ix = 0;
    obj->var_raw_val_sz = 0;
    obj->var_raw_vals = NULL;
    obj->var_val = 0.0;
    obj->var_sen_cnt = 0;
    obj->var_sen = NULL;
    /* Set settings object */
    thsen_set_config(&obj->var_parent, settings);
    thsen_read_config(&obj->var_parent);

    /*
     * The sensor count is determined by the number of
     *  configurations.
     */
    if(obj->var_sen_cnt > 0)
	{
	    obj->var_sen = (thsen**) calloc(obj->var_sen_cnt, sizeof(thsen*));

	    /* Configure senseors */
	    for(i=0; i<obj->var_sen_cnt; i++)
		{
		    obj->var_sen[i] = NULL;
		    obj->var_sen[i] = thgsensor_new(NULL, NULL);

		    thgsens_set_calibration_buffers(THOR_GSEN(obj->var_sen[i]),
					    obj->var_parent._var_configs[i].var_calib_x,			/* X calibration buffer */
					    obj->var_parent._var_configs[i].var_calib_y,			/* Y calibration buffer */
					    obj->var_parent._var_configs[i]._val_calib_elm_cnt);		/* Element count */

		    /* Set range */
		    thgsensor_set_range(THOR_GSEN(obj->var_sen[i]),
					obj->var_parent._var_configs[i].var_range_min,			/* Minimum range */
					obj->var_parent._var_configs[i].var_range_max);			/* Maximum range */					
		}

	}

    /* Initialise pointer array */
    thsen_self_init_vtable(obj);

    /* Return parent object */
    return thsen_return_parent(obj);
}

/* Destructor */
void thsmsen_delete(thsmsen* obj)
{
    int i;
    if(obj == NULL)
	return;

    /* Check if the object is initialised */
    if(obj->var_int_flg != 0 || obj->var_int_flg != 1)
	return;
    
    /* Call destructor of parent object */
    thsen_delete(&obj->var_parent);

    /* Remove sensors */
    for(i=0; i<obj->var_sen_cnt; i++)
	{
	    thgsensor_delete(THOR_GSEN(obj->var_sen[i]));
	    obj->var_sen[i] = NULL;
	}

    if(obj->var_sen_cnt > 0)
	free(obj->var_sen);
    obj->var_sen = NULL;

    obj->var_raw_vals = NULL;
    obj->var_child = NULL;

    
    /*
     * If the object was internaly created, we delete it.
     */
    if(obj->var_int_flg)
	free(obj);

    return;
}

/* Set value array */
int thsmsen_set_value_array(thsmsen* obj, const double* vals, size_t sz)
{
    int i = 0;
    /* Check object */
    if(obj == NULL)
	return -1;
    if(obj->var_int_flg != 1)
	return -1;

    /*
     * Compare the array sizes to the number of sensors.
     * If this is not equal, we have an error.
     */
    if(sz != obj->var_raw_val_sz)
	{
	    obj->var_err_flg = 1;
	    return -1;
	}

    obj->var_err_flg = 0;
	    
    /* Set value pointer */
    obj->var_raw_vals = vals;
    obj->var_raw_val_sz = sz;

    /* Assign values to the configuration file */
    for(; i<obj->var_raw_val_sz; i++)
	thgsens_set_value_ptr(THOR_GSEN(obj->var_sen[i]), &obj->var_raw_vals[i]);

    return 0;
}

/*===========================================================================*/
/* Callback method for configuration file */
static int _thsmsen_set_config(void* obj)
{
    thsmsen* _obj;

    /* Cast object to the right pointer */
    _obj = (thsmsen*) obj;

    _obj->var_sen_cnt = _obj->var_parent._var_num_config;
    return 0;
}

/* Get value method */
static double _thsmsen_get_val(void* obj)
{
    int i;
    thsmsen* _obj;
    
    /* Check object pointer */
    if(obj == NULL)
	return 0.0;

    _obj = (thsmsen*) obj;

    /*
     * If the error flag is set, we return
     * 0
     */
    if(_obj->var_err_flg)
	return 0.0;

    /* Iterate through the raw value and select sensor to use */
    _obj->var_val = 0.0;
    for(i=0; i<_obj->var_raw_val_sz; i++)
	{
	    if(_obj->var_raw_vals[i] > 0.1 && _obj->var_raw_vals[i] < 9.5)
		{
		    _obj->var_val = thsen_get_value(_obj->var_sen[i]);
		    break;
		}
	    
	}

    /*
     * If all failed and the value is still zero we try and retrieve the value
     * from the largest sensor in the range.
     */
    if(_obj->var_val <= 0.0)
	_obj->var_val = thsen_get_value(_obj->var_sen[i-1]);
    
    return _obj->var_val;
}
