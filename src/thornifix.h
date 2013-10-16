/* header file for linux portability */

#ifndef _THORNIFIX_H_
#define _THORNIFIX_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <syslog.h>

/* #define THOR_HEADLESS */
#ifdef THOR_HEADLESS
#define THOR_LOG_ERROR(msg)			\
  syslog (LOG_INFO, msg)
#else
#define THOR_LOG_ERROR(msg)			\
  fprintf(stderr, "%s\n", msg)
#endif

#if defined (WIN32) || defined (_WIN32)
#include <NIDAQmx.h>
/* get error string */
#define NIGetErrorString DAQmxGetErrorString
#define NIClearTask DAQmxClearTask
#define NIStartTask DAQmxStartTask
#define NIStopTask DAQmxStopTask
#define NIWriteAnalogF64 DAQmxWriteAnalogScalarF64
#define NIWriteAnalogArrayF64 DAQmxWriteAnalogF64
#define NIReadAnalogF64 DAQmxReadAnalogF64
#define NICreateTask DAQmxCreateTask
#define NICreateAOVoltageChan DAQmxCreateAOVoltageChan
#define NICreateAIVoltageChan DAQmxCreateAIVoltageChan
#define NICfgSampClkTiming DAQmxCfgSampClkTiming
#define NIRegisterEveryNSamplesEvent DAQmxRegisterEveryNSamplesEvent
#define NIRegisterDoneEvent DAQmxRegisterDoneEvent
#else
#include <NIDAQmxBase.h>
#define NIGetErrorString DAQmxBaseGetExtendedErrorInfo
#define NIClearTask DAQmxBaseClearTask
#define NIStartTask DAQmxBaseStartTask
#define NIWriteAnalogF64 DAQmxBaseWriteAnalogF64
#define NIWriteAnalogArrayF64 DAQmxBaseWriteAnalogF64
#define NIReadAnalogF64 DAQmxBaseReadAnalogF64
#define NIStopTask DAQmxBaseStopTask
#define NICreateTask DAQmxBaseCreateTask
#define NICreateAOVoltageChan DAQmxBaseCreateAOVoltageChan
#define NICreateAIVoltageChan DAQmxBaseCreateAIVoltageChan
#define NICfgSampClkTiming DAQmxBaseCfgSampClkTiming
#define CVICALLBACK __cdecl
#endif

#define THOR_BUFF_SZ 2048

/* pressure sensor calibration factors */
#define THORNIFIX_S1_X {0.0101, 2.4725, 4.9778, 7.4767, 9.9998}			/* INO 167 */
#define THORNIFIX_S1_Y {0.10, -0.21, -0.25, -0.24, -0.16}			/* INO 167 */

#define THORNIFIX_S2_X {0.0103, 2.5074, 5.0248, 7.5286, 10.0297}		/* INO 168 */
#define THORNIFIX_S2_Y {0.10, 0.07, 0.19, 0.25, 0.22}				/* INO 168 */

#define THORNIFIX_S3_X {0.0082, 2.4855, 4.9896, 7.4799, 9.9921}			/* INO 169 */
#define THORNIFIX_S3_Y {0.08, -0.14, -0.21, -0.21, -0.24}			/* INO 169 */

#define THORNIFIX_S4_X {0.0095, 2.4868, 5.0015, 7.5009, 10.0011}		/* INO 170 */
#define THORNIFIX_S4_Y {0.10, -0.08, -0.09, -0.07, -0.06}			/* INO 170 */

#define THORNIFIX_ST_X {0.0096, 2.3933, 4.7914, 7.1875, 9.5614}			/* INO 171 */
#define THORNIFIX_ST_Y {0.10, -0.03, -0.09, -0.20, -0.36}			/* INO 171 */
#define THORNIFIX_S_CAL_SZ 5

/* velocity sensor calibration correction factors */
/* Cert No - 1239420006 */
#define THORNIFIX_P1_X {0.353, 3.337, 13.503, 50.203, 116.407, 220.200, 357.467, 525.033}
#define THORNIFIX_P1_Y {0.2329, 0.1416, 0.2560, 0.8527, 1.0711, 0.8427, 0.5914, 0.4186}

/* Cert No - 1239420007 */
#define THORNIFIX_P2_X {0.277, 2.510, 12.107, 46.297, 108.700, 203.133, 319.733, 459.033}
#define THORNIFIX_P2_Y {0.3205, 0.4546, 0.5079, 1.2158, 1.5401, 1.6001, 1.9156, 2.3403}

/* Cert No - 1239420008 */
#define THORNIFIX_P3_X {0.263, 2.653, 12.693, 52.127, 119.243, 206.667, 326.067, 470.967}
#define THORNIFIX_P3_Y {0.3379, 0.3972, 0.4005, 0.6791, 0.9025, 1.4407, 1.6880, 1.9831}

/* Cert No - 1239420009 */
#define THORNIFIX_P4_X {0.28, 2.49, 12.05, 46.55, 109.52, 201.63, 338.43, 497.33}
#define THORNIFIX_P4_Y {0.3168, 0.4628, 0.5185, 1.1918, 1.4895, 1.6683, 1.2502, 1.2096}
#define THORNIFIX_P_CAL_SZ 8

/* Generic callback function */
typedef unsigned int (*gthor_fptr)(void*, void*);
typedef unsigned int (*gthor_disb_fptr)(void*, int);


/*===========================================================================*/
/* Message handling methods and macros */
#define THORNIFIX_MSG_ELM_NUM 16
#define THORNIFIX_MSG_BUFF_ELM_SZ 8
#define THORNIFIX_MSG_BUFF_SZ THORNIFIX_MSG_ELM_NUM*THORNIFIX_MSG_BUFF_ELM_SZ

/* Message struct aligned - all members are eight byte alinged */
struct thor_msg
{
    /* command to indicate read or write */
    int _cmd __attribute__ ((aligned (8)));

    /* analogue output channels */
    /*--------------------------------------------*/
    double _ai0_val __attribute__ ((aligned (8)));
    double _ai1_val __attribute__ ((aligned (8)));
    /*--------------------------------------------*/

    /* analogue input channels */
    /*--------------------------------------------*/    
    double _ao0_val __attribute__ ((aligned (8)));
    double _ao1_val __attribute__ ((aligned (8)));
    double _ao2_val __attribute__ ((aligned (8)));
    double _ao3_val __attribute__ ((aligned (8)));
    double _ao4_val __attribute__ ((aligned (8)));
    double _ao5_val __attribute__ ((aligned (8)));
    double _ao6_val __attribute__ ((aligned (8)));
    double _ao7_val __attribute__ ((aligned (8)));
    double _ao8_val __attribute__ ((aligned (8)));
    double _ao9_val __attribute__ ((aligned (8)));
    double _ao10_val __attribute__ ((aligned (8)));
    /*--------------------------------------------*/
    
    /* digital input channels */
    /*--------------------------------------------*/
    double _di0_val __attribute__ ((aligned (8)));
    double _di1_val __attribute__ ((aligned (8)));
    /*--------------------------------------------*/    
};

/* size of the message struct */
#define THORINIFIX_MSG_SZ			\
    sizeof(struct thor_msg)

/* initialise message struct */
#define thorinifix_init_msg(obj)		\
    memset((void*) (obj), 0, THORINIFIX_MSG_SZ)

#define thornifix_encode_msg(obj, buff, size) \
    thorinifix_init_msg(obj); \
    memset((void*) (buff), 0, size); \
    sprintf((buff),		     \
	    "%i|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|"		     \
	    "%.2f|",		     \
	    (obj)->_cmd,	     \
	    (obj)->_a00,	     \
	    (obj)->_a01,	     \
	    (obj)->_ai0,	     \
	    (obj)->_ai1,	     \
	    (obj)->_ai2,	     \
	    (obj)->_ai3,	     \
	    (obj)->_ai4,	     \
	    (obj)->_ai5,	     \
	    (obj)->_ai6,	     \
	    (obj)->_ai7,	     \
	    (obj)->_ai8,	     \
	    (obj)->_ai9,	     \
	    (obj)->_ai10,	     \
	    (obj)->_di0,	     \
	    (obj)->_di1)

/* decode message */
int thornifix_decode_msg(const char* buff, size_t size, struct thor_msg* msg);

/*===========================================================================*/
/* error check function */
inline __attribute__ ((always_inline)) static int ERR_CHECK(int32 err)
{
    static char err_msg[THOR_BUFF_SZ] = {'\0'};

    /* get error message */
    if(err)
	{
#if defined (WIN32) || defined (_WIN32)
	    NIGetErrorString (err, err_msg, THOR_BUFF_SZ);
#else
	    NIGetErrorString(err_msg, THOR_BUFF_SZ);
#endif
	    THOR_LOG_ERROR(err_msg);
	    return 1;
	}
    else
	return 0;

}

inline __attribute__ ((always_inline)) static double Round(double val, unsigned int places)	/* rounds a number */
{
    double off = pow(10, places);

    double x = val * off;
    double b = 0;
    double i_part = 0;


    if(modf(x, &i_part) >= 0.5)
	b = (x>=i_part? ceil(x) : floor(x));
    else
	b = (x<i_part? ceil(x) : floor(x));

    return b/off;

}

inline __attribute__ ((always_inline)) static double Mean(const double* data, unsigned int num)
{
    double val = 0.0;
    double div = 0.0;
    unsigned int i;
    for(i=0; i<num; i++,div+=1.0)
    	val += data[i];
     return val / div;
}

/* polynomial interpolating function */
int thor_interpol(const double* x, const double* y, int n, double* z, double* fz, int m);

#endif /* _THORNIFIX_H_ */

