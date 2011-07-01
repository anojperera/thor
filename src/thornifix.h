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
#define NIWriteAnalogF64 DAQmxWriteAnalogF64
#define NIReadAnalogF64 DAQmxReadAnalogF64
#define NICreateTask DAQmxCreateTask
#define NICreateAOVoltageChan DAQmxCreateAOVoltageChan
#define NICreateAIVoltageChan DAQmxBaseCreateAIVoltageChan
#else
#include <NIDAQmxBase.h>
#define NIGetErrorString DAQmxBaseGetExtendedErrorInfo
#define NIClearTask DAQmxBaseClearTask
#define NIStartTask DAQmxBaseStartTask
#define NIWriteAnalogF64 DAQmxBaseWriteAnalogF64
#define NIReadAnalogF64 DAQmxBaseReadAnalogF64
#define NIStopTask DAQmxBaseStopTask
#define NICreateTask DAQmxBaseCreateTask
#define NICreateAOVoltageChan DAQmxBaseCreateAOVoltageChan
#define NICreateAIVoltageChan DAQmxBaseCreateAIVoltageChan
#endif

#endif /* _THORNIFIX_H_ */

