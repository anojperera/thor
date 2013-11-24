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
    size_t var_buff_sz;							/* Buffer size */

    const double const* var_raw_buff;					/* Raw buffer */
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
    thsen* thvsen_new(thvsen* obj, config_setting_t* setting, size_t num);
    void thvsen_delete(thvsen* obj);

    /*
     * Set raw buffer pointer pointer.
     * This class expects the raw sensor values to be continous in memory.
     * Once set, each time get value calls, buffer values are passed to
     * probe sensors to obtain the velocity.
     */
    inline __attribute__ ((always_inline)) static int thvsen_set_raw_buff(thvsen* obj, const double* buff, size_t sz)
    {
	if(obj == NULL)
	    return -1;

	obj->var_raw_buff = buff;
	obj->var_buff_sz = sz;

	return 0;
    }
    
    /* Get value */
    double thvsen_get_val(thvsen* obj);

#ifdef __cpluscplus
}
#endif
    

#endif /* __THVSEN_H__ */
