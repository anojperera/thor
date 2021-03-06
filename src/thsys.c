/* Implementation of the system class */
#include <unistd.h>
#include "thsys.h"

#define THSYS_USEC_CONV 1000000
#define THSYS_UPDATE_RATE (THSYS_USEC_CONV / 2)

/* thread function */
static void* _thsys_start_async(void* para);
static void _thsys_thread_cleanup(void* para);
static void _thsys_queue_del_helper(void* data);

/*
 * Helper macros for configuring the channels.
 */
#define THSYS_CLEAR_TASKS(sys_obj)		\
    NIClearTask((sys_obj)->var_a_outask);	\
    NIClearTask((sys_obj)->var_a_intask)


#define THSYS_CREATE_TASKS(sys_obj)					\
    if(ERR_CHECK(NICreateTask(THSYS_EMPTY_STR, &(sys_obj)->var_a_outask))) \
	(sys_obj)->var_g_panic_flg = 1;					\
    if(ERR_CHECK(NICreateTask(THSYS_EMPTY_STR, &(sys_obj)->var_a_intask))) \
	(sys_obj)->var_g_panic_flg = 1

#define THSYS_CONFIG_CHANNELS(sys_obj)					\
    if(ERR_CHECK(NICreateAOVoltageChan((sys_obj)->var_a_outask, THSYS_A0_CHANNELS, THSYS_EMPTY_STR, THSYS_MIN_VAL, THSYS_MAX_VAL, DAQmx_Val_Volts , NULL))) \
	(sys_obj)->var_g_panic_flg = 1;					\
    if(ERR_CHECK(NICreateAIVoltageChan((sys_obj)->var_a_intask, THSYS_AI_CHANNELS, THSYS_EMPTY_STR,  DAQmx_Val_NRSE, THSYS_MIN_VAL, THSYS_MAX_VAL, DAQmx_Val_Volts, NULL))) \
	(sys_obj)->var_g_panic_flg = 1

/* Initialise method */
int thsys_init(thsys* obj, int (*callback) (thsys*, void*))
{
    int i;
    if(obj == NULL)
	return -1;

    /* initialise flags */
    obj->var_flg = 0;
    obj->var_client_count = 0;
    obj->var_run_flg = 0;
    obj->var_g_panic_flg = 0;

    /* create tasks */
    THSYS_CREATE_TASKS(obj);
    if(obj->var_g_panic_flg)
	{
	    THOR_LOG_ERROR("thor unable to create tasks");
	    return -1;
	}

    /*
     * Create channels in order. If it failed at this point
     * clear the task and exit.
     */
    THSYS_CONFIG_CHANNELS(obj);
    if(obj->var_g_panic_flg)
	{
	    THSYS_CLEAR_TASKS(obj);
	    THOR_LOG_ERROR("thor system failed to initialised");
	    return -1;
	}

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

    pthread_mutex_init(&obj->_var_mutex, NULL);
    /* Initialise the output message queue */
    gqueue_new(&obj->_var_out_queue, _thsys_queue_del_helper);

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
    THSYS_CLEAR_TASKS(obj);

    obj->var_flg = 0;
    obj->var_client_count = 0;
    obj->var_run_flg = 0;
    obj->var_callback_intrupt = NULL;
    obj->var_callback_update = NULL;
    obj->var_ext_obj = NULL;

    /* Delete queue */
    gqueue_delete(&obj->_var_out_queue);

    sem_destroy(&obj->var_sem);
    pthread_mutex_destroy(&obj->_var_mutex);
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

    /*
     * Reconfigure the tasks for the next start.
     * Set init flag to 0.
     */
    obj->var_flg = 0;
    THSYS_CLEAR_TASKS(obj);
    THSYS_CREATE_TASKS(obj);

    THSYS_CONFIG_CHANNELS(obj);
    obj->var_flg = 1;
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
    int i;
    float64* _buff;

    /* check for object and buffer */
    if(obj == NULL || !buff || !obj->var_run_flg)
	return -1;

    /* check size of buffer with internal */
    if(sz != THSYS_NUM_AO_CHANNELS)
	return 1;

    /* allocate buffer */
    _buff = (float64*) calloc(sz, sizeof(float64));
    for(i=0; i<sz; i++)
	_buff[i] = buff[i];

    /* Queue the values */
    pthread_mutex_lock(&obj->_var_mutex);
    gqueue_in(&obj->_var_out_queue, (void*) _buff);
    pthread_mutex_unlock(&obj->_var_mutex);

    return 0;
}

/* thread cleanup handler */
static void _thsys_thread_cleanup(void* para)
{
    int32 _samples;
    float64 _buff[THSYS_NUM_AO_CHANNELS];
    thsys* _obj;
    char _err_msg[THOR_BUFF_SZ];

    if(para == NULL)
	return;
    _obj = (thsys*) para;

    _buff[0] = 0.0;
    _buff[1] = 0.0;
    ERR_CHECK(NIWriteAnalogArrayF64(_obj->var_a_outask, 1, 0, THSYS_DEF_TIMEOUT, DAQmx_Val_GroupByScanNumber, _buff, &_samples, NULL));

    /* Log messsage to indicate output channels have been reset */
    if(_samples > 0)
	{
	    memset((void*) _err_msg, 0, THOR_BUFF_SZ);
	    sprintf(_err_msg, "Output channels reset %d", (int) _samples);
	    THOR_LOG_ERROR(_err_msg);
	}

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
    int32 _samples;
    int _old_state;
    thsys* _obj;
    int32 _samples_read = 0;
    float64* _buff;
    int _rate = 0, _cnt = 0;

    /* push cleanup handler */
    pthread_cleanup_push(_thsys_thread_cleanup, para);

    /* set thread cancel state to cancellable */

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    _obj = (thsys*) para;
    THOR_LOG_ERROR("thor system started");

    ERR_CHECK(NIStartTask(_obj->var_a_intask));
    ERR_CHECK(NIStartTask(_obj->var_a_outask));

    /*
     * Determin rate at wich needs updating.
     * A counter is used to count time increments and only
     * update on second intervals.
     */
    _rate = (THSYS_USEC_CONV /(_obj->var_sample_rate*THSYS_READ_WRITE_FACTOR));
    _cnt = 0;
    while(1)
    	{
    	    /* test for cancel state */
    	    pthread_testcancel();

    	    /* if callback was set exec */
    	    if(_obj->var_callback_intrupt)
	        _obj->var_callback_intrupt(_obj, (_obj->var_ext_obj? _obj->var_ext_obj : NULL));

	    _samples_read = 0;
	    /* change cancel state to protect read */
	    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &_old_state);
    	    ERR_CHECK(NIReadAnalogF64(_obj->var_a_intask, 1, THSYS_DEF_TIMEOUT, DAQmx_Val_GroupByScanNumber, _obj->var_inbuff, THSYS_NUM_AI_CHANNELS, &_samples_read, NULL));

	    /* if a callback for update is hooked, this shall call the callback function */
	    if(_obj->var_callback_update && !_cnt)
	      _obj->var_callback_update(_obj, _obj->var_ext_obj, _obj->var_inbuff, THSYS_NUM_AI_CHANNELS);

	    /* If write values are available write to the device */
	    _buff = NULL;
	    if(gqueue_count(&_obj->_var_out_queue) > 0)
		{
	    	    pthread_mutex_lock(&_obj->_var_mutex);
		    gqueue_out(&_obj->_var_out_queue, (void**) &_buff);
		    pthread_mutex_unlock(&_obj->_var_mutex);

		    /* Write buffer to the device */
		    if(_buff)
			{
			    _samples = 0;
			    ERR_CHECK(NIWriteAnalogArrayF64(_obj->var_a_outask, 1, 0, THSYS_DEF_TIMEOUT, DAQmx_Val_GroupByScanNumber, _buff, &_samples, NULL));
			    free(_buff);
			}
		}

	    pthread_setcancelstate(_old_state, NULL);

    	    pthread_testcancel();
    	    usleep((int) ((THSYS_USEC_CONV /(_obj->var_sample_rate*THSYS_READ_WRITE_FACTOR))));

	    /*
	     * When update rate has exceeded, results shall sent to
	     * the comm server.
	     */
	    if((_cnt += _rate) > THSYS_UPDATE_RATE)
	      _cnt = 0;

    	}

    pthread_cleanup_pop(1);
    return NULL;
}

/* Delete helper method */
static void _thsys_queue_del_helper(void* data)
{
    /* Check for argument */
    if(data)
	free(data);

    return;
}
