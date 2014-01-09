/*
 * Velocity sensors. This class is derived using generic sensor class.
 */
#ifndef __THVSEN_H__
#define __THVSEN_H__

#include <libconfig.h>
#include <stdlib.h>
#include "thornifix.h"
#include "thsen.h"
#include "thvprb.h"


#define THVSEN_SEN_NAME_BUFF 64
#define THVSEN_MIN_RANGE 0.0
#define THVSEN_MAX_RANGE 1600.0

typedef struct _thvsen thvsen;

struct _thvsen
{
    thsen var_parent;							/* Parent class */
    unsigned int var_int_flg;
    unsigned int var_init_flg;						/* Flag to indicate initialised */
    unsigned int var_num_sen;
    unsigned int var_config_sens;					/* Configured sensors */
    size_t var_buff_sz;							/* Buffer size */

    const double* var_raw_buff;						/* Raw buffer */
    double var_val;
    
    /* Array of sensors */
    thsen** var_sens;
    
    void* var_child;							/* child object */
    struct thsen_vftpr var_fptr;					/* Function pointer array */
};

#ifdef __cpluscplus
extern "C" {
#endif

    /* Constructor and destructor */
    thsen* thvsen_new(thvsen* obj, const config_setting_t* setting, size_t num);
    void thvsen_delete(thvsen* obj);

    /*
     * Set raw buffer pointer pointer.
     * This class expects the raw sensor values to be continous in memory.
     * Once set, each time get value calls, buffer values are passed to
     * probe sensors to obtain the velocity.
     */
    inline __attribute__ ((always_inline)) static int thvsen_set_raw_buff(thvsen* obj, double* buff, size_t sz)
    {
	int i = 0;
	double* _raw;
	if(obj == NULL)
	    return -1;

	obj->var_raw_buff = buff;
	obj->var_buff_sz = sz;

	/* Check the initialise flag */
	if(!obj->var_init_flg)
	    return -1;

	/* Set raw buffer pointer to each generic sensor */
	_raw = buff;
	for(; i<sz; i++,_raw++)
	    thgsens_set_value_ptr(THOR_GSEN(obj->var_sens[i]), _raw);

	return 0;
    }

    /* Return parent */
    inline __attribute__ ((always_inline)) static thsen* thvsen_return_parent(thvsen* obj)
    {
	return &obj->var_parent;
    }
    
    /*
     * Get value method is implemented in the parent class.
     * A function pointer is set in the parent class pointing
     * to a local private method.
     */

    /* Get buffer of Differential pressure values */
    int thvsen_get_dp_values(thvsen* obj, double* array, size_t* sz);

#define thvsen_configure_sensors(obj, num)	\
    (obj)->var_config_sens = num
    
#ifdef __cpluscplus
}
#endif
    

#endif /* __THVSEN_H__ */
