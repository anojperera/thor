/* Implementation of leakage program
 * Sun May 15 16:20:25 BST 2011 */

#include "thlkg.h"
#include "thornifix.h"
#if !defined(WIN32) || !defined(_WIN32)
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#endif

#include <math.h>
#include "thlinreg.h"
#include "thpid.h"

static thlkg* var_thlkg = NULL;			/* leakage test object */

#define THLKG_TMP_CHANNEL "Dev1/ai0"		/* temperature channel */
/* For the differential readings shall be taken from two sensors
 * and the difference is calculated. HP shall be 1 and LP shall be 2. */
#define THLKG_DP1_CHANNEL "Dev1/ai1"		/* differential pressure channel 1 */
#define THLKG_DP2_CHANNEL "Dev1/ai2"		/* differential pressure channel 2 */
#define THLKG_ST_CHANNEL "Dev1/ai3"	       	/* static pressure channel */
#define THLKG_FANFEEDBACK_CHANNEL "Dev1/ai4"	/* Fan control feedback */
#define THLKG_FANST_CHANNEL "Dev1/ao0"		/* Fan start stop control */
#define THLKG_FAN_CHANNEL "Dev1/ao1"		/* fan control channel */
#define THLKG_BUFF_SZ 2048
#define THLKG_RATE 1000.0			/* rate of fan control */
#define THLKG_NUM_CHANNELS 5			/* number of channels */
#define THLKG_SAMPLES_PERSECOND 4		/* samples per second per
						   channel */
#define THLKG_STATIC_PRESSURE_CHECK 4.5		/* static pressure check */

#define THLKG_START_RELAY1_VOLTAGE 4.0		/* relay starting voltage */
#define THLKG_STATIC_PRESSURE_ADJ 30.0		/* static pressure adjustment */
#define THLKG_UPDATE_RATE 3			/* update rate */
static char err_msg[THLKG_BUFF_SZ];

/* Counters */
static unsigned int s_counter = 0;		/* sample counter */
static unsigned int counter = 0;		/* counter */
static unsigned int gcounter = 0;
static unsigned int pcounter = 0;		/* counter for PID */

/* Flag to indicate max counter has been reached. This flag is used
 * in moving average calculations. In the firs round of data collection
 * actual sample number shall be passed to the averaging function. When
 * the maximum is reached, the counter is reset. Since we want to
 * calculate moving average, maximum sample size is passed to the averaging
 * function */
static unsigned int max_flg = 0;

/* buffer to hold input values */
static float64 val_buff[THLKG_NUM_CHANNELS];

#if defined(WIN32) || defined(_WIN32)
/* create thread attribute object */
static HANDLE thread;
static HANDLE mutex;
static HANDLE var_sem;
#else
static pthread_attr_t attr;
static pthread_t thread;
static pthread_mutex_t mutex;
/* semaphore to control calibration */
static sem_t var_sem;
#endif

static int start_test = 0;

/* PID Control Objects */
static struct thxy* var_thxy = NULL;
static theq* var_theq = NULL;
static struct thpid var_thpid;

/* private functions */
/* Function declarations for continuous reading */
static int32 CVICALLBACK EveryNCallback(TaskHandle taskHandle,
					int32 everyNsamplesEventType,
					uInt32 nSamples,
					void *callbackData);

static int32 CVICALLBACK DoneCallback(TaskHandle taskHandle,
				      int32 status,
				      void *callbackData);

/* Initialise calibration */
static inline void thlkg_linreg()
{
    /* create new object */
    if(var_thlkg->var_calflg == 0 &&
       (var_thxy == NULL || var_theq == NULL))
	{
	    printf("%s\n","initialised equation");
	    if(thlinreg_new(&var_theq, &var_thxy))
		return;
	}
    else
	{
	    /* calculate the gradient and intercept */
	    thlinreg_calc_equation(var_theq,
				   &var_thpid.var_m,
				   &var_thpid.var_c,
				   NULL);
	    
	    /* set pid equation type */
	    thpid_set_equation_type(&var_thpid,
				    var_theq->var_eqtype);
	}

}

/* Add fanfeedback and control point value */
static inline void thlkg_add_xy_to_list()
{
    if(!var_thxy || !var_theq)
	return;

    var_thxy->y = val_buff[4];		/* Fan Speed
					 * feedback */
    
    if(var_thlkg->var_stoptype == thlkg_lkg)
	var_thxy->x = var_thlkg->var_leakage;
    else
	var_thxy->x = thgsens_get_value(var_thlkg->var_stsensor);

    var_thxy->x2 = var_thxy->x * var_thxy->x;
    var_thxy->xy = var_thxy->x * var_thxy->y;

    /* Add to list */
    aList_Add(&var_theq->var_list, (void*) var_thxy, sizeof(struct thxy));
    
}

/* clears tasks */
static inline void thlkg_clear_tasks()
{
    ERR_CHECK(NIClearTask(var_thlkg->var_outask));
    ERR_CHECK(NIClearTask(var_thlkg->var_intask));
}

/* create an array of output signals */
static inline void thlkg_fanout_signals()
{
    if(var_thlkg->var_fanout)
	free(var_thlkg->var_fanout);
    
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
    var_thlkg->var_fansignal[0] = 0.0;
    var_thlkg->var_fansignal[1] = 0.0;
    
    thgsens_reset_all(var_thlkg->var_dpsensor1);
    thgsens_reset_all(var_thlkg->var_dpsensor2);
    thgsens_reset_all(var_thlkg->var_stsensor);
    thgsens_reset_all(var_thlkg->var_tmpsensor);

    var_thlkg->var_calflg = 0;
    var_thlkg->var_pidflg = 0;

    return 1;
}

/* function assigning values to sensors from buffer */
static inline void thlkg_set_values()
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
    /* set value of dp */
    thgsens_add_value(var_thlkg->var_tmpsensor,
		      (val_buff[0]>0? val_buff[0] : 0.0));
    thgsens_add_value(var_thlkg->var_dpsensor1,
		      (val_buff[1]>0? val_buff[1] : 0.0));
    thgsens_add_value(var_thlkg->var_dpsensor2,
		      (val_buff[1]>0? val_buff[2] : 0.0));
    thgsens_add_value(var_thlkg->var_stsensor,
		      (val_buff[2]>0? val_buff[3] : 0.0));

    /* workout leakage here and
     * call to update external value if
     * function pointer has set */
    var_thlkg->var_dp = thgsens_get_value2(var_thlkg->var_dpsensor1) -
	thgsens_get_value2(var_thlkg->var_dpsensor2);
    var_thlkg->var_st = thgsens_get_value2(var_thlkg->var_stsensor);
    switch(var_thlkg->var_dia_orf)
	{
	case thlkg_ordia1:
	    var_thlkg->var_leakage =
		0.248456 * pow(var_thlkg->var_dp, 0.49954);
	    break;
	case thlkg_ordia2:
	    var_thlkg->var_leakage =
		0.559366 * pow(var_thlkg->var_dp, 0.499551);
	    break;
	case thlkg_ordia3:
	    var_thlkg->var_leakage =
		0.99558 * pow(var_thlkg->var_dp, 0.49668);
	    break;
	case thlkg_ordia4:
	    var_thlkg->var_leakage =
		2.29643 * pow(var_thlkg->var_dp, 0.49539);
	    break;
	case thlkg_ordia5:
	    var_thlkg->var_leakage =
		4.26362 * pow(var_thlkg->var_dp, 0.49446);
	    break;
	default:
	    var_thlkg->var_leakage =
		7.22534 * pow(var_thlkg->var_dp, 0.49325);
	    break;
	}

    /* Moving average calculations */
    var_thlkg->var_lkg_arr[s_counter] = var_thlkg->var_leakage;
    var_thlkg->var_dp_arr[s_counter] = var_thlkg->var_dp;
    var_thlkg->var_st_arr[s_counter] = var_thlkg->var_st;
    /* call averaging functions */
    var_thlkg->var_leakage = Mean(var_thlkg->var_lkg_arr,
				  (max_flg? THLKG_SAMPLES_PERSECOND * THLKG_UPDATE_RATE :
				   s_counter));
    var_thlkg->var_dp = Mean(var_thlkg->var_dp_arr,
				  (max_flg? THLKG_SAMPLES_PERSECOND * THLKG_UPDATE_RATE :
				   s_counter));
    var_thlkg->var_st = Mean(var_thlkg->var_st_arr,
				  (max_flg? THLKG_SAMPLES_PERSECOND * THLKG_UPDATE_RATE :
				   s_counter));
    
    /* calibrate */
    if (var_thlkg->var_calflg == 0 &&
	(var_thlkg->var_stoptype == thlkg_lkg ||
	 var_thlkg->var_stoptype == thlkg_pr))
	{
	    thlkg_add_xy_to_list();

	    /* If idl flag is true,
	       don't add calibration points */
	    if(var_thlkg->var_idlflg)
		{
		    var_thlkg->var_calflg = 1;
		    thlkg_linreg();
#if defined(WIN32) || defined(_WIN32)
		    ReleaseSemaphore(var_sem, 1, NULL);
#else
		    sem_post(&var_sem);
#endif
		}
	}

    /* unlock mutex */
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(mutex);
#else
    pthread_mutex_unlock(&mutex);
#endif
    
    /* call function pointer to update external ui if assigned */
    if(s_counter == 0)
	{
	    if(var_thlkg->var_lkgupdate)
		var_thlkg->var_lkgupdate(var_thlkg->sobj_ptr, &var_thlkg->var_leakage);
	    if(var_thlkg->var_dpupdate)
		var_thlkg->var_dpupdate(var_thlkg->sobj_ptr, &var_thlkg->var_dp);
	    if(var_thlkg->var_stupdate)
		var_thlkg->var_stupdate(var_thlkg->sobj_ptr, &var_thlkg->var_st);
	}
    
}

/* Write to file.
 * File must be open and must be able to write */
static inline void thlkg_write_results()
{
    double _dp = 0.0;
    _dp = thgsens_get_value(var_thlkg->var_dpsensor1) -
	thgsens_get_value(var_thlkg->var_dpsensor2);
    
    /* check if file pointer was assigned */
    if(var_thlkg->var_fp)
	{
	    /* format */
	    /* dp, st, lkg, tmp */
	    fprintf(var_thlkg->var_fp, "%f,%f,%f,%f\n",
		    _dp,
		    thgsens_get_value(var_thlkg->var_stsensor),
		    var_thlkg->var_leakage,
		    thgsens_get_value(var_thlkg->var_tmpsensor));
	}

    /* update screen */
    fflush(stdout);
    printf("%i\t%f\t%f\t%f\t%f\t%f\r",
	   gcounter,
	   var_thlkg->var_fansignal[1],
	   _dp,
	   thgsens_get_value(var_thlkg->var_stsensor),
	   var_thlkg->var_leakage,
	   thgsens_get_value(var_thlkg->var_tmpsensor));

    /* fulsh output */
    fflush(stdout);
}

/* thread function to initiate start of the test */
#if defined (WIN32) || defined (_WIN32)
DWORD WINAPI thlkg_async_start(LPVOID obj)
#else    
static void* thlkg_async_start(void* obj)
#endif    
{
    counter = 0;			/* reset counter */
    gcounter = 0;			/* reset counter */
    pcounter = 0;			/* reset counter */
    
    /* flag to indicate if test started */
    start_test = 1;

    /* clear PID likned list */
    aList_Clear(&var_theq->var_list);
    var_theq->var_list = NULL;

    /* create fan out signal */
    thlkg_fanout_signals();
    
    /* output to screen */
    printf("Counter\tFan\t\tDP\t\tST\t\tLkg\t\tTmp\n");
    
    /* start both acquiring and writing tasks */
    if(ERR_CHECK(NIStartTask(var_thlkg->var_outask)))
#if defined (WIN32) || defined (_WIN32)
	return FALSE;
#else
	return NULL;
#endif
    if(ERR_CHECK(NIStartTask(var_thlkg->var_intask)))
#if defined (WIN32) || defined (_WIN32)
	return FALSE;
#else
	return NULL;
#endif

    /* wait until static pressure reaches below default */
    while(1)
	{
	    if(thgsens_get_value(var_thlkg->var_stsensor) <
	       THLKG_STATIC_PRESSURE_CHECK)
		break;
	}
    
    int32 spl_write = 0;		/* number of samples written */

    var_thlkg->var_fansignal[0] = 0.0;
    var_thlkg->var_fansignal[1] = 0.0;
    
    /* use software timing for now */
    while(var_thlkg->var_stflg)
	{
	    /* Delay for 500ms */
#if defined(WIN32) || defined(_WIN32)
	    if(var_thlkg->var_idlflg > 0 || var_thlkg->var_pidflg > 0)
		Sleep(1500);
	    else
		Sleep(200);
#else
	    sleep(1);
#endif

	    if(gcounter == 1)
		var_thlkg->var_fansignal[0] = THLKG_START_RELAY1_VOLTAGE;

	    if(var_thlkg->var_stoptype == thlkg_lkg ||
	       var_thlkg->var_stoptype == thlkg_pr ||
	       var_thlkg->var_pidflg > 0)
		{
		    var_thlkg->var_fansignal[1] =
			var_thlkg->var_fanout[counter];

		    /* reset counter if exceed limit */
		    if(var_thlkg->var_idlflg == 0 ||
		       var_thlkg->var_pidflg > 0)
			{
			    if(++counter >= (var_thlkg->var_pidflg>0?
					     pcounter : THLKG_RATE))
				{
				    counter = 0;
				    var_thlkg->var_pidflg = 0;
				    if(var_thlkg->var_disb_fptr)
					var_thlkg->var_disb_fptr(var_thlkg->sobj_ptr, 1);
				}
			}
		    else if(var_thlkg->var_idlflg &&
			    var_thlkg->var_calflg &&
			    var_thlkg->var_pidflg == 0)
			{
			    /* set fan output based on PID control */
			    if(var_thlkg->var_stoptype == thlkg_lkg)
				{
				    thpid_pid_control2(&var_thpid,
						       var_thlkg->var_stopval,
						       var_thlkg->var_leakage,
						       var_thlkg->var_fansignal[1],
						       &var_thlkg->var_fanout,
						       &pcounter);
				    var_thlkg->var_pidflg = 1;
				    counter = 0;
				}
			    else if(var_thlkg->var_stoptype == thlkg_pr)
				{
				    thpid_pid_control2(&var_thpid,
						       var_thlkg->var_stopval /* - var_thlkg->var_stop_static_adj */,
						       thgsens_get_value(var_thlkg->var_stsensor),
						       var_thlkg->var_fansignal[1],
						       &var_thlkg->var_fanout,
						       &pcounter);
				    var_thlkg->var_pidflg = 1;
				    counter = 0;
				}
			}
		}

	    
	    /* write to out channel */
	    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thlkg->var_outask,
					       1,
					       0,
					       10.0,
					       DAQmx_Val_GroupByScanNumber,
					       (float64*) var_thlkg->var_fansignal,
					       &spl_write,
					       NULL)))
		break;

	    
	    /* check control mode - if static pressure based or dp based.
	     * if the value exceed stop value set to idle and continue test
	     * until stopped */
	    switch(var_thlkg->var_stoptype)
		{
		case thlkg_lkg:
		    if(var_thlkg->var_leakage >= var_thlkg->var_stopval)
			{
			    if(var_thlkg->var_idlflg == 0)
				{
				    var_thlkg->var_idlflg = 1;
#if defined(WIN32) || defined(_WIN32)
				    WaitForSingleObject(var_sem, INFINITE);
#else
				    sem_wait(&var_sem);
#endif
				}
			    var_thlkg->var_idlflg = 1;
			}
		    break;
		case thlkg_pr:
		    if(thgsens_get_value(var_thlkg->var_stsensor) >=
		       (var_thlkg->var_stopval - var_thlkg->var_stop_static_adj))
			{
			    if(var_thlkg->var_idlflg == 0)
				{
				    var_thlkg->var_idlflg = 1;
#if defined(WIN32) || defined(_WIN32)
				    WaitForSingleObject(var_sem, INFINITE);
#else
				    sem_wait(&var_sem);
#endif
				}
			    var_thlkg->var_idlflg = 1;
			}
		    break;
		default:
		    break;
		}

	    /* call to write results to file and
	     * screen */
	    thlkg_write_results();
	    gcounter++;
	}

    /* Stop fan */
    var_thlkg->var_fansignal[0] = 0.0;
    var_thlkg->var_fansignal[1] = 0.0;
    
    /* stop fan */
    ERR_CHECK(NIWriteAnalogArrayF64(var_thlkg->var_outask,
				    1,
				    0,
				    10.0,
				    DAQmx_Val_GroupByScanNumber,
				    var_thlkg->var_fansignal,
				    &spl_write,
				    NULL));

    printf("\n");
    
    /* stop task */
    ERR_CHECK(NIStopTask(var_thlkg->var_outask));
    ERR_CHECK(NIStopTask(var_thlkg->var_intask));

    /* indicate test stopped */
    start_test = 0;
#if defined (WIN32) || defined (_WIN32)
    return TRUE;
#else
    return NULL;
#endif
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
    int i;
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

    /* Create Fan Start Channel */
    if(ERR_CHECK(NICreateAOVoltageChan(var_thlkg->var_outask,
				       THLKG_FANST_CHANNEL,
				       "",
				       0.0,
				       10.0,
				       DAQmx_Val_Volts,
				       NULL)))
	{
	    thlkg_clear_tasks();
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
    
    /* create temperature sensor */
    if(!thgsens_new(&var_thlkg->var_tmpsensor,
		    THLKG_TMP_CHANNEL,
		    &var_thlkg->var_intask,
		    NULL,
		    NULL))
	{
	    fprintf(stderr, "%s\n", "unable to create temperature sensor");
	    thlkg_clear_tasks();

	    return 0;
	}
    /* create differential pressure switch 1 */
    if(!thgsens_new(&var_thlkg->var_dpsensor1,
		    THLKG_DP1_CHANNEL,
		    &var_thlkg->var_intask,
		    NULL,
		    NULL))
	{
	    fprintf(stderr, "%s\n", "unable to create dp sensor");
	    thlkg_clear_tasks();

	    return 0;
	}

    /* create differential pressure switch 2 */
    if(!thgsens_new(&var_thlkg->var_dpsensor2,
		    THLKG_DP2_CHANNEL,
		    &var_thlkg->var_intask,
		    NULL,
		    NULL))
	{
	    fprintf(stderr, "%s\n", "unable to create dp sensor");
	    thlkg_clear_tasks();

	    return 0;
	}

    /* create static sensor */
    if(!thgsens_new(&var_thlkg->var_stsensor,
		    THLKG_ST_CHANNEL,
		    &var_thlkg->var_intask,
		    NULL,
		    NULL))
	{
	    fprintf(stderr, "%s\n", "unable to create st sensor");
	    thlkg_clear_tasks();

	    thgsens_delete(&var_thlkg->var_dpsensor1);
	    thgsens_delete(&var_thlkg->var_dpsensor2);	    
	    return 0;
	}

    /* create channel to monitor fan feedback */
    if(ERR_CHECK(NICreateAIVoltageChan(var_thlkg->var_intask,
				       THLKG_FANFEEDBACK_CHANNEL,
				       "",
				       DAQmx_Val_Diff,
				       0.0,
				       10.0,
				       DAQmx_Val_Volts,
				       NULL)))
	{
	    thlkg_clear_tasks();
	    return 0;
	}

    var_thlkg->var_lkgupdate = update_lkg;
    var_thlkg->var_stupdate = update_st;
    var_thlkg->var_dpupdate = update_dp;
    var_thlkg->var_stopval = 0.0;
    var_thlkg->var_stop_static_adj = THLKG_STATIC_PRESSURE_ADJ;
    var_thlkg->var_stoptype = ctrl_st;
    var_thlkg->var_dia_orf = dia;
    var_thlkg->var_fp = fp;
    var_thlkg->var_smplerate = THLKG_RATE;
    var_thlkg->var_fansignal[0] = 0.0;
    var_thlkg->var_fansignal[1] = 0.0;
    var_thlkg->var_stflg = 1;
    var_thlkg->var_leakage = 0.0;
    var_thlkg->var_dp = 0.0;
    var_thlkg->var_st = 0.0;
    var_thlkg->var_idlflg = 0;
    var_thlkg->sobj_ptr = sobj;
    var_thlkg->var_calflg = 0;
    var_thlkg->var_pidflg = 0;
    var_thlkg->var_disb_fptr = NULL;
    var_thlkg->var_fanout = NULL;
    var_thlkg->var_lkg_arr = (double*) 
	calloc((THLKG_SAMPLES_PERSECOND * THLKG_UPDATE_RATE), sizeof(double));
    var_thlkg->var_dp_arr = (double*) 
	calloc((THLKG_SAMPLES_PERSECOND * THLKG_UPDATE_RATE), sizeof(double));
    var_thlkg->var_st_arr = (double*)
	calloc((THLKG_SAMPLES_PERSECOND * THLKG_UPDATE_RATE), sizeof(double));
    for(i=0; i<(THLKG_SAMPLES_PERSECOND * THLKG_UPDATE_RATE); i++)
	{
	    var_thlkg->var_lkg_arr[i] = 0.0;
	    var_thlkg->var_dp_arr[i] = 0.0;
	    var_thlkg->var_st_arr[i] = 0.0;
	}
    /* call to create signal array for fan control */
    thlkg_fanout_signals();

    /* initialise structs for linear regression */
    thlkg_linreg();

    /* initialise PID control struct */
    thpid_init(&var_thpid);
    var_thpid.var_raw_flg = 1;

    if(obj)
	*obj = var_thlkg;

    /* configure timing */
    if(ERR_CHECK(NICfgSampClkTiming(var_thlkg->var_intask,
				    "OnboardClock",
				    THLKG_SAMPLES_PERSECOND,
				    DAQmx_Val_Rising,
				    DAQmx_Val_ContSamps,
				    1)))
	{
	    thlkg_clear_tasks();
	    return 0;
	}

    /* Register callbacks */
#if defined(WIN32) || defined(_WIN32)
    if(ERR_CHECK(NIRegisterEveryNSamplesEvent(var_thlkg->var_intask,
					      DAQmx_Val_Acquired_Into_Buffer,
					      1,
					      0,
					      EveryNCallback,
					      NULL)))
	{
	    thlkg_clear_tasks();
	    return 0;
	}

    if(ERR_CHECK(NIRegisterDoneEvent(var_thlkg->var_intask,
				     0,
				     DoneCallback,
				     NULL)))
	{
	    thlkg_clear_tasks();
	    return 0;
	}
#endif
    


#if defined (WIN32) || defined (_WIN32)
    /* initialise mutex */
    mutex = CreateMutex(NULL,				/* default security */
			FALSE,				/* initially not owned */
			NULL);				/* no name */

    var_sem = CreateSemaphore(NULL,			/* default security */
			      0,			/* initial sem count */
			      10,			/* maximum sem count */
			      NULL);
    
#else
    /* initialise mutex */
    pthread_mutex_init(&mutex, NULL);

    /* initialise semaphore */
    sem_init(&var_sem, 0, 0);
#endif
    
    return 1;
}

/* destructor */
extern void thlkg_delete()
{
    printf("%s\n","deleting sensors..");
    /* destroy sensors */
    thgsens_delete(&var_thlkg->var_dpsensor1);
    thgsens_delete(&var_thlkg->var_dpsensor2);
    thgsens_delete(&var_thlkg->var_stsensor);
    thgsens_delete(&var_thlkg->var_tmpsensor);
    
    printf("%s\n","clean up");
    /* set function pointer to NULL */
    var_thlkg->var_lkgupdate = NULL;
    var_thlkg->var_stupdate = NULL;
    var_thlkg->var_dpupdate = NULL;

    free(var_thlkg->var_lkg_arr);
    free(var_thlkg->var_dp_arr);
    free(var_thlkg->var_st_arr);
    var_thlkg->var_lkg_arr = NULL;
    var_thlkg->var_dp_arr = NULL;
    var_thlkg->var_st_arr = NULL;
    
    /* set file pointer to NULL, to
     * be closed by the calling function */
    var_thlkg->var_fp = NULL;

    /* set sobj to NULL */
    var_thlkg->sobj_ptr = NULL;

    /* delete fan signal array */
    free(var_thlkg->var_fanout);
    var_thlkg->var_fanout = NULL;

    printf("%s\n","clear NIDAQmx tasks");
    /* clear tasks */
    thlkg_clear_tasks();

#if defined (WIN32) || defined (_WIN32)
    CloseHandle(mutex);				/* destroy mutex */
    CloseHandle(var_sem);			/* destroy semaphore */
#else
    /* Destroy mutex */
    pthread_mutex_destroy(&mutex);
    
    /* Destroy semaphore */
    sem_destroy(&var_sem);
#endif
    
    printf("%s\n","delete thor..");

    thlinreg_delete(&var_theq);
    free(var_thxy);
    thpid_delete(&var_thpid);

    
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
    if(!var_thlkg->var_dpsensor1->var_okflg)
	{
	    fprintf(stderr, "%s\n", "dp sensor 1 range not set");
	    return 0;
	}

    if(!var_thlkg->var_dpsensor2->var_okflg)
	{
	    fprintf(stderr, "%s\n", "dp sensor 2 range not set");
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
#if defined (WIN32) || defined (_WIN32)
    thread = CreateThread(NULL,			/* default security attribute */
    		     0,				/* use default stack size */
    		     thlkg_async_start,		/* thread function */
    		     NULL,			/* argument to thread function */
    		     0,				/* default creation flag */
    		     &var_thlkg->var_thrid);	/* thread id */
#else
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    var_thlkg->var_thrid =
	pthread_create(&thread, &attr, thlkg_async_start, NULL);
#endif

    return 1;
    
}


/* stop the test */
int thlkg_stop(thlkg* obj)
{
    /* set stop flag and join thread */

    
    /* lock mutex */
#if defined (WIN32) || defined (_WIN32)
    ReleaseSemaphore(var_sem, 1, NULL);
    WaitForSingleObject(mutex, INFINITE);
#else
    sem_post(&var_sem);
    pthread_mutex_lock(&mutex);
#endif
    
    var_thlkg->var_stflg = 0;
    if(var_thlkg->var_disb_fptr)
	var_thlkg->var_disb_fptr(var_thlkg->sobj_ptr, 1);
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(mutex);
#else    
    pthread_mutex_unlock(&mutex);
#endif

    /* wait until thread finishes if test in progress */
#if defined (WIN32) || defined (_WIN32)
    if(start_test)
	{
	    printf("%s\n", "Waiting for clear..");
	    WaitForSingleObject(thread, 5000);
	    CloseHandle(thread);
	}
#else
    if(start_test)
	pthread_join(thread, NULL);
#endif
    /* function stop notify */
    printf("%s\n","test stopped");    

    /* stopped complete */
    printf("%s\n","complete..");

#if !defined (WIN32) || !defined (_WIN32)
    pthread_attr_destroy(&attr);
#endif    
    return 1;
}

/* set fan control */
int thlkg_set_fanctrl_volt(double percen)
{
    unsigned int i = 0;
    if(percen < 0 || percen > 100 || !var_thlkg)
	return 0;

    /* lock mutex */
    printf("\n%s\n", "setting voltage");
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(mutex, INFINITE);
#else
    pthread_mutex_lock(&mutex);
#endif

    if(var_thlkg->var_pidflg > 0)
	{
#if defined (WIN32) || defined (_WIN32)
	    ReleaseMutex(mutex);
#else
	    pthread_mutex_unlock(&mutex);
#endif
	    return 0;
	}

    if(((9.95 * percen / 100) -
	var_thlkg->var_fansignal[1]) < 0 &&
       fabs((9.95 * percen / 100) -
	    var_thlkg->var_fansignal[1]) > 0.1)
	{
	    pcounter = 10 * (unsigned int)
		ceil(fabs((9.95 * percen / 100) -
			  var_thlkg->var_fansignal[1]));

	    if(var_thlkg->var_fanout)
		free(var_thlkg->var_fanout);

	    var_thlkg->var_fanout =
		(double*) calloc(pcounter, sizeof(double));
	    
	    if(!var_thlkg->var_fanout)
		{
#if defined (WIN32) || defined (_WIN32)
		    WaitForSingleObject(mutex, INFINITE);
#else
		    pthread_mutex_lock(&mutex);
#endif
		    return 0;
		}

	    for(i = 0; i < pcounter; i++)
		{
		    var_thlkg->var_fanout[i] =
			var_thlkg->var_fansignal[1] +
			((9.95 * percen / 100) -
			 var_thlkg->var_fansignal[1]) * sin(M_PI * i / (2 * pcounter));
		    /* printf("fan control value %i %f\n",i, var_thlkg->var_fanout[i]); */
		}
	    counter = 0;
	    var_thlkg->var_pidflg = 1;
	    if(var_thlkg->var_disb_fptr && start_test)
		var_thlkg->var_disb_fptr(var_thlkg->sobj_ptr, 0);
	}
    else
	var_thlkg->var_fansignal[1] = 9.95 * percen / 100;

#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(mutex);
#else
    pthread_mutex_unlock(&mutex);
#endif
    /* if(var_thlkg->var_disb_fptr) */
    /* 	var_thlkg->var_disb_fptr(var_thlkg->sobj_ptr, 0); */
    return 1;
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
    if(ERR_CHECK(NIReadAnalogF64(var_thlkg->var_intask,
				 1,
				 10.0,
				 DAQmx_Val_GroupByScanNumber,
				 val_buff,
				 THLKG_NUM_CHANNELS,
				 &spl_read,
				 NULL)))
	return 1;

    
    /* Call to set values */
    thlkg_set_values();
    if(s_counter++ >= (THLKG_SAMPLES_PERSECOND * THLKG_UPDATE_RATE))
	{
	    s_counter = 0;
	    max_flg = 1;
	}
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
