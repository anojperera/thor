/* Header file defining interfaces for AHU performance
 * test. When test is started, input and output tasks
 * are created. Output signal is sent to the actuator
 * until the desired static pressure is read.
 * Data is gathered throughout the test.
 * Mon Jun 20 23:39:35 BST 2011 */

#ifndef _THANUP_H_
#define _THANUP_H_

#include <stdio.h>
#if defined (WIN32) || defined (_WIN32)
#include <NIDAQmx.h>
#else
#include <NIDAQmxBase.h>
#endif
#include "thgsens.h"		/* generic sensor */
#include "thvelsen.h"		/* velocity sensor */

/* object conversion macros */
#define AHUP_GET_VELOCITYDT(obj) \
    (obj->var_velocity)

#define AHUP_GET_STATIC(obj) \
    (obj->var_stsensor)

/* control type */
typedef enum
    {
	thahup_auto,		/* auto control */
	thahup_man		/* manual control */
    } thahup_stopctrl;

/* forward declaration of struct */
typedef struct _thahup thahup;

struct _thahup
{
    TaskHandle var_outask;		/* analog output task */
    TaskHandle var_intask;		/* analog input task */

    thgsens* var_stsensor;		/* static pressure sensor */
    thvelsen* var_velocity;		/* velocity sensor collection */
    thgsens* var_tmpsensor;		/* ambient temperature sensor */

    double var_ductdia;			/* duct diameter */
    gthsen_fptr var_volupdate;		/* update volume flow */
    gthsen_fptr var_velupdate;		/* update velocity flow */
    FILE* var_fp;			/* file pointer */
    double* var_actout;			/* array of actuator output */
    double var_actsignal;		/* actuator control signal */
    double var_stopval;			/* value to idle test */
    double var_volflow_val;		/* volume flow */
    double var_velocity_val;		/* velocity */
    double var_static_val;		/* static pressure */
    double var_temp_val;		/* temperature value */
    
    float64 var_smplerate;		/* sample rate */

    int var_thrid;			/* thread id */
    unsigned int var_stflg;		/* start flag */
    unsigned int var_idlflg;		/* idle flag */

    void* var_sobj;			/* object to pass to callback
					 * function */
    thahup_stopctrl var_stctrl;		/* start control */
};

/* C functions */
#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor - creates an AHU performance teest object.
     * Function pointers passed to object to update volume
     * flow update are assigned by function pointers.
     * velocity sensors are set to default range of 0 - 500Pa.
     * Can be overridden externally */
    extern int thahup_initialise(thahup_stopctrl ctrl_st,		/* start control */
				 FILE* fp,				/* file pointer */
				 gthsen_fptr update_vol,		/* function pointer for volume flow */
				 gthsen_fptr update_vel,		/* function pointer for velocity flow */
				 gthsen_fptr update_tmp,		/* function pointer for temperature update */
				 gthsen_fptr update_static,		/* function pointer static pressure */
				 gthsen_fptr update_dp1,		/* function pointer dp 1 */
				 gthsen_fptr update_dp2,		/* function pointer dp 2 */
				 gthsen_fptr update_dp3,		/* function pointer dp 3 */
				 gthsen_fptr update_dp4,		/* function pointer dp 4 */
				 thahup** obj,				/* argument optional */
				 void* sobj);				/* optional argument to pass to
									 * function calls */

    /* Call to stop tasks if already haven't and free
     * memory */
    extern void thahup_delete();

    /* Set duct diameter. Object pointer is
     * optional */
    extern inline void thahup_set_ductdia(thahup* obj,
					  double val);

    /* Start function - starts test in another thread
     * and continues recording until stop function is
     * called. If object pointer was passed if shall be
     * used */
    extern int thahup_start(thahup* obj);

    /* Stop function - stops gathering data and clears
     * tasks. If object pointer was passed it shall
     * be used */
    extern int thahup_stop(thahup* obj);


    /* actuator control voltage percentage */
    extern int thahup_set_actctrl_volt(double percen);

    /* reset all sensor values */
    extern int thahup_reset_sensors(thahup* obj);
#ifdef __cplusplus
}
#endif

#endif /* _THANUP_H_ */
