/* Implementation of SOV benchmark test
 * Thu Jul 14 16:39:36 GMTDT 2011 */

#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "thsov.h"
#include <pthread.h>
#include <math.h>

/* object */
static thahup* var_thsov = NULL;

/* SOV control channel */
#define THSOV_SOV_CTRL_CHANNEL "dev/ao0"

/* SOV orientation channel
 * SOV - orientation channel roates the orientation of the
 * sov mounting */
#define THSOV_SOV_ORIT_CHANNEL "dev/ao1"

/* Open and close switch channels
 * records activity on switch channels */
#define THSOV_SOV_OPENSW_CHANNEL "dev/ai0"
#define THSOV_SOV_CLOSESW_CHANNEL "dev/ai1"

/* Minimum and maximum orientation */
#define THSOV_ORIT_MIN_ANG 0.0
#define THSOV_ORIT_MAX_ANG 90.0

/* Default control angle */
#define THSOV_ORIT_DEF_ANG 28.5

/* Minimum and maximum orientation voltage */
#define THSOV_ORIT_MIN_VOLT 0.0
#define THSOV_ORIT_MAX_VOLT 10.0

/* Minimum and maximum supply voltage */
#define THSOV_SOV_SUP_MIN_VOLT 0.0
#define THSOV_SOV_SUP_MAX_VOLT 30.0

#define THSOV_SOV_CTRL_MIN_VOLT 0.0
#define THSOV_SOV_CTRL_MAX_VOLT 5.0

/* number of measure ment points */
static int thsov_sup_num_pts = 0;
static int thsov_ctrl_num_pts = 0;

/* Private helper functions */

/* calculate sov control gradient */
static void thsov_calc_ctrl_grad()
{
    var_thsov->var_sov_ctrl_grad =
	THSOV_ORIT_MAX_VOLT / THSOV_ORIT_MAX_ANG;
}

/* calculate SOV supply voltage gradient */
static void thsov_calc_sup_grad()
{
    var_thsov->var_sov_sup_grad =
	THSOV_SOV_CTRL_MAX_VOLT / THSOV_SOV_SUP_MAX_VOLT;
}

/* calculate control voltage for solenoids */
static void thsov_calc_sov_sup_voltage()
{
    /* call to calculate the gradient */
    thsov_calc_sup_grad()
    
    int i = 0;
    /* If not assigned used max voltage */
    if((int) var_thsov->var_start_sov_volt)
	thsov_sup_num_pts = var_thsov->var_start_sov_volt * 2;
    else
	thsov_sup_num_pts = THSOV_SOV_SUP_MAX_VOLT * 2;

    /* create the array */
    var_thsov->var_sov_ctrl =
	(double*) calloc(thsov_sup_num_pts, sizeof(double));
    
    for(; i<thsov_sup_num_pts; i++)
	{
	    var_thsov->var_sov_ctrl[i] =
		var_thsov->var_sov_ctrl_grad *
		var_thsov->var_start_sov_volt *
		cos((1 - i/(double) thsov_sup_num_pts) * M_PI/2);
	}
	
}

/* calculate orientation voltages for SOV */
static void thsov_calc_sov_ctrl_voltage()
{
    /* call to calculate gradient */
    thsov_calc_ctrl_grad();

    /* check if starting angle is assigned */
    if(var_thsov->var_start_or_angl > 0.0 &&
       var_thsov->var_start_or_angl < THSOV_ORIT_MAX_ANG)
	thsov_ctrl_num_pts = 4;
    else
	{
	    thsov_ctrl_num_pts =
		(var_thsov->var_start_or_angl -
		 THSOV_ORIT_MIN_ANG) / THSOV_ORIT_DEF_ANG;
	}
}
