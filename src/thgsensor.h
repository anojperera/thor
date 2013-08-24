/* Generic sensor class to be inherited by all sensor
 * classes. Provides interfaces to reading and writinge
 * derives equation of the straigt line between voltage
 * and physical property */

#ifndef _THGSENSOR_H_
#define _THGSENSOR_H_

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#if defined (WIN32) || defined (_WIN32)
#include <NIDAQmx.h>
#else
#include <NIDAQmxBase.h>
#endif
#include "thornifix.h"
#include "thbuff.h"

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
    char var_ch_name[THGS_CH_NAME_SZ];		/* channel name */
    double var_ave_buff[THGS_CH_RBUFF_SZ];	/* averaging buffer */
    
    TaskHandle* var_task;			/* pointer to task handle */

    /* calibration buffers */
    const double* _var_cal_buff_x;		/* calibration buffer x */
    const double* _var_cal_buff_y;		/* calibration buffer y */
    int _var_calbuff_sz;			/* calibration buffer size */    
    
    double var_grad;				/* gradient of straight line */
    double var_intc;				/* intercept point */    
    double var_min;				/* minimum physical dimension */
    double var_max;				/* maximum physical dimension */
    
    double var_val;				/* sensor return value */
    float64 var_raw;				/* raw voltage value from sensor */

    unsigned int var_termflg;			/* termination flag for data gathering */
    unsigned int _var_raw_set;			/* flag to indicate raw value set */    
    unsigned int var_flg;			/* flag to indicate enabled or disable state */    
    unsigned int var_okflg;			/* flag to indicate ranges has been set */
    
    void* sobj_ptr;				/* pointer to external object */
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor with function pointer and channel name */
    int thgsensor_new(thgsensor* obj,		/* object pointer to initialise */
		      const char* ch_name,	/* channel name */
		      TaskHandle* task,		/* NI task handle */
		      void* data);		/* optional external object pointer */

    /* destructor */
    void thgsensor_delete(thgsensor* obj);

    /* set range */
    inline __attribute__ ((always_inline)) static int thgsensor_set_range(thgsensor* obj, double min, double max)
    {
	if(obj == NULL)
	    return 1;
	obj->var_min = min;
	obj->var_max = max;

	/* calculate gradient and intercept */
	THGS_CALC_GRAD(obj);
	THGS_CALC_INTR(obj);
	return 0;
    }

    /* Add new raw value */
    inline __attribute__ ((always_inline)) static int thgsens_add_value(thgsens* obj, float64 val)
    {
	if(!obj)
	    return 0;
	obj->var_raw = val;
	obj->_var_raw_set = 1;
	return 1;
    }

    /* get current value */
    extern inline double thgsens_get_value(thgsens* obj);

    /* get current value without calling callback
     * function */
    extern inline double thgsens_get_value2(thgsens* obj);

    /* reset all */
    extern inline int thgsens_reset_all(thgsens* obj);

    /* reset value */
    extern inline int thgsens_reset_value(thgsens* obj);

    /* set buffer and buffer size */
    inline __attribute__ ((always_inline)) static int thgsens_set_calibration_buffers(thgsens* obj, const double* x, const double* y, int n)
    {
	if(obj == NULL)
	    return 1;
	obj->_var_cal_buff_x = x;
	obj->_var_cal_buff_y = y;
	obj->_var_calbuff_sz = n;
	return 0;
    }
#ifdef __cplusplus
}
#endif

#endif /* _THGSENSOR_H_ */
