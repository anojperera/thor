/* Header file for SOV interface test
 * Thu Jul 14 13:39:12 GMTDT 2011 */

#ifndef __THSOV_H__
#define __THSOV_H__

#include <stdio.h>
#if defined (WIN32) || defined (_WIN32)
#include <NIDAQmx.h>
#else
#include <NIDAQmxBase.h>
#endif
#include "thgsens.h"		/* generic sensor */

/* struct for SOV */
typedef struct _thsov thsov;

typedef enum {
    thsov_dmp_open,
    thsov_dmp_close,
    thsov_dmp_err
} thsov_dmp_state;

/* Function pointer for updating damper state */
typedef unsigned int (*gthsov_dmp_state_fptr)(void*, thsov_dmp_state);

struct _thsov
{
    TaskHandle var_outask;		/* analog output task */
    TaskHandle var_intask;		/* analog input task */

    FILE* var_fp;			/* file pointer */

    double* var_sov_ang_volt;		/* control voltage array */
    double* var_sov_sup_volt;		/* sov supply voltage array */
    double var_sov_start_angle;		/* starting angle */
    double var_sov_supply_volt;		/* supply voltage */

    /* supply and angle gradients */
    double var_sov_st_ang_grad;
    double var_sov_sup_volt_grad;

    thsov_dmp_state var_dmp_state;	/* damper / actuator state */

    unsigned int var_cyc_cnt;		/* cycle counter */
    double var_sov_ang_fd;		/* angle feedback */
    double var_sov_sup_fd;		/* supply feedback */
    
    float64 var_write_array[2];		/* write array to 2-channels */

    thgsens* var_tmp_sensor;		/* temperature sensor */

    /******* Function Pointers to update functions ********/
    /* damper state change pointer */
    gthsov_dmp_state_fptr var_state_update;
    gthsen_fptr var_ang_update;		/* SOV orientation angle update */
    gthor_fptr var_cyc_update;		/* cycle count update */
    gthsen_fptr var_volt_update;		/* voltage update */
    /******************************************************/
    
    void* var_sobj;			/* pointer to external object */
    unsigned int var_arr_init;		/* array initialised */
    unsigned int var_stflg;
    float var_milsec_wait;		/* Miliseconds to wait */
    int var_thrid;
};

/* C functions */
#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor */
    int thsov_initialise(gthsen_fptr tmp_update,		/* update temperature */
			 gthsov_dmp_state_fptr dmp_update,	/* damper open and close */
			 gthsen_fptr sov_angle_update,		/* SOV orientation update */
			 gthor_fptr cyc_update,			/* Cycle update */
			 gthsen_fptr volt_update,		/* Voltage update */
			 double sov_suppy_start,		/* Supply start voltage */
			 double sov_angle_start,		/* Starting angle */
			 double sov_wait_time,			/* SOV wait time */
			 thsov** obj,				/* Optional - pointer to local object */
			 FILE* fp,				/* File pointer */
			 void* sobj);				/* pointer to external object */

    /* Destructor */
    void thsov_delete(thsov** obj);

    extern inline int thsov_set_voltage_and_angle(thsov* obj,		/* Pointer to object */
						  double st_volt,	/* Starting voltage */
						  double st_ang);	/* Starting angle */

    /* Set wait time before damper open/close */
    extern inline int thsov_set_wait_time(thsov* obj,
					  double wait_time);

    /* Reset internal variable values */
    extern inline int thsov_reset_values(thsov* obj);

    /* Start function - starts tests in another thread.
     * and continues to record until stop function is
     * called. If object pointer is passed it shall be
     * used */
    int thsov_start(thsov* obj);

    /* Stop function - stops gather data and clears
     * tasks. If object pointer was passed, it shall
     * be used */
    int thsov_stop(thsov* obj);
			 
			 
#ifdef __cplusplus
}
#endif

#endif /* __THSOV_H__ */
