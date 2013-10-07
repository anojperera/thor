/* Implementation of the sensor class */
#include "thgsensor.h"
#include <math.h>

/* Constructor */
int thgsensor_new(thgsensor* obj,			/* object pointer to initialise */
		  const char* ch_name,			/* channel name */
		  TaskHandle* task,			/* NI task handle */
		  void* data)				/* optional external object pointer */
{
    int i;
    /* check for arguments */
    if(obj == NULL)
	return -1;
    if(ch_name == NULL)
	return -1;
    if(task == NULL)
	return -1;

    /* Copy channel name */
    strncpy(obj->var_ch_name, ch_name, THGS_CH_NAME_SZ-1);
    obj->var_ch_name[THGS_CH_NAME_SZ-1] = '\0';

    /* zero averaging buffer */
    for(i = 0; i<THGS_CH_RBUFF_SZ; i++)
	obj->var_ave_buff[i] = 0.0;
    obj->_var_ave_buff = 0.0;
    
    /* set task pointer */
    obj->var_task = task;

    /* set calibration buffers */
    obj->_var_cal_buff_x = NULL;
    obj->_var_cal_buff_y = NULL;

    obj->var_grad = 0.0;
    obj->var_intc = 0.0;
    obj->var_min = 0.0;
    obj->var_max = 0.0;

    obj->var_val = 0.0;
    obj->var_raw = 0.0;

    obj->var_termflg = 0;
    obj->_var_raw_set = 0;
    obj->var_flg = 0;
    obj->var_okflg = 0;
    obj->_count = 0;
    obj->_count_flg = 0;
    obj->sobj_ptr = data;

    return 0;
}

/* Delete method */
void thgsensor_delete(thgsensor* obj)
{
    obj->var_task = NULL;
    
    obj->_var_cal_buff_x = NULL;
    obj->_var_cal_buff_y = NULL;

    obj->sobj_ptr = NULL;
}

/* get value */
double thgsens_get_value(thgsens* obj)
{
    if(obj == NULL)
       return 0.0;
    /* check if range is set */
    if(!obj->var_flg)
	return 0.0;
    
    /* add to buffer */
    obj->var_ave_buff[obj->_count] = obj->var_grad * (double) var_raw + obj->var_intc;
    
    obj->_var_ave_buff += obj->var_ave_buff[obj->_count];
    obj->_var_ave_buff -= obj->var_ave_buff[(int) fabs(obj->_count-THGS_CH_RBUFF_SZ)];
    obj->var_val = obj->_var_ave_buff / (obj->_count_flg > 0? THGS_CH_RBUFF_SZ : obj->_count);

    /* increment counter and reset at max */
    if(obj->_count++ >= THGS_CH_RBUFF_SZ)
	{
	    obj->_count = 0;
	    obj->_count_flg = 1;
	}
    return obj->var_val;
}

/* reset all */
int thgsens_reset_all(thgsens* obj)
{
    int i = 0;
    if(obj == NULL)
	return -1;
    for(i=0; i<THGS_CH_RBUFF_SZ; i++)
	obj->var_ave_buff[i] = 0.0;

    obj->_var_ave_buff = 0.0;

    obj->_count = 0;
    obj->_count_flg = 0;
}
