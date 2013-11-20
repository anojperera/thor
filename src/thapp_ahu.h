/*
 * Application for running the AHU performance test.
 * Auto Controls the damper and records pressure and
 * temperature readings.
 */
#ifndef __THAPP_AHU_H__
#define __THAPP_AHU_H__

#include <stdlib.h>
#include "thornifix.h"
#include "thapp.h"

#define THAPP_AHU_DMP_BUFF 100

typedef struct _thapp_ahu thapp_ahu;

struct _thapp_ahu
{
    thapp _var_parent;
    int var_init_flg;

    
    double var_dmp_buff[THAPP_AHU_DMP_BUFF];
    
    /* Array of function pointers */
    struct _thapp_fptr_arr _var_fptr;    
    void* var_child;
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor and destructor */
    thapp* thapp_ahu_new(void);
    void thapp_ahu_delete(thapp_ahu* obj);

    /* Start and stop methods are implemented in the parent class. */

    
    
#ifdef __cplusplus
}
#endif    

#endif /* __THAPP_AHU_H__ */
