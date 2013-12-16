/*
 * Generic class, provides additional interfaces for derriving
 * sensor classes. This class inherits thsen super class.
 */

#ifndef _THGSENSOR_H_
#define _THGSENSOR_H_

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "thornifix.h"
#include "thsen.h"

#define THGS_MIN_VOLT 0.0
#define THGS_MAX_VOLT 10.0
#define THGS_CH_NAME_SZ 64
#define THGS_CH_RBUFF_SZ 20

/* macros for calculating gradient and intercept */
#define THGS_CALC_GRAD(sensor)						\
    (sensor)->var_grad = (sensor)->var_max - (sensor)->var_min /	\
	(THGS_MAX_VOLT - THGS_MIN_VOLT)

#define THGS_CALC_INTR(sensor)						\
    (sensor)->var_intc = (sensor)->var_min - (sensor)->var_grad *	\
	THGS_MIN_VOLT


typedef struct _thgsensor thgsensor;

struct _thgsensor
{
    thsen var_parent;				/* parent object */
    unsigned int var_int_flg;
    unsigned int var_init_flg;			/* flag to initialise */
    unsigned int var_out_range_flg;		/* flag to the sensor is out of range */
    
    char var_ch_name[THGS_CH_NAME_SZ];		/* channel name */
    double var_ave_buff[THGS_CH_RBUFF_SZ];	/* averaging buffer */
    double _var_ave_buff;
    

    /* calibration buffers */
    const double* _var_cal_buff_x;		/* calibration buffer x */
    const double* _var_cal_buff_y;		/* calibration buffer y */
    int _var_calbuff_sz;			/* calibration buffer size */    
    
    double var_grad;				/* gradient of straight line */
    double var_intc;				/* intercept point */    
    double var_min;				/* minimum physical dimension */
    double var_max;				/* maximum physical dimension */
    
    double var_val;				/* sensor return value */
    double var_raw;				/* raw voltage value from sensor */
    const double* var_raw_ptr;			/* raw voltage pointer */
    double var_min_val;				/* If the value is set, this can be used to reset the
						 * value of the sensor. Useful for resetting to the environment
						 * values.
						 */

    unsigned int var_termflg;			/* termination flag for data gathering */
    unsigned int _var_raw_set;			/* flag to indicate raw value set */    
    unsigned int var_flg;			/* flag to indicate enabled or disable state */    
    unsigned int var_okflg;			/* flag to indicate ranges has been set */
    unsigned int _count;			/* internal buffer counter */
    unsigned int _count_flg;			/* count flag */
    void* sobj_ptr;				/* pointer to external object */
    void* var_child;

    /* Function pointers of child classes */
    struct thsen_vftpr var_fptr;		/* function pointer array */
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor with function pointer and channel name */
    thsen* thgsensor_new(thgsensor* obj,		/* object pointer to initialise */
			 void* data);			/* optional external object pointer */
    
    /* destructor */
    void thgsensor_delete(thgsensor* obj); 

    /* set range */
    inline __attribute__ ((always_inline)) static int thgsensor_set_range(thgsensor* obj, double min, double max)
    {
	if(obj == NULL)
	    return -1;
	if(obj->var_init_flg != 1)
	    return -1;
	
	obj->var_min = min;
	obj->var_max = max;

	/* calculate gradient and intercept */
	THGS_CALC_GRAD(obj);
	THGS_CALC_INTR(obj);
	obj->var_flg = 1;
	return 0;
    }

    /* Add new raw value */
    inline __attribute__ ((always_inline)) static int thgsens_set_value(thgsensor* obj, double val)
    {
	if(!obj)
	    return -1;
	if(obj->var_init_flg != 1)
	    return -1;
	
	obj->var_raw = val;
	obj->_var_raw_set = 1;
	return 0;
    }

    /*
     * Set raw value pointer. When this method is used, there's no requirement to
     * call set value method every time before get value is called.
     */
    inline __attribute__ ((always_inline)) static int thgsens_set_value_ptr(thgsensor* obj, const double* val)
    {
	if(!obj)
	    return -1;
	if(obj->var_init_flg != 1)
	    return -1;
	
	obj->var_raw_ptr = val;
	obj->_var_raw_set = 1;
	return 0;
    }

    /*
     * Get value method is implemented in the parent class.
     * A function pointer is set in the parent class pointing
     * to a local private method.
     */
    inline __attribute__ ((always_inline)) static double thgsensor_get_value(thgsensor* obj)
    {
	if(obj == NULL)
	    return 0.0;
	else
	    return obj->var_val;
    }

    /* Return parent */
    inline __attribute__ ((always_inline)) static thsen* thgsens_return_parent(thgsensor* obj)
    {
	return &obj->var_parent;
    }


    /* reset all */
    int thgsens_reset_all(thgsensor* obj);

    /* set buffer and buffer size */
    inline __attribute__ ((always_inline)) static int thgsens_set_calibration_buffers(thgsensor* obj, const double* x, const double* y, int n)
    {
	if(obj == NULL)
	    return -1;
	if(obj->var_init_flg != 1)
	    return -1;
	
	obj->_var_cal_buff_x = x;
	obj->_var_cal_buff_y = y;
	obj->_var_calbuff_sz = n;
	return 0;
    }

    /* Sets the initial value of the sensor. */
#define thgsens_set_init_val(obj, val)		\
    (obj)->var_min_val = val;
    
#ifdef __cplusplus
}
#endif

#endif /* _THGSENSOR_H_ */
