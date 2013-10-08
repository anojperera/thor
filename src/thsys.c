/* Implementation of the system class */
#include <unistd.h>
#include "thsys.h"

/* thread function */
static void* _thsys_start_async(void* para);
static void _thsys_thread_cleanup(void* para);
    
int thsys_init(thsys* obj)
{
    int i;
    if(obj == NULL)
	return -1;

    /* initialise flags */
    obj->var_flg = 0;
    obj->var_client_count = 0;
    obj->var_run_flg = 0;

    /* create tasks */
    ERR_CHECK(NICreateTask(THSYS_EMPTY_STR, &obj->var_a_outask));
    ERR_CHECK(NICreateTask(THSYS_EMPTY_STR, &obj->var_a_intask));

    /* create channels in order */
    ERR_CHECK(NICreateAIVoltageChan(obj->var_a_intask, THSYS_AI_CHANNELS, THSYS_EMPTY_STR,  DAQmx_Val_NRSE, THSYS_MIN_VAL, THSYS_MAX_VAL, DAQmx_Val_Volts, NULL));
    ERR_CHECK(NICreateAOVoltageChan(obj->var_a_outask, THSYS_A0_CHANNELS, THSYS_EMPTY_STR, DAQmx_Val_NRSE, THSYS_MIN_VAL, THSYS_MAX_VAL, NULL));

    /* initialise buffers */
    for(i=0; i<THSYS_NUM_AI_CHANNELS; i++)
	obj->var_inbuff[i] = 0.0;
    for(i=0; i<THSYS_NUM_AO_CHANNELS; i++)
	obj->var_outbuff[i] = 0.0;
    obj->var_sample_rate = THSYS_DEFAULT_SAMPLE_RATE;
    obj->var_flg = 1;
    return 0;
}

/* Delete object pointer */
void thsys_delete(thsys* obj)
{
    if(!obj->var_flg || obj->var_client_coount == 0)
	return;

    NIStopTask(obj->var_a_outask);
    NIStopTask(obj->var_a_intask);

    NIClearTask(obj->var_a_outask);
    NIClearTask(obj->var_a_intask);

    obj->var_flg = 0;
    obj->var_client_count = 0;
    obj->var_run_flg = 0;

    syslog (LOG_INFO, "thor system stopped");
    return;
}

/* Start system */
int thsys_start(thsys* obj)
{
    pthread_attr_t _attr;					/* thread attribute */
    if(obj == NULL)
	return -1;

    if(!obj->var_flg)
	return -1;

    /* indicate running */
    if(obj->var_run_flg)
	return 0;

    /* configure timing and start tasks */
    ERR_CHECK(NICfgSampClkTiming(obj->var_a_intask, THSYS_CLOCK_SOURCE, obj->var_sample_rate, DAQmx_Val_Rising, DAQmx_Val_ContSamps, 1));

    ERR_CHECK(NIStartTask(obj->var_a_intask));
    ERR_CHECK(NIStartTask(obj->var_a_outask));

    /* initialise thread attribute */
    pthread_attr_init(&_attr);
    pthread_attr_setdetachstate(&_attr, PTHREAD_CREATE_DETACHED);

    /* creat thread */
    pthread_create(&obj->var_thread, &_attr, _thsys_start_async, (void*) obj);

    /* destroy attribute object as we no longer need it */
    pthread_attr_destroy(&_attr);

    obj->var_run_flg = 1;
    return 0;
}

/* stop test */
int thsys_stop(thsys* obj)
{
    if(obj == NULL)
	return -1;
    
    if(!obj->var_run_flg)
	return -1;

    /* cancel thread */
    pthread_cancel(obj->var_thread);
    return 0;
}

/* Thread function */
static void* _thsys_start_async(void* para)
{
    int _old_state;
    thsys* _obj;
    int32 _samples_read = 0;

    /* push cleanup handler */
    pthread_cleanup_push(_thsys_thread_cleanup, para);
    
    /* set thread cancel state to cancellable */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &_old_state);
    _obj = (thsys*) obj;
    while(1)
	{
	    /* test for cancel state */
	    pthread_testcancel();
	    ERR_CHECK(NIReadAnalogF64(_obj->var_a_intask, 1, 1.0, DAQmx_Val_GroupByChannel, _obj->var_inbuff, THSYS_NUM_AI_CHANNELS, _samples_read, NULL));
	    usleep(1000 /(_obj->var_sample_rate*THSYS_READ_WRITE_FACTOR));
	}
    
    pthread_exit(para);
    return NULL;
}

/* thread cleanup handler */
static void _thsys_thread_cleanup(void* para)
{
    thsys* _obj;
    if(para == NULL)
	return;

    _obj = (thsys*) para;

    /* stop tasks */
    NIStopTask(_obj->var_a_outask);
    NIStopTask(_obj->var_a_intask);
    obj->var_run_flg = 0;
    return;
 }
