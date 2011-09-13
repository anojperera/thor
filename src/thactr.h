/* Header file for defining interfaces for actuator
 * reliability test. The actuator shall be cycled
 * number of times until failure
 * Tue Sep 13 09:58:31 GMTDT 2011 */

#ifndef __THACTR_H__
#define __THACTR_H__

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined (WIN32) || defined (_WIN32)
#include <NIDAQmx.h>
#else
#include <NIDAQmxBase.h>
#endif

#include <pthread.h>
#include "thornifix.h"

typedef struct _thactr thactr;

struct _thactr
{
    TaskHandle var_outask;		/* Out task */
    TaskHandle var_intask;		/* In task */

    unsigned int var_cyc_cnt;		/* Cycle count */
    double var_idle_time;		/* Idle time */
    unsigned int var_relay_ix;		/* relay index */

    gthor_fptr var_update_fnc;		/* Update function */
    void* sobj_ptr;			/* object to pass to callback function */
};

#ifdef __cplusplus
extern "C" {
#endif

    extern int thactr_initialise(gthor_fptr update_cycle,	/* Function pointer to update cycle */
				 FILE* fp,			/* FILE pointer */
				 thactr** obj,			/* pointer to internal object */
				 void* sobj);			/* object to pass to callback */

    extern void thactr_delete();

    /* Start function - starts test in another thread.
     * and continues recording until stop function is called.
     * If object pointer was passed it shall be used */
    extern int thactr_start(thactr* obj);

    /* Stop function - stops gathering data and clears
     * tasks. If object pointer was passed, it shall be
     * used */
    extern int thactr_stop(thactr* obj);

    /* Reset internal counters */
    extern inline int thactr_reset_all(thactr* obj);

#ifdef __cplusplus
}
#endif

#endif /* __THACTR_H__ */
