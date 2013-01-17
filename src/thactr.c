/* Implementation of actuator reliability test */

#include "thactr.h"

#if !defined(WIN32) || !defined(_WIN32)
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#endif

#include "thornifix.h"

static thactr* var_thactr = NULL;		/* object pointer */

#define THACTR_RELAY_CHANNEL "Dev1/ao0"

#define THACTR_TMP_CHANNEL "Dev1/ai0"
#define THACTR_OPEN_CHANNEL "Dev1/ai1"
#define THACTR_CLOSE_CHANNEL "Dev1/ai2"

#define THACTR_OPEN_IX 1
#define THACTR_CLOSE_IX 2

/* Default wait time */
#define THACTR_DEF_IDLE_TIME 3000

/* Actuator switching voltage */
#define THACTR_SWITCH_VOLT_CHANGE 9.6

#define THACTR_START_RELAY1_VOLTAGE 4.0
#define THACTR_STOP_RELAY1_VOLTAGE 0.0

/* Number of channels */
#define THACTR_NUM_CHANNELS 3

/* Minimum and maximum voltage */
#define THACTR_MIN_VOLTAGE 0.0
#define THACTR_MAX_VOLTAGE 10.0

static float64 val_buff[THACTR_NUM_CHANNELS];

static unsigned int counter = 0;		/* counter */
static unsigned int gcounter = 0;

/* create thread attribute object */
#if defined (WIN32) || defined (_WIN32)
static HANDLE thread;
static HANDLE mutex;
#else
static pthread_attr_t attr;
static pthread_t thread;
static pthread_mutex_t mutex;
#endif
static int start_test = 0;

/* Private functions */
/* Function declarations for continuous reading */
static int32 CVICALLBACK EveryNCallback(TaskHandle taskHandle,
				 int32 everyNsamplesEventType,
				 uInt32 nSamples,
				 void *callbackData);

static int32 CVICALLBACK DoneCallback(TaskHandle taskHandle,
			       int32 status,
			       void *callbackData);

/* Clears tasks */
static inline void thactr_clear_tasks()
{
    ERR_CHECK(NIClearTask(var_thactr->var_outask));
    ERR_CHECK(NIClearTask(var_thactr->var_intask));
}

/* Set values */
static inline void thactr_set_values()
{
    /* Lock mutex */
#if defined (WIN32) || defined (_WIN32)
    DWORD mutex_state;
    mutex_state = WaitForSingleObject(mutex, INFINITE);
    if(mutex_state != WAIT_OBJECT_0)
	return;
#else
    pthread_mutex_lock(&mutex);
#endif
    
    var_thactr->var_tmp_sensor->var_raw =
	(val_buff[0]>0? val_buff[0] : 0.0);
    
    if(val_buff[THACTR_OPEN_IX] >= var_thactr->var_sw_volt)
	var_thactr->var_opcl_flg = 1;
    else if(val_buff[THACTR_CLOSE_IX] >= var_thactr->var_sw_volt)
	var_thactr->var_opcl_flg = 0;

    printf("%i\t%f\t%f\n",gcounter,val_buff[THACTR_OPEN_IX], val_buff[THACTR_CLOSE_IX]);
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(mutex);
#else
    pthread_mutex_unlock(&mutex);
#endif
}

/* Write value */
static inline void thactr_write_values()
{
    if(var_thactr->var_fp)
	{
	    fprintf(*var_thactr->var_fp, "%i,%s,%f\n",
		    gcounter,
		    (var_thactr->var_opcl_flg? "OPEN" : "CLOSE"),
		    thgsens_get_value2(var_thactr->var_tmp_sensor));
	}

    printf("%i\t%s\t%f\n",
	   gcounter,
	   (var_thactr->var_opcl_flg? "OPEN" : "CLOSE"),
	   thgsens_get_value2(var_thactr->var_tmp_sensor));
}

#if defined (WIN32) || defined (_WIN32)
DWORD WINAPI thactr_async_start(LPVOID obj)
#else
static void* thactr_async_start(void* obj)
#endif
{
    gcounter = 0;		/* reset counter */
    counter = 0;		/* reset counter */
    float64 out_buff[1];	/* out buffer */
    int32 spl_write = 0;
    int err_flg = 0;
    int cyc_flg = 0;		/* cycle flag */

    /* flag to indicate test in progress */
    start_test = 1;

    /* start both accuiring and writing tasks */
    if(ERR_CHECK(NIStartTask(var_thactr->var_outask)))
#if defined (WIN32) || defined (_WIN32)
	return FALSE;
#else
	return NULL;
#endif
    if(ERR_CHECK(NIStartTask(var_thactr->var_intask)))
#if defined (WIN32) || defined (_WIN32)
	return FALSE;
#else
	return NULL;
#endif

    /* Output to screen */
    if(var_thactr->var_fp)
	fprintf(*var_thactr->var_fp, "Cycle,State,Temp\n");

    printf("Cycle\tState\tTemp\n");
    
    out_buff[0] = THACTR_START_RELAY1_VOLTAGE;

    cyc_flg = 0;	/* set cycle flag */
    while(var_thactr->var_stflg)
	{
	    /* send signal to open ralay */
	    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thactr->var_outask,
					       1,		/* samples per channel */
					       0,		/* auto start */
					       10.0,		/* time out */
					       DAQmx_Val_GroupByScanNumber,
					       out_buff,
					       &spl_write,
					       NULL)))
		break;
	    while(1)
		{
		    /* Delay */
#if defined(WIN32) || defined(_WIN32)
		    Sleep(var_thactr->var_idle_time);
#else
		    sleep(1);
#endif
		    /* Check if damper is open */
		    if(cyc_flg == 0 && var_thactr->var_opcl_flg > 0)
			{
			    cyc_flg = 1;
			    if(var_thactr->var_cyc_fnc)
				var_thactr->var_cyc_fnc(var_thactr->sobj_ptr, (void*) &var_thactr->var_cyc_cnt);

			    thactr_write_values();

			    out_buff[0] = THACTR_STOP_RELAY1_VOLTAGE;

			    break;

			}
		    else if(cyc_flg == 1 && var_thactr->var_opcl_flg == 0)
			{
			    /* increment counter */
			    var_thactr->var_cyc_cnt++;
			    if(var_thactr->var_cyc_fnc)
				var_thactr->var_cyc_fnc(var_thactr->sobj_ptr, (void*) &var_thactr->var_cyc_cnt);

			    thactr_write_values();

			    out_buff[0] = THACTR_START_RELAY1_VOLTAGE;
			    gcounter++;		/* increment generic counter */
			    cyc_flg = 0;
			    break;
			}
		    else
			{
			    err_flg = 1;
			    printf("%s\n","Limit switch voltage error");
			    break;
			}
			
		}

	    /* If Error flag is set */
	    if(err_flg)
		break;
	}

    /* Reset solenoid */
    out_buff[0] = 0.0;
    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thactr->var_outask,
				       1,		/* samples per channel */
				       0,		/* auto start */
				       10.0,		/* time out */
				       DAQmx_Val_GroupByScanNumber,
				       out_buff,
				       &spl_write,
				       NULL)));

        /* stop task */
    ERR_CHECK(NIStopTask(var_thactr->var_outask));
    ERR_CHECK(NIStopTask(var_thactr->var_intask));

    /* Indicate test end */
    printf("%s\n","Test End");

    start_test = 0;
#if defined (WIN32) || defined (_WIN32)
    return TRUE;
#else
    return NULL;
#endif
}

/* Constructor */
int thactr_initialise(gthor_fptr update_cycle,	/* Function pointer to update cycle */
		      gthsen_fptr update_temp,	/* Function pointer to update temperature */
		      FILE** fp,		/* FILE pointer */
		      thactr** obj,		/* pointer to internal object */
		      void* sobj)		/* object to pass to callback */
{
    /* Create internal object */
    var_thactr = (thactr*) malloc(sizeof(thactr));

    if(!var_thactr)
	{
	    printf("thactr_initialise %s\n", "unable to create thactr object");
	    return 1;
	}

    /* Create out task */
    if(ERR_CHECK(NICreateTask("", &var_thactr->var_outask)))
	return 1;

    /* Create in task */
    if(ERR_CHECK(NICreateTask("", &var_thactr->var_intask)))
	return 1;

    /* Create output channel for relay */
    if(ERR_CHECK(NICreateAOVoltageChan(var_thactr->var_outask,
				       THACTR_RELAY_CHANNEL,
				       "",
				       THACTR_MIN_VOLTAGE,
				       THACTR_MAX_VOLTAGE,
				       DAQmx_Val_Volts,
				       NULL)))
	return 1;

    /* Create temperature sensor */
    if(!thgsens_new(&var_thactr->var_tmp_sensor,
		    THACTR_TMP_CHANNEL,
		    &var_thactr->var_intask,
		    update_temp,
		    sobj))
	{
	    fprintf(stderr, "%s\n", "unable to create temperature sensor");
	    thactr_clear_tasks();
	    return 1;
	}    

    /* Create open and close switch channels */
    /* Open switch channel */
    if(ERR_CHECK(NICreateAIVoltageChan(var_thactr->var_intask,
				       THACTR_OPEN_CHANNEL,
				       "",
				       DAQmx_Val_NRSE,
				       THACTR_MIN_VOLTAGE,
				       THACTR_MAX_VOLTAGE,
				       DAQmx_Val_Volts,
				       NULL)))
	{
	    thactr_clear_tasks();
	    return 1;
	}

    /* Close switch channel */
    if(ERR_CHECK(NICreateAIVoltageChan(var_thactr->var_intask,
				       THACTR_CLOSE_CHANNEL,
				       "",
				       DAQmx_Val_NRSE,
				       THACTR_MIN_VOLTAGE,
				       THACTR_MAX_VOLTAGE,
				       DAQmx_Val_Volts,
				       NULL)))
	{
	    thactr_clear_tasks();
	    return 1;
	}

    var_thactr->var_cyc_cnt = 0;
    var_thactr->var_idle_time = THACTR_DEF_IDLE_TIME;

    var_thactr->var_sw_volt = THACTR_SWITCH_VOLT_CHANGE;
    var_thactr->var_relay_ix = 0;
    var_thactr->var_opcl_flg = 0;		/* set to close */

    var_thactr->var_cyc_fnc = update_cycle;
    var_thactr->var_tmp_fnc = update_temp;
    var_thactr->var_fp = fp;
    var_thactr->sobj_ptr = sobj;

    var_thactr->var_stflg = 0;			/* stop flag */

    /* Configure timing */
    if(ERR_CHECK(NICfgSampClkTiming(var_thactr->var_intask,
				    "OnboardClock",
				    THACTR_NUM_CHANNELS,
				    DAQmx_Val_Rising,
				    DAQmx_Val_ContSamps,
				    1)))
    	{
    	    thactr_clear_tasks();
    	    return 1;
    	}    

    /* Register callbacks */
#if defined(WIN32) || defined(_WIN32)    
    if(ERR_CHECK(NIRegisterEveryNSamplesEvent(var_thactr->var_intask,
    					      DAQmx_Val_Acquired_Into_Buffer,
    					      1,
    					      0,
    					      EveryNCallback,
    					      NULL)))
    	{
    	    thactr_clear_tasks();
    	    return 1;
    	}
    
    if(ERR_CHECK(NIRegisterDoneEvent(var_thactr->var_intask,
    				     0,
    				     DoneCallback,
    				     NULL)))
    	{
    	    thactr_clear_tasks();
    	    return 1;
    	}

    /* Initialise mutex */
    mutex = CreateMutex(NULL,				/* default security */
			FALSE,				/* initially not owned */
			NULL);				/* no name */
#else
    /* initialis mutex */
    pthread_mutex_init(&mutex, NULL);
#endif

    if(obj)
	*obj = var_thactr;

    printf("%s\n","Initialisation complete");

    return 0;
}

/* Delete */
void thactr_delete(thactr** obj)
{
    printf("%s\n","delete sensors...");
    if(obj)
	*obj = NULL;

    if(var_thactr == NULL)
	return;

    /* Delete temperature sensor */
    thgsens_delete(&var_thactr->var_tmp_sensor);

    printf("%s\n","clean up");

    var_thactr->var_cyc_fnc = NULL;
    var_thactr->var_tmp_fnc = NULL;

    /* Set file pointer to NULL */
    var_thactr->var_fp = NULL;

    printf("%s\n","clear NIDAQmx tasks");

    /* Clear tasks */
    thactr_clear_tasks();
#if defined (WIN32) || defined (_WIN32)
    CloseHandle(mutex);				/* destroy mutex */
#else
    pthread_mutex_destroy(&mutex);
#endif

    free(var_thactr);
    var_thactr = NULL;
}

/* Start test asynchronously */
int thactr_start(thactr* obj)
{
    /* check temperature sensor */
    if(!var_thactr->var_tmp_sensor->var_okflg)
	{
	    printf("%s\n","temperature sensor range not set");
	    return 1;
	}

    var_thactr->var_stflg = 1;

    /* initialise thread attributes */
#if defined (WIN32) || defined (_WIN32)
    thread = CreateThread(NULL,			/* default security attribute */
    		     0,				/* use default stack size */
    		     thactr_async_start,	/* thread function */
    		     NULL,			/* argument to thread function */
    		     0,				/* default creation flag */
    		     &var_thactr->var_thrid);	/* thread id */
#else
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    var_thactr->var_thrid =
	pthread_create(&thread, &attr, thactr_async_start, NULL);
#endif
    return 0;
}

/* Stop test */
int thactr_stop(thactr* obj)
{
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(mutex, INFINITE);
#else
    pthread_mutex_lock(&mutex);
#endif
    var_thactr->var_stflg = 0;
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(mutex);
#else
    pthread_mutex_unlock(&mutex);
#endif

    printf("%s\n","waiting for NI Tasks to finish..");
#if defined (WIN32) || defined (_WIN32)
    if(start_test)
	{
	    WaitForSingleObject(thread, INFINITE);
	    CloseHandle(thread);
	}
#else
    if(start_test)
	pthread_join(thread, NULL);
    pthread_attr_destroy(&attr);
#endif

    start_test = 0;

    /* Test stopped */
    printf("%s\n","test_stopped..");
    return 0;
}

/* Reset values */
inline int thactr_reset_all(thactr* obj)
{
    var_thactr->var_cyc_cnt = 0;
    var_thactr->var_relay_ix = 0;
    var_thactr->var_opcl_flg = 0;

    var_thactr->var_stflg = 0;

    thgsens_reset_all(var_thactr->var_tmp_sensor);
    return 0;
}

/*=======================================================*/
/* Implementation of Callback functions */
int32 CVICALLBACK EveryNCallback(TaskHandle taskHandle,
				 int32 everyNsamplesEventType,
				 uInt32 nSamples,
				 void *callbackData)
{
    int32 spl_read = 0;		/* samples read */
    
    /* Read data into buffer */
    if(ERR_CHECK(NIReadAnalogF64(var_thactr->var_intask,
				 1,
				 10.0,
				 DAQmx_Val_GroupByScanNumber,
				 val_buff,
				 THACTR_NUM_CHANNELS,
				 &spl_read,
				 NULL)))
	return 1;

    /* Call to set values */
    thactr_set_values();
    return 0;
    
}

/* DoneCallback */
int32 CVICALLBACK DoneCallback(TaskHandle taskHandle,
			       int32 status,
			       void *callbackData)
{
    /* Check if data accquisition was stopped
     * due to error */
    if(ERR_CHECK(status))
	return 0;

    return 0;
}

