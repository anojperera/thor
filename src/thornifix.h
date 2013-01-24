/* header file for linux portability */

#ifndef _THORNIFIX_H_
#define _THORNIFIX_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

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
#define THORNIFIX_P2_Y {0.3379, 0.3972, 0.4005, 0.6791, 0.9025, 1.4407, 1.6880, 1.9831}

#define THORNIFIX_P4_X {0.28, 2.49, 12.05, 46.55, 109.52, 201.63, 338.43, 497.33}
#define THORNIFIX_P4_Y {0.3168, 0.4628, 0.5185, 1.1918, 1.4895, 1.6683, 1.2502, 1.2096}

/* Generic callback function */
typedef unsigned int (*gthor_fptr)(void*, void*);
typedef unsigned int (*gthor_disb_fptr)(void*, int);

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
	    fprintf(stderr, "%s\n", err_msg);
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

inline __attribute__ ((always_inline)) static double Mean(const double* data, size_t num)
{
    double val = 0.0;
    int i;
    for(i=0; i<num; i++)
	val += data[i];
    return val / (num <= 0? 1.0 : (double) num);
}

/* polynomial interpolating function */
static int thor_interpol(const double* x, const double* y, int n, double* z, double* fz, int m)
{
    int i, j, k;
    double* _tbl, *_coef;
    double _t;

    /* allocate storage */
    _tbl = (double*) calloc(n, sizeof(double));
    if(_tbl == NULL)
	return 1;
    _coef = (double*) calloc(n, sizeof(double));
    if(_coef == NULL)
	{
	    free(_tbl);
	    return 1;
	}
    /* initialise coefficnets */
    for(i=0; i<n; i++)
	_tbl[i] = y[i];

    /* work out the coefficients of the interpolating polynomial */
    _coef[0] = _tbl[0];

    for(k=1; k<n; k++)
	{
	    for(i=0; i<n-k; i++)
		{
		    j=i+k;
		    _tbl[i] = (_tbl[i+1] - _tbl[i]) / (x[j] - x[i]);
		}
	    _coef[k] = _tbl[0];
	}
    free(_tbl);

    /* work out interpolating polynomial specified points */
    for(k=0; k<m; k++)
	{
	    fz[k] = _coef[0];
	    for(j=1; j<n; j++)
		{
		    _t = _coef[j];
		    for(i=0; i<j; i++)
			_t = _t * (z[k] - x[i]);

		    fz[k] += _t;
		}
	}
    free(_coef);
    return 0;
}

#endif /* _THORNIFIX_H_ */

