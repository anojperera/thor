/* Velocity sensor for AHU. Sensor uses 4 dp sensors in
 * an array. Each sensor can be activated independantly.
 * Pressure reading is converted to velocity by Bernuili's.
 * Average velocity is worked out and callback function is called
 * for updating.
 * Mon Jun 20 14:18:54 GMTDT 2011 */

#ifndef __THVELSEN_H__
#define __THVELSEN_H__

#include <stdlib.h>
#include "thgsens.h"
#if defined (WIN32) || defined (_WIN32)
#include <NIDAQmx.h>
#else
#include <NIDAQmxBase.h>
#endif
typedef struct _thvelsen thvelsen;

struct _thvelsen
{
    thgsens* var_v1;		/* sensor 1 */
    thgsens* var_v2;		/* sensor 2 */
    thgsens* var_v3;		/* sensor 3 */
    thgsens* var_v4;		/* sensor 4 */

    double var_ave_vel;		/* average velocity */
    TaskHandle* var_task;	/* task handle */
    gthsen_fptr var_fptr;	/* function pointer */
    
    unsigned int var_sen_cnt;	/* sensor counter */
    void* var_sobj;		/* external object */

    /* flags */
    unsigned int var_v1_flg;
    unsigned int var_v2_flg;
    unsigned int var_v3_flg;
    unsigned int var_v4_flg;

    unsigned int var_okflg;	/* ok flag to indicate
				 * range is set */
    double var_gmin;		/* generic minimum value */
    double var_gmax;		/* generic maximum value */

    /* channel names */
    char* var_ch1;
    char* var_ch2;
    char* var_ch3;
    char* var_ch4;

    /* local copy of function pointers */
    gthsen_fptr var_fptr_s1;	/* function pointer */
    gthsen_fptr var_fptr_s2;	/* function pointer */
    gthsen_fptr var_fptr_s3;	/* function pointer */
    gthsen_fptr var_fptr_s4;	/* function pointer */
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor with task handle */
    int thvelsen_new(thvelsen** obj,
		     TaskHandle* task,		/* task */
		     gthsen_fptr fptr,		/* function pointer */
		     void* data);		/* external data */

    /* destructor */
    void thvelsen_delete(thvelsen** obj);

    /* configure sensors:
     * if all sensors were previously configured
     * passing NULL pointer to channel name and 0 to
     * min and max range shall configure the sensor usin
     * the previous settings */
    int thvelsen_config_sensor(thvelsen* obj,		/* object */
    			       unsigned int ix,		/* index */
			       const char* ch_name,	/* channel name */
			       gthsen_fptr fptr,
			       double min,		/* minimum range */
			       double max);		/* maximum range */

    /* disable sensor */
    int thvelsen_disable_sensor(thvelsen* obj,
				unsigned int ix);

    /* enable sensor */
    int thvelsen_enable_sensor(thvelsen* obj,
			       unsigned int ix);

    double thvelsen_get_velocity(thvelsen* obj);

#ifdef __cplusplus
}
#endif

#endif /* __THVELSEN_H__ */
