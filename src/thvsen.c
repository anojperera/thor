/*
 * Implementation of the velocity class.
 * The velocity sensor contains array of sensor probes,
 * derrived from parent generic sensor class.
 * An average velocity is worked out between the sensors.
 */

#include "thvsen.h"

static double thvsen_get_val(void* obj);
static int thvsen_set_config(void* obj);

/* Constructor */
thsen* thvsen_new(thvsen* obj, config_setting_t* setting, size_t num)
{
    int i;
    double* _raw_val;
    
    if(setting == NULL || num <= 0)
	return NULL;
    
    /* check for arguments */
    if(obj == NULL)
	{
	    obj = (thvsen*) malloc(sizeof(thvsen));
	    obj->var_int_flg = 1;
	}
    else
	obj->var_int_flg = 0;

    /* Initialise sensor object */
    if(thsen_init(&obj->var_parent))
	{
	    if(obj->var_int_flg)
		free(obj);
	    return NULL;
	}

    /* Set child pointer of parent object */
    obj->var_parent.var_child = (void*) obj;

    /* Set function pointers of parent object */
    thsen_set_parent_get_fptr(obj, thvsen_get_val);
    thsen_set_parent_setconfig_fptr(obj, thvsen_set_config);

    obj->var_num_sen = 0;
    obj->var_raw_buff = NULL;
    obj->var_val = 0.0;
    obj->var_sens = NULL;
    obj->var_child = NULL;

    /* Initialise function pointer array */
    thsen_self_init_vtable(obj);
    
    /* Set configuration file and read arrays */
    thsen_set_config(&obj->var_parent, setting);
    if(thsen_read_config(&obj->var_parent))
	{
	    thvsen_delete(obj);
	    return NULL;
	}


    /* create array of velocity probes */
    obj->var_sens = (thsen**) calloc(obj->var_num_sen, sizeof(thsen*));

    /* Configure sensors */
    for(i=0; i<obj->var_num_sen; i++)
	{
	    obj->var_sens[i] = NULL;
	    obj->var_sens[i] = thvprb_new(NULL);

	    /* Set range, calibration buffers and other parameters */
	    thgsens_set_calibration_buffers(THOR_GSEN(obj->var_sens[i]),					/* Velocity probe */
					    obj->var_parent._var_configs[i].var_calib_x,			/* X calibration buffer */
					    obj->var_parent._var_configs[i].var_calib_y,			/* Y calibration buffer */
					    obj->var_parent._var_configs[i]._val_calib_elm_cnt);		/* Element count */

	    /* Set range */
	    thgsensor_set_range(THOR_GSEN(obj->var_sens[i]),							/* Sensor */
				obj->var_parent._var_configs[i].var_range_min,					/* Minimum range */
				obj->var_parent._var_configs[i].var_range_max);					/* Maximum range */
	}

    
    /* Set initialisation flag */
    obj->var_init_flg = 1;

    /* Initialise self function pointer array */
    thsen_self_init_vtable(obj);

    /* Return parent super class */
    return thsen_return_parent(obj);
}


/* Delete sensor collection */
void thvsen_delete(thvsen* obj)
{
    int i = 0;
    
    if(obj == NULL)
	return;

    /* Delete parent object */
    

    /* Delete sensor collection */
    for(i=0; i<obj->var_num_sen; i++)
	{
	    if(obj->var_sens == NULL)
		break;
	    thgsensor_delete(THOR_GSEN(obj->var_sens[i]));
	    obj->var_sens[i] = NULL;
	}

    if(obj->var_sens)
	free(obj->var_sens);
    obj->var_sens = NULL;

    thsen_self_init_vtable(obj);
    obj->var_init_flg = 0;

    /* If the object was internally created, we delete it */
    if(obj->var_int_flg)
	free(obj);
    return;
}

/* Differential pressure values */
int thvsen_get_dp_values(thvsen* obj, double* array, size_t* sz)
{
    int i;
    if(obj == NULL || array = NULL)
	return -1;

    /* Get differential values of each velocity probe and fill the buffer */
    *sz = obj->var_num_sen;

    for(i=0; i<(*sz); i++)
	array[i] = thgsensor_get_value(THOR_GSEN(obj->var_sens[i]));

    return 0;
}

/*
 * Get value method calculates the velocity of the air stream measured between the sensors.
 */
static double thvsen_get_val(void* obj)
{
    thvsen* _obj;
    int i;

    if(obj == NULL)
	return 0.0;

    /* Cast object to the self */
    _obj = (thvsen*) obj;

    if(_obj->var_init_flg != 1)
	return 0.0;

    _obj->var_val = 0.0;
    for(i=0; i<_obj->var_num_sen; i++)
	{
	    if(i >= _obj->var_buff_sz)
		break;
	    _obj->var_val += thsen_get_value(_obj->var_sens[i]);
	}

    _obj->var_val /= i;
    return _obj->var_val;
}

/* Set cofiguration file */
static int thvsen_set_config(void* obj)
{
    thvsen* _obj;

    if(obj == NULL)
	return 0.0;
    
    _obj = (thvsen*) obj;
    _obj->var_num_sen = _obj->var_parent._var_num_config;
    return 0;
}
