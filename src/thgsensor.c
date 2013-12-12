/* Implementation of the sensor class */
#include "thgsensor.h"
#include <math.h>

static void _thgsensor_delete(void* obj);
static double _thgsensor_get_val(void* obj);

/* Constructor */
thsen* thgsensor_new(thgsensor* obj,			/* object pointer to initialise */
		  void* data)				/* optional external object pointer */
{
    int i;
    
    /* check for arguments */
    if(obj == NULL)
	{
	    obj = (thgsensor*) malloc(sizeof(thgsensor));
	    obj->var_int_flg = 1;
	}
    else
	obj->var_int_flg = 0;

    /* initialise the parent object */
    if(thsen_init(&obj->var_parent))
	{
	    if(obj->var_int_flg)
		free(obj);
	    return NULL;
	}

    /* set child pointer of parent object to self */
    obj->var_parent.var_child = (void*) obj;

    /*
     * Using helper macros in thsen class, we set
     * the function pointer array with local method.
     */
    thsen_set_parent_del_fptr(obj, _thgsensor_delete);
    thsen_set_parent_get_fptr(obj, _thgsensor_get_val);


    memset(obj->var_ch_name, 0, THGS_CH_NAME_SZ);

    /* zero averaging buffer */
    for(i = 0; i<THGS_CH_RBUFF_SZ; i++)
	obj->var_ave_buff[i] = 0.0;
    obj->_var_ave_buff = 0.0;
    
    /* set calibration buffers */
    obj->_var_cal_buff_x = NULL;
    obj->_var_cal_buff_y = NULL;

    obj->var_grad = 0.0;
    obj->var_intc = 0.0;
    obj->var_min = 0.0;
    obj->var_max = 0.0;

    obj->var_val = 0.0;
    obj->var_raw = 0.0;
    obj->var_raw_ptr = NULL;
    obj->var_min_val = 0.0;

    obj->var_init_flg = 1;
    obj->var_out_range_flg = 0;
    obj->var_termflg = 0;
    obj->_var_raw_set = 0;
    obj->var_flg = 0;
    obj->var_okflg = 0;
    obj->_count = 0;
    obj->_count_flg = 0;
    obj->var_out_range_flg = 0;
    obj->sobj_ptr = data;
    obj->var_child = NULL;

    /* initialise self vtable */
    thsen_self_init_vtable(obj);

    return thsen_return_parent(obj);
}

/* Delete method */
void thgsensor_delete(thgsensor* obj)
{
    if(obj == NULL)
	return;
    
    /* Delete parent object */
    thsen_delete(&obj->var_parent);
    
    obj->_var_cal_buff_x = NULL;
    obj->_var_cal_buff_y = NULL;

    obj->sobj_ptr = NULL;
    obj->var_raw_ptr = NULL;    
    obj->var_out_range_flg = 0;    
    obj->var_init_flg = 0;

    /* Set delete pointers NULL */
    obj->var_child = NULL;

    /* We reset the function pointer array before deleting self */
    thsen_self_init_vtable(obj);

    if(obj->var_int_flg)
	free(obj);
    
    return;
}


/* reset all */
int thgsens_reset_all(thgsensor* obj)
{
    int i = 0;
    if(obj == NULL)
	return -1;

    /* Check for initialisation flag */
    if(obj->var_init_flg != 1)
	return -1;
    
    for(i=0; i<THGS_CH_RBUFF_SZ; i++)
	obj->var_ave_buff[i] = 0.0;

     obj->_var_ave_buff = 0.0;

    obj->_count = 0;
    obj->_count_flg = 0;
    return 0;
}

/*===========================================================================*/
/****************************** Private Methods ******************************/
/* Delete helper */
static void _thgsensor_delete(void* obj)
{
    thgsensor* _obj;
    /* Check argument */
    if(obj == NULL)
	return;

    _obj = (thgsensor*) obj;
    thgsensor_delete(_obj);
    return;
}

/* Get val method */
static double _thgsensor_get_val(void* obj)
{
    thgsensor* _obj;
    if(obj == NULL)
	return 0.0;

    /* Cast object to self */
    _obj = (thgsensor*) obj;

    
    /* check if range is set */
    if(!_obj->var_flg)
	return 0.0;
    if(_obj->var_init_flg != 1)
        return 0.0;

    /* Set raw value from pointer */
    if(_obj->var_raw_ptr)
	_obj->var_raw = *_obj->var_raw_ptr;

    /* If the raw value is negative, we return 0.0 */
    if(_obj->var_raw < 0.0)
	return 0.0;

    
    /* add to buffer */
    _obj->var_ave_buff[_obj->_count] = _obj->var_grad * _obj->var_raw + _obj->var_intc;
    
    _obj->_var_ave_buff += _obj->var_ave_buff[_obj->_count];
    _obj->_var_ave_buff -= _obj->var_ave_buff[(int) fabs(_obj->_count-THGS_CH_RBUFF_SZ)];
    _obj->var_val = _obj->_var_ave_buff / (_obj->_count_flg > 0? THGS_CH_RBUFF_SZ : _obj->_count);

    /* increment counter and reset at max */
    if(_obj->_count++ >= THGS_CH_RBUFF_SZ)
	{
	    _obj->_count = 0;
	    _obj->_count_flg = 1;
	}

    /* Adjust calculated value if minimum value was set */
    _obj->var_val -= (_obj->var_min_val > 0.0? _obj->var_min_val : 0.0);
    if(_obj->var_fptr.var_get_fptr)
	return _obj->var_fptr.var_get_fptr(_obj->var_child);
    else
	return _obj->var_val;
}
