/* Implementation of SOV test
 * Fri Jul 15 09:22:17 GMTDT 2011 */

#include "thsov.h"
#include "thornifix.h"		/* Header file for portability
				 * between linux and win32 */

#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <pthread.h>
#include <math.h>

/* Test object declared static */
static thsov* var_thsov = NULL;


/* Define write channels */
#define THSOV_SOV_ANG_CHANNEL "Dev1/ao0"
#define THSOV_SOV_SUP_CHANNEL "Dev1/ao1"


/* Define read channels */
#define THSOV_TMP_CHANNEL "Dev1/ai0"
#define THSOV_ACT_OPEN_CHANNEL "Dev1/ai1"
#define THSOV_ACT_CLOSE_CHANNEL "Dev1/ai2"
#define THSOV_SOV_ANG_FEEDBACK_CHANNEL "Dev1/ai3"

/* Default wait time 4s */
#define THSOV_DEF_WAIT_TIME 10000

/****************************************/
/* Max and minimum solenoid orientation */
#define THSOV_SOV_ANG_MIN 0.0
#define THSOV_SOV_ANG_MAX 90.0

/* SOV orientation segments; As a minimum solenoid
 * is rotated 4 quadrants */
#define THSOV_SOV_ANG_QUAD 5

/* Max and minimum solenoid orientation voltage */
#define THSOV_SOV_ANG_VOLT_MIN 0.0
#define THSOV_SOV_ANG_VOLT_MAX 10.0
#define THSOV_SOV_MIN_FEEDBACK 2.1
/****************************************/
/* Minimum and maximum supply voltage */
#define THSOV_SOV_SUP_MIN 0.0
#define THSOV_SOV_SUP_MAX 30.0

/* Minimum and maximum supply control voltage */
#define THSOV_SOV_SUP_VOLT_MIN 0.0
#define THSOV_SOV_SUP_VOLT_MAX 5.0
/****************************************/
#define THSOV_SWITCH_VOLT_MIN 0.0
#define THSOV_SWITCH_VOLT_MAX 10.0
#define THSOV_SWITCH_VOLT_CHANGE 9.7
/****************************************/

#define THSOV_NUM_BUFF 4
/****************************************/

#define THSOV_NUM_INPUT_CHANNELS 4

/* Angle conversion macros */
#define THSOV_CONVERT_TO_RAD(val) \
    (M_PI * val / 180)

#define THSOV_CONVERT_TO_DEG(val) \
    (180 * val / M_PI)
/****************************************/

/* number of supply voltage points */
static int THSOV_SUPPLY_PTS = 0;

/* read buffer */
static float64 val_buff[THSOV_NUM_BUFF];

static unsigned int counter = 0;		/* counter */
static unsigned int gcounter = 0;

/* create thread attribute object */
static pthread_attr_t attr;
static pthread_t thread;
static pthread_mutex_t mutex;

static int start_test = 0;

/* Flag is used to watch open and close cycles
 * of the damper */
static int cycle_flag = 0;

/* Flag to indicate not write */
static int act_wait_flag = 0;


/* Private functions */
/* Function declarations for continuous reading */
int32 CVICALLBACK EveryNCallback(TaskHandle taskHandle,
				 int32 everyNsamplesEventType,
				 uInt32 nSamples,
				 void *callbackData);

int32 CVICALLBACK DoneCallback(TaskHandle taskHandle,
			       int32 status,
			       void *callbackData);

static inline int thsov_write_values();
static inline int thsov_write_raw_values();

static char* thsov_wait1 = "Waiting for modulating actuator.";
static char* thsov_wait2 = "Waiting for modulating actuator..";
static char* thsov_wait3 = "Waiting for modulating actuator...";
/*=======================================================================*/

/* Clears tasks */
static inline void thsov_clear_tasks()
{
    ERR_CHECK(NIClearTask(var_thsov->var_outask));
    ERR_CHECK(NIClearTask(var_thsov->var_intask));
}

/* Calculate SOV orientation gradient */
static inline int thsov_calc_angle_grad()
{
    var_thsov->var_sov_st_ang_grad =
	(THSOV_SOV_ANG_VOLT_MAX - THSOV_SOV_ANG_VOLT_MIN) /
	(THSOV_SOV_ANG_MAX - THSOV_SOV_ANG_MIN);

    return 0;
}

/* Calculate SOV supply voltage gradient */
static inline int thsov_calc_sup_grad()
{
    var_thsov->var_sov_sup_volt_grad =
	(THSOV_SOV_SUP_VOLT_MAX - THSOV_SOV_SUP_VOLT_MIN) /
	(THSOV_SOV_SUP_MAX - THSOV_SOV_SUP_MIN);
    
    return 0;
}

/* Convert feedback angle to supply voltage */
static inline int thsov_convert_volt_ang(double val)
{
    if(var_thsov->var_sov_st_ang_grad > 0.0)
	{
	var_thsov->var_sov_ang_fd =
	    val / var_thsov->var_sov_st_ang_grad;
	}
    else
	var_thsov->var_sov_ang_fd = 0.0;

    return 0;
}

/* Covert Power Supply control voltage to
 * actual voltage */
static inline int thsov_convert_volt_sup(double val)
{
    if(var_thsov->var_sov_sup_volt_grad > 0.0)
	{
	    var_thsov->var_sov_sup_fd =
		val / var_thsov->var_sov_sup_volt_grad;
	}
    else
	var_thsov->var_sov_sup_fd = 0.0;

    return 0;
}

/* Create control voltage arrays */
static int thsov_create_angle_volt_array()
{
    int i = 0;
    double inc_ang = 0.0;	  /* increment angle */

    /* call to calculate orientation angle
     * gradient */
    thsov_calc_angle_grad();
    
    if(var_thsov->var_sov_start_angle > 0 &&
       var_thsov->var_sov_start_angle < THSOV_SOV_ANG_MAX)
	{
	    inc_ang = (THSOV_SOV_ANG_MAX -
		       var_thsov->var_sov_start_angle) /
		(THSOV_SOV_ANG_QUAD-1);
	}
    else
	{
	    /* set start angle */
	    var_thsov->var_sov_start_angle =
		THSOV_SOV_ANG_MIN;
	    
	    inc_ang = (THSOV_SOV_ANG_MAX -
		       THSOV_SOV_ANG_MIN) /
		(THSOV_SOV_ANG_QUAD - 1);
	}

    /* create array */
    if(var_thsov->var_arr_init == 0)
	{
	    var_thsov->var_sov_ang_volt =
		(double*) calloc(THSOV_SOV_ANG_QUAD, sizeof(double));
	}

    /* assign values to array */
    for(; i<THSOV_SOV_ANG_QUAD; i++)
	{
	    var_thsov->var_sov_ang_volt[i] =
		var_thsov->var_sov_st_ang_grad *
		(var_thsov->var_sov_start_angle +
		 (double) i * inc_ang);
		
	}
    
    return 0;
}

/* Control voltage for supply */
static int thsov_create_supply_volt_array()
{
    int i = 0;

    /* call to calculate the gradient */
    thsov_calc_sup_grad();
    
    if(var_thsov->var_sov_supply_volt <= 0 ||
       var_thsov->var_sov_supply_volt > THSOV_SOV_SUP_MAX)
	var_thsov->var_sov_supply_volt = THSOV_SOV_SUP_MAX;
    
    THSOV_SUPPLY_PTS = (int) var_thsov->var_sov_supply_volt * 2;

    /* create array */
    if(var_thsov->var_arr_init == 0)
	{
	    var_thsov->var_sov_sup_volt =
		(double*) calloc(THSOV_SUPPLY_PTS, sizeof(double));
	}
    
    for(; i<THSOV_SUPPLY_PTS; i++)
	{
	    var_thsov->var_sov_sup_volt[i] = 0;			/* initialise */

	    var_thsov->var_sov_sup_volt[i] =
		var_thsov->var_sov_sup_volt_grad *
		var_thsov->var_sov_supply_volt *
		cos(((double) i/THSOV_SUPPLY_PTS) * M_PI / 2);
	}

    return 0;
}

/* Assigning values to sensors from buffer */
static inline void thsov_set_values()
{
    /* Lock mutex */
    pthread_mutex_lock(&mutex);
    
    var_thsov->var_tmp_sensor->var_raw =
	(val_buff[0]>0? val_buff[0] : 0.0);
    /* If waiting for modulating actuator do not
       gather data switch data */
    if(act_wait_flag == 0)
	{
	    /* Set switch state */
	    if(Round((double) (val_buff[1]>0? val_buff[1] : 0.0),1) >=
	       var_thsov->var_switching_vlt)
		{
		    var_thsov->var_dmp_state = thsov_dmp_open;
		    cycle_flag = 1;
		    if(var_thsov->var_state_update)
			{
			    var_thsov->var_state_update(var_thsov->var_sobj,
							var_thsov->var_dmp_state);
			}
		}
	    else if(Round((val_buff[2]>0? val_buff[2] : 0.0),1) >=
		    var_thsov->var_switching_vlt)
		{
		    var_thsov->var_dmp_state = thsov_dmp_close;
		    if(var_thsov->var_state_update)
			{
			    var_thsov->var_state_update(var_thsov->var_sobj,
							var_thsov->var_dmp_state);
			}	    
		}
	    else
		var_thsov->var_dmp_state = thsov_dmp_err;
	    
	}
    /* Set angle */
    thsov_convert_volt_ang((val_buff[3]>0? val_buff[3] : 0.0));


    /* update orientation */
    if(var_thsov->var_ang_update)
	{
	    var_thsov->var_ang_update(var_thsov->var_sobj,
				      &var_thsov->var_sov_ang_fd);
	}

    if(var_thsov->var_raw_chnl_flg > 0)
	thsov_write_raw_values();

    /* unlock mutex */
    pthread_mutex_unlock(&mutex);

}

/* Write raw values read from channels to screen */
inline int thsov_write_raw_values()
{
    printf("%f\t%f\t%f\t%f\n",
	   val_buff[0],
	   val_buff[1],
	   val_buff[2],
	   val_buff[3]);

    return 0;
}


/* Write values to console, update UI
 * and call function pointers to update */
inline int thsov_write_values()
{
    /* Check file pointer and update */
    /* Format -
     * Item, Cycle, State, Voltage, Angle, Temperature */
    if(*var_thsov->var_fp)
	{
	    fprintf(*var_thsov->var_fp,
		    "%i,%s,%.2f, %.2f, %.2f\n",
		    var_thsov->var_cyc_cnt,
		    (var_thsov->var_dmp_state == thsov_dmp_open?
		     "Open" : "Close"),
		    var_thsov->var_sov_sup_fd,
		    var_thsov->var_sov_ang_fd,
		    thgsens_get_value(var_thsov->var_tmp_sensor));
	}

    /* Console output */
    printf("%i\t%s\t%.2f\t%.2f\t%.2f\n",
	   counter,
	   (var_thsov->var_dmp_state == thsov_dmp_open?
	    "Open" : "Close"),
	   var_thsov->var_sov_ang_fd,
	   var_thsov->var_sov_sup_fd,
	   thgsens_get_value(var_thsov->var_tmp_sensor));

    /* Call function pointers */
    if(var_thsov->var_cyc_update)
	{
	    var_thsov->var_cyc_update(var_thsov->var_sobj,
				      (void*) &var_thsov->var_cyc_cnt);
	}

    if(var_thsov->var_volt_update)
	{
	    var_thsov->var_volt_update(var_thsov->var_sobj,
				       &var_thsov->var_sov_sup_fd);
	}

    return 0;
				 
}

/* Thread function to initiate start of the test */
static void* thsov_async_start(void* obj)
{
    gcounter = 0;		/* reset counter */
    counter = 0;		/* reset counter */

    int v_counter = 0;
    int a_counter = -1;
    int reset_flg = 0;

    int wait_count = -1;
    
    /* int err_flg = 0; */
    int32 spl_write = 0;
    
    /* flag to indicate test in progress */
    start_test = 1;

    /* start both accuiring and writing tasks */
    if(ERR_CHECK(NIStartTask(var_thsov->var_outask)))
	return NULL;
    if(ERR_CHECK(NIStartTask(var_thsov->var_intask)))
	return NULL;

    /* Output to screen */
    printf("Cycle\tState\tVoltage\tAngle\tTemp\n");
    if(*var_thsov->var_fp)
	{
	    fprintf(*var_thsov->var_fp,
		    "Cycle,State,Voltage,Angle,Temp\n");
	}

    /* Use software timing */
    while(var_thsov->var_stflg)
	{
	    reset_flg = 0;
	    /* Increment counter */
	    if(++a_counter >= THSOV_SOV_ANG_QUAD)
		{
		    printf("%s\n","Actuator reset");
		    reset_flg = 1;
		    a_counter = 0;
		    if(++v_counter >= THSOV_SUPPLY_PTS)
			break;
		}

	    /* Set angle and supply voltage */
	    var_thsov->var_write_array[0] =
		(float64) var_thsov->var_sov_ang_volt[a_counter];
	    var_thsov->var_write_array[1] = 0.0;		/* supply voltage 0 */
	    
	    /* Write to channels */
	    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thsov->var_outask,
					       1,		/* samples per channel */
					       0,		/* auto start */
					       10.0,		/* time out */
					       DAQmx_Val_GroupByScanNumber,
					       (float64*) var_thsov->var_write_array,
					       &spl_write,
					       NULL)))
		break;

	    /* Wait until actuator returns to position */
	    while((val_buff[3] < var_thsov->var_write_array[0]*0.99 ||
		   val_buff[3] > var_thsov->var_write_array[0]*1.01) &&
		  (var_thsov->var_write_array[0] > 0 || reset_flg > 0))
		{
		    if(var_thsov->var_stflg == 0)
			break;

		    /* break from idle loop until actuator is being reset */
		    if(reset_flg >0 && val_buff[3] <= var_thsov->var_min_feedbak_vlt)
			break;

		    act_wait_flag = 1;
		    /* act_wait_flag = 1; */
		    if(++wait_count > 2)
			wait_count = 0;

		    fflush(stdout);
		    switch(wait_count)
			{
			case 0:
			    if(var_thsov->var_indicate_wait)
				var_thsov->var_indicate_wait(var_thsov->var_sobj, (void*) thsov_wait1);
			    printf("%s\r", thsov_wait1);
			    break;
			case 1:
			    if(var_thsov->var_indicate_wait)
				var_thsov->var_indicate_wait(var_thsov->var_sobj, (void*) thsov_wait2);
			    printf("%s\r", thsov_wait2);
			    break;
			default:
			    if(var_thsov->var_indicate_wait)
				var_thsov->var_indicate_wait(var_thsov->var_sobj, (void*) thsov_wait3);
			    printf("%s\r", thsov_wait3);
			}
			    
#if defined(WIN32) || defined(_WIN32)
		    Sleep(500);
#else
		    sleep(1);
#endif
		}
	    act_wait_flag = 0;
	    /* If stopped button pressed exit loop */
	    if(var_thsov->var_stflg == 0)
		break;	    
	    
	    /* Solenoid supply voltage */
	    var_thsov->var_write_array[1] =
		(float64) var_thsov->var_sov_sup_volt[v_counter];
	    
	    counter = 0;
	    while(var_thsov->var_stflg)
		{
		    /* Write to channels */
		    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thsov->var_outask,
						       1,		/* samples per channel */
						       0,		/* auto start */
						       10.0,		/* time out */
						       DAQmx_Val_GroupByScanNumber,
						       (float64*) var_thsov->var_write_array,
						       &spl_write,
						       NULL)))
			break;

		    /* Delay N seconds until sensors record response */
#if defined(WIN32) || defined(_WIN32)
		    Sleep((DWORD) (var_thsov->var_milsec_wait *
				   ((reset_flg>0 && counter==0)? 2 : 1)));
#else
		    sleep((int) var_thsov->var_milsec_wait/1000);
#endif

		    /* Check damper state if loop in first pass */
		    if(counter == 0)
			{
			    thsov_convert_volt_sup(var_thsov->var_write_array[1]);
			    /* Check damper state */
			    if(var_thsov->var_dmp_state != thsov_dmp_open &&
			       var_thsov->var_cont_flg == 0)
				{
				    var_thsov->var_dmp_state = thsov_dmp_err;
				    break;
				}
			    /* Send signal to close damper */
			    var_thsov->var_write_array[1] = 0.0;
			    thsov_write_values();
			}
		    else
			{
			    if(var_thsov->var_dmp_state == thsov_dmp_close &&
			       cycle_flag == 1)
				var_thsov->var_cyc_cnt++;	/* Increment cycle count */
			    else if(var_thsov->var_cont_flg == 0)
				var_thsov->var_dmp_state = thsov_dmp_err;

			    thsov_write_values();
			    break;	/* Exit inner loop */
			}
		    counter++;
		}
	    /* Check damper state and exit if error */
	    if(var_thsov->var_dmp_state == thsov_dmp_err)
		break;
	    
	}

    /* Reset actuator and orientation */
    var_thsov->var_write_array[0] = 0.0;
    var_thsov->var_write_array[1] = 0.0;
    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thsov->var_outask,
				       1,		/* samples per channel */
				       0,		/* auto start */
				       10.0,		/* time out */
				       DAQmx_Val_GroupByScanNumber,
				       (float64*) var_thsov->var_write_array,
				       &spl_write,
				       NULL)));

    /* stop task */
    ERR_CHECK(NIStopTask(var_thsov->var_outask));
    ERR_CHECK(NIStopTask(var_thsov->var_intask));

    /* indicate test end */
    printf("%s\n","Test End");

    /* indicate test stopped */
    start_test = 0;
    return NULL;    
}

 /* Constructor */
int thsov_initialise(gthsen_fptr tmp_update,		/* update temperature */
		     gthsov_dmp_state_fptr dmp_update,	/* damper open and close */
		     gthsen_fptr sov_angle_update,	/* SOV orientation update */
		     gthor_fptr cyc_update,		/* Cycle update */
		     gthsen_fptr volt_update,		/* Voltage update */
		     gthor_fptr wait_update,		/* Wait update */
		     double sov_suppy_start,		/* Supply start voltage */
		     double sov_angle_start,		/* Starting angle */
		     double sov_wait_time,		/* Wait time */
		     thsov** obj,			/* Optional - pointer to local object */
		     FILE** fp,				/* Optional file pointer */
		     void* sobj)			/* pointer to external object */
{
    /* Create object struct */
    var_thsov = (thsov*) malloc(sizeof(thsov));

    if(!var_thsov)
	return 1;

    /* Create out task */
    if(ERR_CHECK(NICreateTask("", &var_thsov->var_outask)))
	return 1;

    /* Create in task */
    if(ERR_CHECK(NICreateTask("", &var_thsov->var_intask)))
	return 1;
    
    /* Create out channel for SOV orientation */
    if(ERR_CHECK(NICreateAOVoltageChan(var_thsov->var_outask,
				       THSOV_SOV_ANG_CHANNEL,
				       "",
				       THSOV_SOV_ANG_VOLT_MIN,
				       THSOV_SOV_ANG_VOLT_MAX,
				       DAQmx_Val_Volts,
				       NULL)))
	return 1;

    /* Create out channel for SOV supply */
    if(ERR_CHECK(NICreateAOVoltageChan(var_thsov->var_outask,
				       THSOV_SOV_SUP_CHANNEL,
				       "",
				       THSOV_SOV_SUP_VOLT_MIN,
				       THSOV_SOV_SUP_VOLT_MAX,
				       DAQmx_Val_Volts,
				       NULL)))
	return 1;


    /* Create temperature sensor */
    if(!thgsens_new(&var_thsov->var_tmp_sensor,
		    THSOV_TMP_CHANNEL,
		    &var_thsov->var_intask,
		    tmp_update,
		    sobj))
	{
	    fprintf(stderr, "%s\n", "unable to create temperature sensor");
	    thsov_clear_tasks();
	    return 1;
	}

    /* Create open and close switch channels */
    /* Open switch channel */
    if(ERR_CHECK(NICreateAIVoltageChan(var_thsov->var_intask,
				       THSOV_ACT_OPEN_CHANNEL,
				       "",
				       DAQmx_Val_NRSE,
				       THSOV_SWITCH_VOLT_MIN,
				       THSOV_SWITCH_VOLT_MAX,
				       DAQmx_Val_Volts,
				       NULL)))
	return 1;

    /* Close switch channel */
    if(ERR_CHECK(NICreateAIVoltageChan(var_thsov->var_intask,
				       THSOV_ACT_CLOSE_CHANNEL,
				       "",
				       DAQmx_Val_NRSE,
				       THSOV_SWITCH_VOLT_MIN,
				       THSOV_SWITCH_VOLT_MAX,
				       DAQmx_Val_Volts,
				       NULL)))
	return 1;

    /* Actuator orientation feedback channel */
    if(ERR_CHECK(NICreateAIVoltageChan(var_thsov->var_intask,
				       THSOV_SOV_ANG_FEEDBACK_CHANNEL,
				       "",
				       DAQmx_Val_Diff,
				       THSOV_SOV_ANG_VOLT_MIN,
				       THSOV_SOV_ANG_VOLT_MAX,
				       DAQmx_Val_Volts,
				       NULL)))
	return 1;

    var_thsov->var_arr_init = 0;
    var_thsov->var_sov_start_angle = sov_angle_start;
    var_thsov->var_sov_supply_volt = sov_suppy_start;

    var_thsov->var_sov_st_ang_grad = 0.0;
    var_thsov->var_sov_sup_volt_grad = 0.0;

    var_thsov->var_dmp_state = thsov_dmp_close;
    var_thsov->var_cyc_cnt = 0;
    var_thsov->var_switching_vlt = THSOV_SWITCH_VOLT_CHANGE;
    var_thsov->var_min_feedbak_vlt = THSOV_SOV_MIN_FEEDBACK;
    var_thsov->var_cont_flg = 0;
    var_thsov->var_raw_chnl_flg = 0;

    /******* Function pointer assign ********/
    var_thsov->var_state_update = dmp_update;
    var_thsov->var_ang_update = sov_angle_update;
    var_thsov->var_cyc_update = cyc_update;
    var_thsov->var_volt_update = volt_update;
    var_thsov->var_indicate_wait = wait_update;
    /****************************************/

    var_thsov->var_sobj = sobj;
    var_thsov->var_fp = fp;
    var_thsov->var_stflg = 0;
    var_thsov->var_sov_sup_fd = 0.0;
    
    if(sov_wait_time <= 0)
	var_thsov->var_milsec_wait = THSOV_DEF_WAIT_TIME;
    else
	var_thsov->var_milsec_wait = sov_wait_time;

    printf("%s %f\n", "sov wait time", var_thsov->var_milsec_wait);
    
    var_thsov->var_thrid = 0;
    
    /* Call to create control voltages */
    thsov_create_angle_volt_array();
    thsov_create_supply_volt_array();

    /* Set external pointer */
    if(obj)
	*obj = var_thsov;

    /* Configure timing */
    if(ERR_CHECK(NICfgSampClkTiming(var_thsov->var_intask,
    				   "OnboardClock",
    				   THSOV_NUM_INPUT_CHANNELS,
    				   DAQmx_Val_Rising,
    				   DAQmx_Val_ContSamps,
    				   1)))
    	{
    	    thsov_clear_tasks();
    	    return 1;
    	}

    /* Register callbacks */
#if defined(WIN32) || defined(_WIN32)    
    if(ERR_CHECK(NIRegisterEveryNSamplesEvent(var_thsov->var_intask,
    					      DAQmx_Val_Acquired_Into_Buffer,
    					      1,
    					      0,
    					      EveryNCallback,
    					      NULL)))
    	{
    	    thsov_clear_tasks();
    	    return 1;
    	}
    
    if(ERR_CHECK(NIRegisterDoneEvent(var_thsov->var_intask,
    				     0,
    				     DoneCallback,
    				     NULL)))
    	{
    	    thsov_clear_tasks();
    	    return 1;
    	}
#endif
    
    /* Initialis mutex */
    pthread_mutex_init(&mutex, NULL);
    printf("%s\n","Initialisation complete");
    
    return 0;
}

/* Destructor */
void thsov_delete(thsov** obj)
{
    printf("%s\n","deleting sensors..");
    if(obj)
	*obj = NULL;
    
    /* Delete sensor */
    thgsens_delete(&var_thsov->var_tmp_sensor);

    printf("%s\n","clean up");

    /* set function pointers to NULL */
    var_thsov->var_state_update = NULL;
    var_thsov->var_ang_update = NULL;

    /* set file pointer to NULL */
    var_thsov->var_fp = NULL;

    /* set external object to NULL */
    var_thsov->var_sobj = NULL;

    /* delete control arrays */
    if(var_thsov->var_arr_init)
	{
	    free(var_thsov->var_sov_ang_volt);
	    free(var_thsov->var_sov_sup_volt);
	    
	    var_thsov->var_sov_ang_volt = NULL;
	    var_thsov->var_sov_sup_volt = NULL;
	}
    
    printf("%s\n","clear NIDAQmx tasks");

    /* Clear tasks */
    thsov_clear_tasks();

    /* Destroy mutex */
    pthread_mutex_destroy(&mutex);

    printf("%s\n","delete thor..");

    /* free object */
    free(var_thsov);
    var_thsov = NULL;
    
}

/* Set starting angle and voltage */
inline int thsov_set_voltage_and_angle(thsov* obj,	/* Pointer to object */
				       double st_volt,	/* Starting voltage */
				       double st_ang)	/* Starting angle */
{
    var_thsov->var_sov_start_angle = st_ang;
    var_thsov->var_sov_supply_volt = st_volt;

    thsov_create_angle_volt_array();
    thsov_create_supply_volt_array();
    return 0;
}

/* Set wait time */
inline int thsov_set_wait_time(thsov* obj,
			       double wait_time)
{
	var_thsov->var_milsec_wait = (float) wait_time;
	return 0;
}

/* Reset sensors */
inline int thsov_reset_values(thsov* obj)
{
    return 0;
}

/* Start test asynchronously */
int thsov_start(thsov* obj)
{
    /* check temperature sensors */
    if(!var_thsov->var_tmp_sensor->var_okflg)
	{
	    fprintf(stderr, "%s\n", "temperature sensor range not set");
	    return 1;
	}

    var_thsov->var_stflg = 1;

    /* initialise thread attributes */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    var_thsov->var_thrid =
	pthread_create(&thread, &attr, thsov_async_start, NULL);

    return 0;
    
}

/* Stop test */
int thsov_stop(thsov* obj)
{
    pthread_mutex_lock(&mutex);
    var_thsov->var_stflg = 0;
    pthread_mutex_unlock(&mutex);

    printf("%s\n","waiting for NI Tasks to finish..");
    if(start_test)
	pthread_join(thread, NULL);

    start_test = 0;

    /* Test stopped */
    printf("%s\n","test stopped..");

    pthread_attr_destroy(&attr);
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
    if(ERR_CHECK(NIReadAnalogF64(var_thsov->var_intask,
				 1,
				 10.0,
				 DAQmx_Val_GroupByScanNumber,
				 val_buff,
				 THSOV_NUM_INPUT_CHANNELS,
				 &spl_read,
				 NULL)))
	return 1;

    /* Call to set values */
    thsov_set_values();
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

/* set minimum feedback voltage */
inline int thsov_set_minimum_feedback_voltage(thsov* obj,
					      double val)
{
    if(!obj)
	return 1;

    obj->var_min_feedbak_vlt = val;
    return 0;
}

/* set continue full cycle flag */
inline int thsov_set_cont_flg(thsov* obj,
			      unsigned int val)
{
    if(!obj)
	return 1;

    obj->var_cont_flg = val;
    return 0;
}

inline int thsov_set_raw_channel_output_flg(thsov* obj,
					    unsigned int val)
{
    if(!obj)
	return 1;

    obj->var_raw_chnl_flg = val;
    return 0;
}
