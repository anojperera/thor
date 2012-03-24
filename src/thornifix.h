/* header file for linux portability */

#ifndef _THORNIFIX_H_
#define _THORNIFIX_H_

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

static inline double Round(double val, unsigned int places)	/* rounds a number */
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
static inline double Mean(const double* data, size_t num)
{
    dobule val = 0.0;
    int i;
    for(i=0; i<num; i++)
	val += data[i];
    return val / (num <= 0? 1.0 : (double) num);
}
#endif /* _THORNIFIX_H_ */

