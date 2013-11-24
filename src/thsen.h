/*
 * Generic sensor class. All sensor inherit this class.
 * Sun Nov 24 13:14:27 GMT 2013
 */

#ifndef _THSEN_H_
#define _THSEN_H_

#include <stdlib.h>
#include <libconfig.h>
#include "thornifix.h"

typedef struct _thsen thsen;

/* Function pointer table */
struct thsen_vftpr
{
    void (*var_del_fptr)(void*);					/* Delete function pointer */
    double (*var_get_fptr)(void*);					/* Get function pointer */
    int (*var_set_config_fptr)(void*);					/* Calls when set configuration method is invoked */
};

/* Sensor configuration struct */
struct senconfig
{
    unsigned int  _val_calib_elm_cnt;					/* element count */    
    char var_sen_name_buff[THVSEN_SEN_NAME_BUFF];			/* sensor name buffer */
    double var_range_min;						/* sensor minimum range */
    double var_range_max;						/* sensor maximum range */

    double* var_calib_x;						/* calibration buffers */
    double* var_calib_y;						/* calibration buffers */

};

struct _thsen
{
    int var_init_flg;							/* flag to indicate the sensor object is initialised */
    const config_setting_t* var_setting;				/* setting object pointer */
    
    void* var_child;							/* pointer to a child object */
    
    size_t _var_num_config;						/* number of configuration files */
    struct senconfig* _var_configs;					/* Array of sensor configuration data loaded */

    /*
     * Function pointers to be called in derrived classes
     * if set they shall be called.
     */
    struct thsen_vftpr var_fptr;

};

#ifdef __cpluscplus
extern "C" {
#endif

    /* Initialise sensor object */
    int thsen_init(thsen* obj);
    void thsen_delete(thsen* obj);

    /* Helper macros for setting function pointers */
#define thsen_self_init_vtable(obj)		\
    (obj)->var_fptr.var_del_fptr = NULL;	\
    (obj)->var_fptr.var_get_fptr = NULL;	\
    (obj)->var_fptr.var_set_config_fptr = NULL
#define thsen_set_parent_del_fptr(obj, fptr)		\
    (obj)->var_parent.var_fptr.var_del_fptr = fptr
#define thsen_set_parent_get_fptr(obj, fptr)		\
    (obj)->var_parent.var_fptr.var_get_fptr = fptr
#define thsen_set_parent_setconfig_fptr(obj, fptr)		\
    (obj)->var_parent.var_fptr.var_set_config_fptr = fptr

    /* Return parent */
#define thsen_return_parent(obj)		\
    &(obj)->var_parent
    /* Get value of the sensor */
    inline __attribute__ ((always_inline)) static double thsen_get_value(thsen* obj)
    {
	if(obj == NULL)
	    return 0.0;

	if(obj->var_fptr.var_get_fptr)
	    return obj->var_fptr.var_get_fptr(obj->var_child);

	return 0.0;
    }


    /* Set configuration data */
    inline __attribute__ ((always_inline)) static int thsen_set_config(thsen* obj, const config_setting_t* setting)
    {
	if(obj == NULL || setting == NULL)
	    return -1;

	/*
	 * Check if object was initialised.
	 * If the object was initialised, init_value should exactly be 1.
	 */
	if(obj->var_init_flg != 1)
	    return -1;

	/* Set value and call function pointer if was set */
	obj->var_setting = setting;
	if(obj->var_fptr.var_set_config_fptr)
	    obj->var_fptr.var_set_config_fptr(obj->var_child);

	return 0;
    }

    /*
     * Read settings file and store calibration buffers
     * into internal networks.
     */
    int thsen_read_config(thsen* obj);
    int thsen_read_array(const config_setting_t* setting, size_t sz, double** arr);    
#ifdef __cpluscplus
}
#endif

#endif /* _THSEN_H_ */
