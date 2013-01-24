/* Generic sensor class to be inherited by all sensor
 * classes. Provides interfaces to reading and writinge
 * derives equation of the straigt line between voltage
 * and physical property */

#ifndef __THGSENS_H__
#define __THGSENS_H__

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

typedef struct _thgsens thgsens;
typedef unsigned int (*gthsen_fptr)(void*, double*);	/* generic function pointer */

struct _thgsens
{
    char* var_ch_name;				/* channel name */
    TaskHandle* var_task;			/* pointer to task */
    thbuff* var_buffval;			/* buffer value */
    const double* _var_cal_buff_x;		/* calibration buffer x */
    const double* _var_cal_buff_y;		/* calibration buffer y */
    double var_min;				/* minimum physical dimension */
    double var_max;				/* maximum physical dimension */
    double var_val;				/* sensor return value */
    float64 var_raw;				/* raw voltage value from sensor */
    unsigned int var_termflg;			/* termination flag for data gathering */
    int _var_calbuff_sz;			/* buffer size */
    gthsen_fptr var_update;			/* update function pointer - if the function
						 * pointer was assigned, it shall be called
						 * is used for updating ui */
    double var_grad;				/* gradient of straight line */
    double var_intc;				/* intercept point */
    unsigned int var_okflg;			/* flag to indicate ranges has been set */
    void* sobj_ptr;				/* pointer to external object */
    unsigned int var_flg;			/* flag to indicate enabled or disable state */
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor with function pointer and channel name */
    int thgsens_new(thgsens** obj, const char* ch_name,
		    TaskHandle* task, gthsen_fptr fptr, void* data);

    /* destructor */
    void thgsens_delete(thgsens** obj);

    /* set range */
    extern inline int thgsens_set_range(thgsens* obj, double min, double max);

    /* Add new raw value */
    inline __attribute__ ((always_inline)) static int thgsens_add_value(thgsens* obj, float64 val)
    {
	if(!obj)
	    return 0;
	obj->var_raw = val;
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

#endif /* __THGSENS_H__ */
