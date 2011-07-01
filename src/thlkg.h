/* Leakage test program interface.
 * Program takes input from dp gauge, static gauge and
 * temperature gauge and calculates leakage based on orifice
 * diameter. 
 * Sun May 15 15:25:57 BST 2011 */

#ifndef _THLKG_H_
#define _THLKG_H_

#include <stdio.h>
#include <math.h>
#if defined (WIN32) || defined (_WIN32)
#include <NIDAQmx.h>
#else
#include <NIDAQmxBase.h>
#endif
#include <pthread.h>

#include "thgsens.h"

/* start type */
typedef enum
    {
	thlkg_lkg,		/* fan control until leakage rate */
	thlkg_pr,		/* fan controlled until static */
	thlkg_cont,		/* manual start */
    } thlkg_stopctrl;

/* orifice diameter */
typedef enum
    {
	thlkg_ordia1,		/* 20 dia */
	thlkg_ordia2,		/* 30 dia */
	thlkg_ordia3,		/* 40 dia */
	thlkg_ordia4,		/* 60 dia */
	thlkg_ordia5,		/* 80 dia */
	thlkg_ordia6		/* 100 dia */
    } thlkg_orifice_plate;

typedef struct _thlkg thlkg;

struct _thlkg
{
    TaskHandle var_outask;		/* analog output task */
    TaskHandle var_intask;		/* analog input task */
    thgsens* var_dpsensor;		/* differential pressure sensor */
    thgsens* var_stsensor;		/* static sensor */
    thgsens* var_tmpsensor;		/* temperature sensor */
    gthsen_fptr var_lkgupdate;		/* function pointer to update leakage */
    double var_stopval;			/* value to stop controlling fan */
    thlkg_stopctrl var_stoptype;	/* stop type to control fan, in ctrl
					 * mode fan control is not attempted */
    FILE* var_fp;			/* file pointer */
    thlkg_orifice_plate var_dia_orf;	/* orifice plate diameter */
    float64 var_smplerate;		/* sample rate */
    double* var_fanout;			/* array of fan control signals */
    double var_leakage;			/* leakage per damper */
    double var_fansignal;		/* fan signal */
    int var_thrid;			/* thread id */

    unsigned int var_stflg;		/* start flag */
    unsigned int var_idlflg;		/* idle flag */
    void* sobj_ptr;			/* object to pass to callback functions */
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor - creates and leakge test object.
     * function pointers passed to update leakage,
     * static pressure and differential pressure.
     * Pass file pointer to insert results into a file.
     * Optionally pass object pointer which shall return
     * the internal object pointer */
    extern int thlkg_initialise(thlkg_stopctrl ctrl_st,		/* start control */
				thlkg_orifice_plate dia,	/* orifice plate dia */
				gthsen_fptr update_lkg,		/* leakage update */
				gthsen_fptr update_st,		/* static upate */
				gthsen_fptr update_dp,		/* dp update */
				FILE* fp,			/* file pointer */
				thlkg** obj,			/* argument is optional */
				void* sobj);			/* optional object to passed
								 * to callback functions */
    /* Call stop task functions if already haven't
     * and free memory */
    extern void thlkg_delete();

    /* Start function - starts test in another thread.
     * and continues recording until stop function is called.
     * If object pointer was passed it shall be used */
    extern int thlkg_start(thlkg* obj);

    /* Stop function - stops gathering data and clears
     * tasks. If object pointer was passed, it shall be
     * used */
    extern int thlkg_stop(thlkg* obj);

    /* control fan */
    extern int thlkg_set_fanctrl_volt(double percen);

    extern int thlkg_reset_sensors(thlkg* obj);

#ifdef __cplusplus
}
#endif

#endif /* _THLKG_H_ */
