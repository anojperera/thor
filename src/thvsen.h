/*
 * Velocity sensors. This class is derived using generic sensor class.
 */
#ifndef __THVSEN_H__
#define __THVSEN_H__

#include <libconfig.h>
#include <stdlib.h>
#include "thornifix.h"
#include "thvprb.h"


#define THVSEN_SEN_NAME_BUFF 64
#define THVSEN_MIN_RANGE 0.0
#define THVSEN_MAX_RANGE 1600.0

typedef struct _thvsen thvsen;

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


struct _thvsen
{
    unsigned int var_init_flg;						/* Flag to indicate initialised */
    unsigned int var_config_sen;					/* Flag to indicate the number of configurations */
    size_t var_buff_sz;							/* Buffer size */
    config_setting_t* _var_setting;					/* Configuration object */

    const double const* var_raw_buff;					/* Raw buffer */
    double var_val;

    
    /* Array of sensors */
    struct senconfig* _var_configs;					/* Configuration buffers */
    thgsensor** var_sens;
};

#ifdef __cpluscplus
extern "C" {
#endif

    /* Constructor and destructor */
    int thvsen_new(thvsen* obj, config_setting_t* setting, size_t num);
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
