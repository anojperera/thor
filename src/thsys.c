/* Implementation of the system class */
#include <unistd.h>
#include "thsys.h"

/* thread function */
static void* _thsys_start_async(void* para);
static void _thsys_thread_cleanup(void* para);

int thsys_init(thsys* obj, int (*callback) (thsys*, void*))
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

    ERR_CHECK(NICreateAOVoltageChan(obj->var_a_outask, THSYS_A0_CHANNELS, THSYS_EMPTY_STR, THSYS_MIN_VAL, THSYS_MAX_VAL, DAQmx_Val_Volts , NULL));
    ERR_CHECK(NICreateAIVoltageChan(obj->var_a_intask, THSYS_AI_CHANNELS, THSYS_EMPTY_STR,  DAQmx_Val_NRSE, THSYS_MIN_VAL, THSYS_MAX_VAL, DAQmx_Val_Volts, NULL));

    /* initialise buffers */
    for(i=0; i<THSYS_NUM_AI_CHANNELS; i++)
	obj->var_inbuff[i] = 0.0;
    for(i=0; i<THSYS_NUM_AO_CHANNELS; i++)
	obj->var_outbuff[i] = 0.0;
    
    obj->var_sample_rate = THSYS_DEFAULT_SAMPLE_RATE;
    obj->var_callback_intrupt = callback;
    obj->var_callback_update = NULL;
    obj->var_ext_obj = NULL;
    obj->var_flg = 1;
    sem_init(&obj->var_sem, 0, 0);

    THOR_LOG_ERROR("thor system initialised");

    return 0;
}

/* Delete object pointer */
void thsys_delete(thsys* obj)
{
  /* Stop test regardless of number of connections */
    if(!obj->var_flg)
	return;

    if(obj->var_run_flg)
      {
	/*
	 * Call e stop to stop the test.
	 * Wait until, test is stopped then
	 * clean up resources.
	 */
	thsys_e_stop(obj);
	sem_wait(&obj->var_sem);

      }
    NIClearTask(obj->var_a_outask);
    NIClearTask(obj->var_a_intask);

    obj->var_flg = 0;
    obj->var_client_count = 0;
    obj->var_run_flg = 0;
    obj->var_callback_intrupt = NULL;
    obj->var_callback_update = NULL;
    obj->var_ext_obj = NULL;
    sem_destroy(&obj->var_sem);
    THOR_LOG_ERROR("thor system cleaned up");
    return;
}

/* Start system */
int thsys_start(thsys* obj)
{
    if(obj == NULL)
	return -1;

    if(!obj->var_flg)
	return -1;
    obj->var_client_count++;
    /* indicate running */
    if(obj->var_run_flg)
	return 0;

    /* configure timing and start tasks */
    ERR_CHECK(NICfgSampClkTiming(obj->var_a_intask, THSYS_CLOCK_SOURCE, obj->var_sample_rate, DAQmx_Val_Rising, DAQmx_Val_ContSamps, 1));

    THOR_LOG_ERROR("thor timer configure complete");
    
    /* creat thread */
    pthread_create(&obj->var_thread, NULL, _thsys_start_async, (void*) obj);

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

    /*
     * Decrement client connections and cancel the thread.
     * If the test is still running put to stop.
     */
    if((--obj->var_client_count) > 0)
	return 0;
    
    /* cancel thread */
    pthread_cancel(obj->var_thread);
    pthread_join(obj->var_thread, NULL);
    return 0;
}

int thsys_e_stop(thsys* obj)
{
    /* Check for object pointer */
    if(obj == NULL)
      return -1;

    /* Decrement client count */
    obj->var_client_count = 1;
    return thsys_stop(obj);
}

/* set write buffer */
int thsys_set_write_buff(thsys* obj, float64* buff, size_t sz)
{
    int32 _samples;
    /* check for object and buffer */
    if(obj == NULL || !buff || !obj->var_run_flg)
	return -1;

    /* check size of buffer with internal */
    if(sz != THSYS_NUM_AO_CHANNELS)
	return 1;

    /* Write buffer to the device */
    ERR_CHECK(NIWriteAnalogArrayF64(obj->var_a_outask, 1, 0, 1.0, DAQmx_Val_GroupByChannel, buff, &_samples, NULL));

    return _samples;
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
    _obj->var_run_flg = 0;
    sem_post(&_obj->var_sem);

    THOR_LOG_ERROR("thor system stopped");
    return;
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

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    
    _obj = (thsys*) para;    
    THOR_LOG_ERROR("thor system started");
    
    ERR_CHECK(NIStartTask(_obj->var_a_intask));
    ERR_CHECK(NIStartTask(_obj->var_a_outask));    

    while(1)
    	{
    	    /* test for cancel state */
    	    pthread_testcancel();
	    
    	    /* if callback was set exec */
    	    if(_obj->var_callback_intrupt)
	        _obj->var_callback_intrupt(_obj, (_obj->var_ext_obj? _obj->var_ext_obj : NULL));

	    /* change cancel state to protect read */
	    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &_old_state);
    	    ERR_CHECK(NIReadAnalogF64(_obj->var_a_intask, 1, 1.0, DAQmx_Val_GroupByChannel, _obj->var_inbuff, THSYS_NUM_AI_CHANNELS, &_samples_read, NULL));

	    /* if a callback for update is hooked, this shall call the callback function */
	    if(_obj->var_callback_update)
	      _obj->var_callback_update(_obj, _obj->var_ext_obj, _obj->var_inbuff, THSYS_NUM_AI_CHANNELS);
	    pthread_setcancelstate(_old_state, NULL);

    	    pthread_testcancel();
    	    usleep(1000 /(_obj->var_sample_rate*THSYS_READ_WRITE_FACTOR));
    	}

    pthread_cleanup_pop(1);
    return NULL;
}
