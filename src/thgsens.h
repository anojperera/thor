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

typedef struct _thgsens thgsens;
typedef unsigned int (*gthsen_fptr)(void*, double*);	/* generic function pointer */

struct _thgsens
{
    char* var_ch_name;				/* channel name */
    TaskHandle* var_task;			/* pointer to task */
    double var_min;				/* minimum physical dimension */
    double var_max;				/* maximum physical dimension */
    double var_val;				/* sensor return value */
    float64 var_raw;				/* raw voltage value from sensor */
    unsigned int var_termflg;			/* termination flag for data gathering */
    gthsen_fptr var_update;			/* update function pointer - if the function
						 * pointer was assigned, it shall be called
						 * is used for updating ui */
    double var_grad;				/* gradient of straight line */
    double var_intc;				/* intercept point */
    unsigned int var_okflg;			/* flag to indicate ranges has been set */
    void* sobj_ptr;				/* pointer to external object */
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

    /* get current value */
    extern inline double thgsens_get_value(thgsens* obj);

    /* reset all */
    extern inline int thgsens_reset_all(thgsens* obj);

    /* reset value */
    extern inline int thgsens_reset_value(thgsens* obj);

#ifdef __cplusplus
}
#endif

#endif /* __THGSENS_H__ */
