/* header file for linux portability */

#ifndef _THORNIFIX_H_
#define _THORNIFIX_H_

#include <stdlib.h>
#include <stdio.h>
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
#define THORNIFIX_S1_X {0.0101, 2.4725, 4.9778, 7.4767 9.9998}			/* INO 167 */
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
/* Generic callback function */
typedef unsigned int (*gthor_fptr)(void*, void*);
typedef unsigned int (*gthor_disb_fptr)(void*, int);

/* error check function */
static inline int ERR_CHECK(int32 err)
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
    _tbl = (double*) malloc(sizeof(double)*n);
    if(_tbl == NULL)
	return 1;
    _coef = (double*) calloc(n, sizeof(double));
    if(_coef == NULL)
	{
	    free(_tbl);
	    return 1;
	}
    /* initialise coefficnets */
    memcpy(_tbl, y, sizeof(double)*n);

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

		    _fz[k] += _t;
		}
	}
    free(_coef);
    return 0;
}

#endif /* _THORNIFIX_H_ */

