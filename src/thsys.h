/* Generic class for handling the main loop */
#ifndef __THSYS_H__
#define __THSYS_H__

#include <stdlib.h>
#include <string.h>
#include "thornifix.h"

#define THSYS_AI_CHANNELS "Dev1/ai0:10";
#define THSYS_A0_CHANNELS "Dev1/ao0:1";
#define THSYS_NUM_AI_CHANNELS 11
#define THSYS_NUM_AO_CHANNELS 2
#define THSYS_MIN_VAL 0.0
#define THSYS_MAX_VAL 10.0

typedef struct _thsys thsys;

struct _thsys
{
    int var_flg;
    int var_client_count;
    int var_run_flg;			/* flag to indicate system is running */
    
    /* Analog tasks */
    TaskHandle var_a_outask;		/* analog output task */
    TaskHandle var_a_intask;		/* analog input task */

    /* Digital tasks */
    TaskHandle var_d_outask;		/* analog output task */    
    TaskHandle var_d_intask;		/* analog input task */

    float64 var_sample_rate;		/* sample rate */
    float64 var_inbuff[THSYS_NUM_AI_CHANNELS];
    float64 var_outbuff[THSYS_NUM_AO_CHANNELS];
};

#ifdef __cplusplus
extern "C" {
#endif

    /* initialise system struct */
    int thsys_init(thsys* obj);
    int thsys_delete(thsys* obj);

    /* start method */
    int thsys_start(thsys* obj);
    int thsys_stop(thsys* obj);
    
#ifdef __cplusplus
}
#endif

#endif /* __THSYS_H__ */
