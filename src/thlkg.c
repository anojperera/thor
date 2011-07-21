/* Implementation of leakage program
 * Sun May 15 16:20:25 BST 2011 */

#include "thlkg.h"
#include "thornifix.h"
#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <pthread.h>

static thlkg* var_thlkg = NULL;			/* leakage test object */

#define THLKG_DP_CHANNEL "Dev1/ai1"		/* differential pressure channel */
#define THLKG_ST_CHANNEL "Dev1/ai2"	       	/* static pressure channel */
#define THLKG_TMP_CHANNEL "Dev1/ai3"		/* temperature channel */
#define THLKG_FAN_CHANNEL "Dev1/ao0"		/* fan control channel */
#define THLKG_BUFF_SZ 2048
#define THLKG_RATE 1000.0			/* rate of fan control */
#define THLKG_NUM_CHANNELS 3			/* number of channels */


static char err_msg[THLKG_BUFF_SZ];
static unsigned int counter = 0;		/* counter */
static unsigned int gcounter = 0;

/* buffer to hold input values */
static float64 val_buff[THLKG_NUM_CHANNELS];

/* create thread attribute object */
static pthread_attr_t attr;
static pthread_t thread;
static pthread_mutex_t mutex;

static int start_test = 0;

/* private functions */

/* clears tasks */
static inline void thlkg_clear_tasks()
{
    ERR_CHECK(NIClearTask(var_thlkg->var_outask));
    ERR_CHECK(NIClearTask(var_thlkg->var_intask));
}

/* create an array of output signals */
static inline void thlkg_fanout_signals()
{
    var_thlkg->var_fanout = (double*)
	calloc(THLKG_RATE, sizeof(double));

    int i = 0;
    for(; i< THLKG_RATE; i++)
	{
	    var_thlkg->var_fanout[i] =
		9.98 * sin((double) i * M_PI / (2 * THLKG_RATE));
	}
    
}

/* reset sensor values */
int thlkg_reset_sensors(thlkg* obj)    
{
    var_thlkg->var_fansignal = 0.0;
    thgsens_reset_all(var_thlkg->var_dpsensor);
    thgsens_reset_all(var_thlkg->var_stsensor);
    thgsens_reset_all(var_thlkg->var_tmpsensor);

    return 1;
}

/* function assigning values to sensors from buffer */
static inline void thlkg_set_values()
{
    /* set value of dp */
    var_thlkg->var_dpsensor->var_raw =
	(val_buff[0]>0? val_buff[0] : 0.0);
    var_thlkg->var_stsensor->var_raw =
	(val_buff[1]>0? val_buff[1] : 0.0);
    var_thlkg->var_tmpsensor->var_raw =
	(val_buff[2]>0? val_buff[2] : 0.0);

    /* workout leakage here and
     * call to update external value if
     * function pointer has set */
    switch(var_thlkg->var_dia_orf)
	{
	case thlkg_ordia1:
	    var_thlkg->var_leakage =
		0.248456 * pow(thgsens_get_value(var_thlkg->var_dpsensor), 0.49954);
	    break;
	case thlkg_ordia2:
	    var_thlkg->var_leakage =
		0.559366 * pow(thgsens_get_value(var_thlkg->var_dpsensor), 0.499551);
	    break;
	case thlkg_ordia3:
	    var_thlkg->var_leakage =
		0.99558 * pow(thgsens_get_value(var_thlkg->var_dpsensor), 0.49668);
	    break;
	case thlkg_ordia4:
	    var_thlkg->var_leakage =
		2.29643 * pow(thgsens_get_value(var_thlkg->var_dpsensor), 0.49539);
	    break;
	case thlkg_ordia5:
	    var_thlkg->var_leakage =
		4.26362 * pow(thgsens_get_value(var_thlkg->var_dpsensor), 0.49446);
	    break;
	default:
	    var_thlkg->var_leakage =
		7.22534 * pow(thgsens_get_value(var_thlkg->var_dpsensor), 0.49325);
	    break;
	}

    /* call function pointer to update external ui if assigned */
    if(var_thlkg->var_lkgupdate)
	var_thlkg->var_lkgupdate(var_thlkg->sobj_ptr, &var_thlkg->var_leakage);
    
}

/* Write to file.
 * File must be open and must be able to write */
static inline void thlkg_write_results()
{
    /* check if file pointer was assigned */
    if(var_thlkg->var_fp)
	{
	    /* format */
	    /* dp, st, lkg, tmp */
	    fprintf(var_thlkg->var_fp, "%f,%f,%f,%f\n",
		    thgsens_get_value(var_thlkg->var_dpsensor),
		    thgsens_get_value(var_thlkg->var_stsensor),
		    var_thlkg->var_leakage,
		    thgsens_get_value(var_thlkg->var_tmpsensor));
	}

    /* update screen */
    fflush(stdout);
    printf("%i\t%f\t%f\t%f\t%f\t%f\r",
	   gcounter,
	   var_thlkg->var_fansignal,
	   thgsens_get_value(var_thlkg->var_dpsensor),
	   thgsens_get_value(var_thlkg->var_stsensor),
	   var_thlkg->var_leakage,
	   thgsens_get_value(var_thlkg->var_tmpsensor));

    /* fulsh output */
    fflush(stdout);
}

/* thread function to initiate start of the test */
static void* thlkg_async_start(void* obj)
{
    counter = 0;			/* reset counter */
    gcounter = 0;			/* reset counter */
    float64 st_fan_val = 0.0;

    
    /* flag to indicate if test started */
    start_test = 1;

    /* start both acquiring and writing tasks */
    if(ERR_CHECK(NIStartTask(var_thlkg->var_outask)))
	return NULL;
    if(ERR_CHECK(NIStartTask(var_thlkg->var_intask)))
	return NULL;

    int32 spl_read = 0;			/* number of samples read */
#if !defined(WIN32) || !defined(_WIN32)    
    int32 spl_write = 0;		/* number of samples written */
#endif
    
    /* output to screen */
    printf("Counter\tFan\t\tDP\t\tST\t\tLkg\t\tTmp\n");
    
    /* use software timing for now */
    while(var_thlkg->var_stflg)
	{
	    /* Delay for 500ms */
#if defined(WIN32) || defined(_WIN32)
	    Sleep(500);
#else
	    sleep(1);
#endif

	    if(var_thlkg->var_stoptype == thlkg_lkg ||
	       var_thlkg->var_stoptype == thlkg_pr)
		{
		    var_thlkg->var_fansignal =
			var_thlkg->var_fanout[counter];

		    /* reset counter if exceed limit */
		    if(var_thlkg->var_idlflg == 0)
			{
			    if(++counter >= THLKG_RATE)
				counter = 0;
			}
		}

	    
	    /* write to out channel */
#if defined (WIN32) || defined (_WIN32)	    
	    if(ERR_CHECK(NIWriteAnalogF64(var_thlkg->var_outask,
						0,
						10.0,
						(float64) var_thlkg->var_fansignal,
						NULL)))
		break;
#else
	    if(ERR_CHECK(NIWriteAnalogF64(var_thlkg->var_outask,
						 1,
						 0,
						 10.0,
						 DAQmx_Val_GroupByChannel,
						 (float64*) &var_thlkg->var_fansignal,
						 &spl_write,
						 NULL)))
		break;
#endif

	    
	    /* read settings */
	    if(ERR_CHECK(NIReadAnalogF64(var_thlkg->var_intask,
					 1,
					 10.0,
					 DAQmx_Val_GroupByScanNumber,
					 val_buff,
					 THLKG_NUM_CHANNELS,
					 &spl_read,
					 NULL)))
		break;

	    /* call function to assign values to sensors */
	    thlkg_set_values();

	    /* check control mode - if static pressure based or dp based.
	     * if the value exceed stop value set to idle and continue test
	     * until stopped */
	    switch(var_thlkg->var_stoptype)
		{
		case thlkg_lkg:
		    if(var_thlkg->var_leakage >= var_thlkg->var_stopval)
			var_thlkg->var_idlflg = 1;
		    break;
		case thlkg_pr:
		    if(thgsens_get_value(var_thlkg->var_stsensor) >=
		       var_thlkg->var_stopval)
			var_thlkg->var_idlflg = 1;
		    break;
		default:
		    break;
		}

	    /* call to write results to file and
	     * screen */
	    thlkg_write_results();
	    gcounter++;
	}
    
    /* stop fan */
#if defined (WIN32) || defined (_WIN32)
    ERR_CHECK(NIWriteAnalogF64(var_thlkg->var_outask,
			       0,
			       10.0,
			       st_fan_val,
			       NULL));
#else
    ERR_CHECK(NIWriteAnalogF64(var_thlkg->var_outask,
			       1,
			       0,
			       10.0,
			       DAQmx_Val_GroupByChannel,
			       &st_fan_val,
			       &spl_write,
			       NULL));
#endif
    printf("\n");
    
    /* stop task */
    ERR_CHECK(NIStopTask(var_thlkg->var_outask));
    ERR_CHECK(NIStopTask(var_thlkg->var_intask));

    /* indicate test stopped */
    start_test = 0;
    return NULL;
}

/**************************************************************************/

/* constructor */
int thlkg_initialise(thlkg_stopctrl ctrl_st,		/* start control */
		     thlkg_orifice_plate dia,		/* orifice plate dia */
		     gthsen_fptr update_lkg,		/* leakage update */
		     gthsen_fptr update_st,		/* static upate */
		     gthsen_fptr update_dp,		/* dp update */
		     FILE* fp,				/* file pointer */
		     thlkg** obj,
		     void* sobj)			/* object to pass to callback
							 * function */
{
    int32 err_code;
    
    var_thlkg = (thlkg*) malloc(sizeof(thlkg));
    
    if(!var_thlkg)
	return 0;

    /* create out task */
    err_code = NICreateTask("", &var_thlkg->var_outask);
    if(err_code)
	{
	    /* get error message */
#if defined (WIN32) || defined (_WIN32)
	    NIGetErrorString(err_code, err_msg, THLKG_BUFF_SZ);
#else
	    NIGetErrorString(err_msg, THLKG_BUFF_SZ);
#endif
	    fprintf(stderr, "%s\n", err_msg);

	    return 0;
	}

    /* create in task */
    err_code = NICreateTask("", &var_thlkg->var_intask);
    if(err_code)
	{
	    /* get error message */
#if defined (WIN32) || defined (_WIN32)      
	    NIGetErrorString(err_code, err_msg, THLKG_BUFF_SZ);
#else
	    NIGetErrorString(err_msg, THLKG_BUFF_SZ);
#endif
      
	    fprintf(stderr, "%s\n", err_msg);

	    return 0;
	}

    /* create out channel for fan control */
    err_code = NICreateAOVoltageChan(var_thlkg->var_outask,
				     THLKG_FAN_CHANNEL,
				     "",
				     0.0,
				     10.0,
				     DAQmx_Val_Volts,
				     NULL);
    if(err_code)
	{
	    /* get error message */
#if defined (WIN32) || defined (_WIN32)      
	    NIGetErrorString(err_code, err_msg, THLKG_BUFF_SZ);
#else
	    NIGetErrorString(err_msg, THLKG_BUFF_SZ);
#endif
	    fprintf(stderr, "%s\n", err_msg);

	    thlkg_clear_tasks();
	    return 0;
	}    

    /* create differential pressure switch */
    if(!thgsens_new(&var_thlkg->var_dpsensor,
		    THLKG_DP_CHANNEL,
		    &var_thlkg->var_intask,
		    update_dp,
		    sobj))
	{
	    fprintf(stderr, "%s\n", "unable to create dp sensor");
	    thlkg_clear_tasks();

	    return 0;
	}

    /* create static sensor */
    if(!thgsens_new(&var_thlkg->var_stsensor,
		    THLKG_ST_CHANNEL,
		    &var_thlkg->var_intask,
		    update_st,
		    sobj))
	{
	    fprintf(stderr, "%s\n", "unable to create st sensor");
	    thlkg_clear_tasks();

	    thgsens_delete(&var_thlkg->var_dpsensor);
	    return 0;
	}

    /* create temperature sensor */
    if(!thgsens_new(&var_thlkg->var_tmpsensor,
		    THLKG_TMP_CHANNEL,
		    &var_thlkg->var_intask,
		    NULL,
		    NULL))
	{
	    fprintf(stderr, "%s\n", "unable to create temperature sensor");
	    thlkg_clear_tasks();

	    thgsens_delete(&var_thlkg->var_dpsensor);
	    thgsens_delete(&var_thlkg->var_stsensor);

	    return 0;
	}

    var_thlkg->var_lkgupdate = update_lkg;

    var_thlkg->var_stopval = 0.0;
    var_thlkg->var_stoptype = ctrl_st;
    var_thlkg->var_dia_orf = dia;
    var_thlkg->var_fp = fp;
    var_thlkg->var_smplerate = THLKG_RATE;
    var_thlkg->var_fansignal = 0.0;
    var_thlkg->var_stflg = 1;
    var_thlkg->var_leakage = 0.0;
    var_thlkg->var_idlflg = 0;
    var_thlkg->sobj_ptr = sobj;

    /* call to create signal array for fan control */
    thlkg_fanout_signals();

    if(obj)
	*obj = var_thlkg;

    /* initialise mutex */
    pthread_mutex_init(&mutex, NULL);
    
    return 1;
}

/* destructor */
extern void thlkg_delete()
{
    printf("%s\n","deleting sensors..");
    /* destroy sensors */
    thgsens_delete(&var_thlkg->var_dpsensor);
    thgsens_delete(&var_thlkg->var_stsensor);
    thgsens_delete(&var_thlkg->var_tmpsensor);
    
    printf("%s\n","clean up");
    /* set function pointer to NULL */
    var_thlkg->var_lkgupdate = NULL;

    /* set file pointer to NULL, to
     * be closed by the calling function */
    var_thlkg->var_fp = NULL;

    /* delete fan signal array */
    free(var_thlkg->var_fanout);
    var_thlkg->var_fanout = NULL;

    printf("%s\n","clear NIDAQmx tasks");
    /* clear tasks */
    thlkg_clear_tasks();

    pthread_mutex_destroy(&mutex);

    printf("%s\n","delete thor..");
    /* free object */
    free(var_thlkg);
    var_thlkg = NULL;

}

/* Call to start function.
 * Creates a detached thread and execute data gathering.
 * Also performs if all reuquired data has been set */
int thlkg_start(thlkg* obj)
{
    /* check if sensor range is assigned */
    if(!var_thlkg->var_dpsensor->var_okflg)
	{
	    fprintf(stderr, "%s\n", "dp sensor range not set");
	    return 0;
	}

    if(!var_thlkg->var_stsensor->var_okflg)
	{
	    fprintf(stderr, "%s\n", "static sensor range not set");
	    return 0;
	}

    if(!var_thlkg->var_tmpsensor->var_okflg)
	{
	    fprintf(stderr, "%s\n", "temperature sensor range not set");
	    return 0;
	}

    /* reset flags */
    var_thlkg->var_stflg = 1;		/* start flag to on */
    var_thlkg->var_idlflg = 0;		/* idle flag to off */

    /* initialise attribute */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    var_thlkg->var_thrid =
	pthread_create(&thread, &attr, thlkg_async_start, NULL);

    return 1;
    
}


/* stop the test */
int thlkg_stop(thlkg* obj)
{
    /* set stop flag and join thread */

    /* lock mutex */
    pthread_mutex_lock(&mutex);
    var_thlkg->var_stflg = 0;
    pthread_mutex_unlock(&mutex);

    /* wait until thread finishes if test in progress */
    if(start_test)
	pthread_join(thread, NULL);

    /* function stop notify */
    printf("%s\n","test stopped");    

    /* stopped complete */
    printf("%s\n","complete..");

    pthread_attr_destroy(&attr);
    
    return 1;
}

/* set fan control */
int thlkg_set_fanctrl_volt(double percen)
{
    if(percen < 0 || percen > 100 || !var_thlkg)
	return 0;
    
    /* lock mutex */
    printf("\n%s\n", "setting voltage");
    pthread_mutex_lock(&mutex);
    var_thlkg->var_fansignal = 9.95 * percen / 100;
    pthread_mutex_unlock(&mutex);    

    return 1;
}
