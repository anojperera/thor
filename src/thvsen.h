/*
 * Velocity sensors. This class is derived using generic sensor class.
 */
#ifndef __THVSEN_H__
#define __THVSEN_H__

#include <libconfig.h>
#include "thornifix.h"
#include "thvprb.h"


#define THVSEN_NUM_PROBES 4
typedef struct _thvsen thvsen;

struct _thvsen
{
    unsigned int var_init_flg;						/* Flag to indicate initialised */
    unsigned int var_config_sen;					/* Flag to indicate the number of configurations */

    config_t* _var_config;						/* Configuration object */

    double* var_raw_buff;						/* Raw buffer */
    double var_val_buff[THVSEN_NUM_PROBES];
    double var_val;
    
    /* Array of sensors */
    thgsensor* var_sens[THVSEN_NUM_PROBES];

};

#ifdef __cpluscplus
extern "C" {
#endif

    /* Constructor and destructor */
    int thvsen_new(thvsen* obj, config_t* config);
    void thvsen_delete(thvsen* obj);

    /* Get value */
    double thvsen_get_val(thvsen* obj, double* buff, size_t sz);

#ifdef __cpluscplus
}
#endif
    

#endif /* __THVSEN_H__ */
