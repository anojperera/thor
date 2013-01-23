/* Implementation of ahu test object */

#include "thahup.h"
#if !defined(WIN32) || !defined(_WIN32)
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#endif
#include <math.h>

#include "thlinreg.h"
#include "thpid.h"

/* object */
static thahup* var_thahup = NULL;			/* ahu test object */

/* Channel Indexes */
#define THAHUP_ACTFD_IX 0
#define THAHUP_TMP_IX 1
#define THAHUP_ST_IX 2
#define THAHUP_DP1_IX 3
#define THAHUP_DP2_IX 4
#define THAHUP_DP3_IX 5
#define THAHUP_DP4_IX 6

#define THAHUP_ACT_CTRL_CHANNEL "Dev1/ao0"
#define THAHUP_ACT_FD_CHANNEL "Dev1/ai0"		/* actuator feedback channel */

/* channel names for sensors */
#define THAHUP_TMP_CHANNEL "Dev1/ai1"			/* temperature sensor */

#define THAHUP_ST_CHANNEL "Dev1/ai2"			/* static pressure sensor - ino 171 */
#define THAHUP_VEL_DP1_CHANNEL "Dev1/ai3"		/* differential pressure velocity - ino 167 */
#define THAHUP_VEL_DP2_CHANNEL "Dev1/ai4"		/* differential pressure velocity - ino 168 */
#define THAHUP_VEL_DP3_CHANNEL "Dev1/ai5"		/* differential pressure velocity - ino 169 */
#define THAHUP_VEL_DP4_CHANNEL "Dev1/ai6"		/* differential pressure velocity - ino 170 */


#define THAHUP_MIN_RNG 0.0				/* default minimum range for velocity sensors */
#define THAHUP_MAX_RNG 1600.0				/* default maximum range for velocity sensors */

/*******************************************/
#define THAHUP_BUFF_SZ 2048				/* message buffer */
#define THAHUP_RATE 1000.0				/* actuator rate control */

#define THAHUP_NUM_INPUT_CHANNELS 7			/* number of channels */
#define THAHUP_SAMPLES_PERSECOND 8			/* samples read second */
#define THAHUP_UPDATE_RATE 3
#define THAHUP_MIN_FEEDBACK_VOLT 2.1			/* minimum feedback voltage */

#define THAHUP_CHECK_VSENSOR(obj) \
    obj? (obj->var_flg? obj->var_val : 0.0) : 0.0

/* static char err_msg[THAHUP_BUFF_SZ]; */
static unsigned int s_counter = 0;
static unsigned int counter = 0;			/* counter */
static unsigned int gcounter = 0;
static unsigned int max_flg = 0;
/* buffer to hold input values */
static float64 val_buff[THAHUP_NUM_INPUT_CHANNELS];

/* thread and thread attributes objects */
#if defined (WIN32) || defined (_WIN32)
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

/* start test flag */
static int start_test = 0;



static struct thxy* var_thxy = NULL;
static theq* var_theq = NULL;
static struct thpid var_thpid;

/* Function declarations for continuous reading */
static int32 CVICALLBACK EveryNCallback(TaskHandle taskHandle,
				 int32 everyNsamplesEventType,
				 uInt32 nSamples,
				 void *callbackData);

static int32 CVICALLBACK DoneCallback(TaskHandle taskHandle,
			       int32 status,
			       void *callbackData);

/* initialise calibration */
static inline void thahup_linreg()
{
    /* Create new object */
    if(var_thahup->var_calflg == 0 &&
       (var_thxy == NULL || var_theq == NULL))
	{
	    if(thlinreg_new(&var_theq, &var_thxy))
		return;
	}
    else
	{
	    /* calculate gradient and intercept */
	    thlinreg_calc_equation(var_theq,
				   &var_thpid.var_m,
				   &var_thpid.var_c,
				   NULL);

	    /* set pid equation type */
	    thpid_set_equation_type(&var_thpid,
				    var_theq->var_eqtype);	    
	}
}

/* add static pressure and feedback voltage to list for
 * calculating the gradient and intercept of the line */
static inline void thahup_add_xy_to_list()
{
    if(!var_thxy || !var_theq)
	return;

    var_thxy->x = var_thahup->var_static_val;
    var_thxy->y = var_thahup->var_act_fd_val;
    var_thxy->x2 = var_thxy->x * var_thxy->x;
    var_thxy->xy = var_thxy->x * var_thxy->y;

    aList_Add(&var_theq->var_list, (void*) var_thxy, sizeof(struct thxy));
}

/* clear tasts */
static inline void thahup_clear_tasks()
{
    ERR_CHECK(NIClearTask(var_thahup->var_outask));
    ERR_CHECK(NIClearTask(var_thahup->var_intask));    
}

/* create an array of output signals */
static inline void thahup_actout_signals()
{
    /* alloc memory for actuator control array */
    var_thahup->var_actout = (double*)
	calloc(THAHUP_RATE, sizeof(double));

    int i = 0;
    for(; i<THAHUP_RATE; i++)
	{
	    var_thahup->var_actout[i] =
		9.98 * sin((double) i * M_PI / (2 * THAHUP_RATE));
	}
}

/* function for assigning values to sensors */
static inline void thahup_set_values()
{
#if defined (WIN32) || defined (_WIN32)
    DWORD mutex_state;
    mutex_state = WaitForSingleObject(mutex, INFINITE);
    if(mutex_state != WAIT_OBJECT_0)
	return;
#else
    pthread_mutex_lock(&mutex);
#endif

    /* add values to array */
    var_thahup->var_t_arr[s_counter] = (val_buff[THAHUP_TMP_IX]>0? val_buff[THAHUP_TMP_IX] : 0.0);
    var_thahup->var_v0_arr[s_counter] = (val_buff[THAHUP_DP1_IX]>0? val_buff[THAHUP_DP1_IX] : 0.0);
    var_thahup->var_v1_arr[s_counter] = (val_buff[THAHUP_DP2_IX]>0? val_buff[THAHUP_DP2_IX] : 0.0);
    var_thahup->var_v2_arr[s_counter] = (val_buff[THAHUP_DP3_IX]>0? val_buff[THAHUP_DP3_IX] : 0.0);
    var_thahup->var_v3_arr[s_counter] = (val_buff[THAHUP_DP4_IX]>0? val_buff[THAHUP_DP4_IX] : 0.0);    
    var_thahup->var_s_arr[s_counter] = (val_buff[THAHUP_ST_IX]>0? val_buff[THAHUP_ST_IX] : 0.0);

    /* call averaging functions */
    /* set values temperature */
    var_thahup->var_tmpsensor->var_raw = 
	Mean(var_thahup->var_t_arr,
	     (max_flg? THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE :
	      s_counter));
    
    /* set values of velocity sensors */
    if(var_thahup->var_velocity->var_v1->var_flg)
	{
	    var_thahup->var_velocity->var_v1->var_raw = 
		Mean(var_thahup->var_v0_arr,
		     (max_flg? THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE :
		      s_counter));
	}

    if(var_thahup->var_velocity->var_v2->var_flg)
	{
	    var_thahup->var_velocity->var_v2->var_raw =
		Mean(var_thahup->var_v1_arr,
		     (max_flg? THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE :
		      s_counter));
	}

    if(var_thahup->var_velocity->var_v3->var_flg)
	{
	    var_thahup->var_velocity->var_v3->var_raw =
		Mean(var_thahup->var_v2_arr,
		     (max_flg? THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE :
		      s_counter));
	}

    if(var_thahup->var_velocity->var_v4->var_flg)
	{
	    var_thahup->var_velocity->var_v4->var_raw =
		Mean(var_thahup->var_v3_arr,
		     (max_flg? THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE :
		      s_counter));
	}

    var_thahup->var_stsensor->var_raw =
	Mean(var_thahup->var_s_arr,
	     (max_flg? THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE :
	      s_counter));	

    if(s_counter == 0)
	{
	    var_thahup->var_act_fd_val =
		(val_buff[THAHUP_ACTFD_IX]>0? val_buff[THAHUP_ACTFD_IX] : 0.0);
	}
    
    /* static pressure */
    var_thahup->var_static_val =
	thgsens_get_value(var_thahup->var_stsensor);

    /* get velocity */
    var_thahup->var_velocity_val =
	thvelsen_get_velocity(var_thahup->var_velocity);

    /* temperature */
    var_thahup->var_temp_val =
	thgsens_get_value(var_thahup->var_tmpsensor);

    /* volume flow */
    if(var_thahup->var_ductdia > 0.0)
	{
	    var_thahup->var_volflow_val =
		M_PI * pow(var_thahup->var_ductdia / 2000, 2) *
		var_thahup->var_velocity_val;
	}
    else
	var_thahup->var_volflow_val = 0.0;

    if(var_thahup->var_volupdate)
	var_thahup->var_volupdate(var_thahup->var_sobj, &var_thahup->var_volflow_val);

    /* record calibration results on zero counter and
     * calibration flag false */
    /*     if(gcounter == 0 && var_thahup->var_calflg == 0) */
    /* 	{ */
    /* 	    thahup_add_xy_to_list(); */
    /* 	    if(var_thahup->var_act_fd_val > 9.9) */
    /* 		{ */
    /* 		    var_thahup->var_calflg = 1; */
    /* #if defined (WIN32) || defined (_WIN32) */
    /* 		    ReleaseSemaphore(var_sem, 1, NULL); */
    /* #else */
    /* 		    sem_post(&var_sem); */
    /* #endif */
    /* 		} */
    /* 	} */
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(mutex);
#else    
    pthread_mutex_unlock(&mutex);
#endif

}

/* get values */
static inline void thahup_write_results()
{
    /* check if file pointer was assigned */
    if(var_thahup->var_fp)
	{
	    fprintf(var_thahup->var_fp, "%i,%f,%f,%f,%f,%f,%f,%f\n",
		    gcounter,
		    THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v1),
		    THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v2),
		    THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v3),
		    THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v4),
		    var_thahup->var_velocity_val,
		    var_thahup->var_volflow_val,
		    var_thahup->var_temp_val);
	}

    /* update screen */
    /* printf("%i\t%7.2f\t%7.2f\t%7.2f\t%7.2f\t%7.2f\t%7.2f\t%7.2f\n", */
    /* 	   gcounter, */
    /* 	   /\* THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v1) *\/val_buff[THAHUP_DP1_IX], */
    /* 	   /\* THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v2) *\/val_buff[THAHUP_DP2_IX], */
    /* 	   /\* THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v3) *\/val_buff[THAHUP_DP3_IX], */
    /* 	   /\* THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v4) *\/val_buff[THAHUP_DP4_IX], */
    /* 	   var_thahup->var_velocity_val, */
    /* 	   var_thahup->var_volflow_val, */
    /* 	   var_thahup->var_temp_val); */
    if(var_thahup->var_result_buff)
	{
	    var_thahup->var_result_buff[THAHUP_RESULT_BUFF_DP1_IX] = thgsens_get_value2(var_thahup->var_velocity->var_v1);
	    var_thahup->var_result_buff[THAHUP_RESULT_BUFF_DP2_IX] = thgsens_get_value2(var_thahup->var_velocity->var_v2);
	    var_thahup->var_result_buff[THAHUP_RESULT_BUFF_DP3_IX] = thgsens_get_value2(var_thahup->var_velocity->var_v3);
	    var_thahup->var_result_buff[THAHUP_RESULT_BUFF_DP4_IX] = thgsens_get_value2(var_thahup->var_velocity->var_v4);
	    var_thahup->var_result_buff[THAHUP_RESULT_BUFF_ST_IX] = var_thahup->var_static_val;
	    var_thahup->var_result_buff[THAHUP_RESULT_BUFF_VEL_IX] = var_thahup->var_velocity_val;
	    var_thahup->var_result_buff[THAHUP_RESULT_BUFF_VOL_IX] = var_thahup->var_volflow_val;
	    var_thahup->var_result_buff[THAHUP_RESULT_BUFF_TMP_IX] = var_thahup->var_temp_val;
	}
}


/* start test asynchronously */
#if defined (WIN32) || defined (_WIN32)
DWORD WINAPI thahup_async_start(LPVOID obj)
#else
void* thahup_async_start(void* obj)
#endif    
{

    int32 spl_write = 0;		/* number of samples written */
    
    counter = 0;		/* reset counter */
    gcounter = 0;		/* reset counter */

    /* flag to indicate if test started */
    start_test = 1;

    /* start both acquiring and writing tasks */
    if(ERR_CHECK(NIStartTask(var_thahup->var_outask)))
#if defined (WIN32) || defined (_WIN32)
	return FALSE;
#else
	return NULL;
#endif
    if(ERR_CHECK(NIStartTask(var_thahup->var_intask)))
#if defined (WIN32) || defined (_WIN32)
	return FALSE;
#else
	return NULL;
#endif

    /* use software timing */
    while(var_thahup->var_stflg)
	{
	    /* check control option */
	    if(var_thahup->var_calflg == 0)
		var_thahup->var_actsignal = 0.0;
	    else if(var_thahup->var_stctrl == thahup_auto)
		{
		    /* comment */
		    printf("pid control\n");
		    var_thahup->var_actsignal =
			var_thahup->var_actout[counter];

		    /* reset counter if exceed limit */
		    if(var_thahup->var_idlflg == 0)
			{
			    if(++counter >= THAHUP_RATE)
				counter = 0;
			}

		    /* call PID controller set the volate output */
		    thpid_pid_control(&var_thpid,
				      var_thahup->var_stopval,
				      var_thahup->var_static_val,
				      &var_thahup->var_actsignal);
				      
		}

	    /* write to out channel */
	    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thahup->var_outask,
					       1,
					       0,
					       10.0,
					       DAQmx_Val_GroupByChannel,
					       (float64*) &var_thahup->var_actsignal,
					       &spl_write,
					       NULL)))
		break;

	    /* wait for semaphore */
/* 	    if(var_thahup->var_calflg == 0) */
/* 		{ */
/* #if defined (WIN32) || defined (_WIN32) */
/* 		    WaitForSingleObject(var_sem, INFINITE); */
/* #else		     */
/* 		    sem_wait(&var_sem); */
/* #endif */
/* 		    thahup_linreg(); */
/* 		} */

#if defined(WIN32) || defined(_WIN32)
	    Sleep(500);
#else
	    sleep(1);
#endif
	    
	    /* check control mode - if automatic, set test to
	       idle when static pressure reach desired */
	    if(var_thahup->var_stctrl == thahup_auto &&
	       var_thahup->var_static_val > var_thahup->var_stopval)
		var_thahup->var_idlflg = 1;

	    /* call to write results to file and screen */
	    thahup_write_results();
	    gcounter++;
	}

    /* indicate test stopped */
    start_test = 0;
#if defined (WIN32) || defined (_WIN32)
    return TRUE;
#else
    return NULL;
#endif
}

/*************************************************************/

/* constructor */
int thahup_initialise(thahup_stopctrl ctrl_st,		/* start control */
		      FILE* fp,				/* file pointer */
		      gthsen_fptr update_vol,		/* function pointer for volume flow */
		      gthsen_fptr update_vel,		/* function pointer for velocity flow */
		      gthsen_fptr update_tmp,		/* function pointer for temperature update */
		      gthsen_fptr update_static,	/* function pointer static pressure */
		      gthsen_fptr update_dp1,		/* function pointer dp 1 */
		      gthsen_fptr update_dp2,		/* function pointer dp 2 */
		      gthsen_fptr update_dp3,		/* function pointer dp 3 */
		      gthsen_fptr update_dp4,		/* function pointer dp 4 */		      
		      thahup** obj,			/* argument optional */
		      void* sobj)			/* optional argument to pass to
							 * function calls */
{
    /* create ahu test object */
    var_thahup = (thahup*) malloc(sizeof(thahup));
    
    if(!var_thahup)
	return 1;

    /* create output task */
    if(ERR_CHECK(NICreateTask("", &var_thahup->var_outask)))
	return 1;

    /* create input task */
    if(ERR_CHECK(NICreateTask("", &var_thahup->var_intask)))
	{
	    thahup_clear_tasks();
	    return 1;
	}

    /* set attributes */
    var_thahup->var_volupdate = update_vol;
    var_thahup->var_velupdate = update_vel;
    var_thahup->var_fp = fp;
    var_thahup->var_ductdia = 0.0;
    var_thahup->var_actout = NULL;
    var_thahup->var_actsignal = 0.0;
    var_thahup->var_stopval = 0.0;

    var_thahup->var_volflow_val = 0.0;
    var_thahup->var_velocity_val = 0.0;
    var_thahup->var_static_val = 0.0;
    var_thahup->var_temp_val = 0.0;
    
    var_thahup->var_smplerate = 0.0;

    var_thahup->var_thrid = 0;
    var_thahup->var_stflg = 0;
    var_thahup->var_idlflg = 0;

    var_thahup->var_act_fd_val = 0;
    var_thahup->var_act_fd_minval =
	THAHUP_MIN_FEEDBACK_VOLT;

    var_thahup->var_sobj = sobj;
    var_thahup->var_stctrl = ctrl_st;
    var_thahup->var_calflg = 0;

    var_thahup->var_v0_arr = NULL;
    var_thahup->var_v1_arr = NULL;
    var_thahup->var_v2_arr = NULL;
    var_thahup->var_v3_arr = NULL;
    var_thahup->var_s_arr = NULL;
    var_thahup->var_t_arr = NULL;
    var_thahup->var_result_buff = NULL;
    /* create output channel for actuator control */
    if(ERR_CHECK(NICreateAOVoltageChan(var_thahup->var_outask,
				       THAHUP_ACT_CTRL_CHANNEL,
				       "",
				       0.0,
				       10.0,
				       DAQmx_Val_Volts,
				       NULL)))
	{
	    printf("%s\n","create output channel failed!");
	    thahup_clear_tasks();
	    return 1;
	}
    
    /* input voltage channel for actuator feedback */
    if(ERR_CHECK(NICreateAIVoltageChan(var_thahup->var_intask,
				       THAHUP_ACT_FD_CHANNEL,
				       "",
				       DAQmx_Val_Diff,
				       0.0,
				       10.0,
				       DAQmx_Val_Volts,
				       NULL)))
	{
	    printf("%s\n","create act feedback channel failed!");
	    thahup_clear_tasks();	    
	    return 1;
	}
    
    /* create temperature sensor */
    if(!thgsens_new(&var_thahup->var_tmpsensor,
		    THAHUP_TMP_CHANNEL,
		    &var_thahup->var_intask,
		    update_tmp,
		    sobj))
	{
	    printf ("%s\n","error creating temperature sensor");
	    thahup_clear_tasks();
	    return 1;
	}

    /* create static sensor */
    if(!thgsens_new(&var_thahup->var_stsensor,
		    THAHUP_ST_CHANNEL,
		    &var_thahup->var_intask,
		    update_static,
		    sobj))
	{
	    printf ("%s\n","error creating static sensor");
	    	    thgsens_delete(&var_thahup->var_tmpsensor);
	    thahup_clear_tasks();
	    return 1;
	}
    
    /* create velocity sensor */
    if(!thvelsen_new(&var_thahup->var_velocity,
		     &var_thahup->var_intask,
		     update_vel,
		     sobj))
	{
	    printf ("%s\n","error creating velocity sensor");
	    thgsens_delete(&var_thahup->var_tmpsensor);
	    thgsens_delete(&var_thahup->var_stsensor);
	    thahup_clear_tasks();
	    return 1;
	}

    printf("%s\n","configuring velocity sensors..");
    /* configure velocity sensors */
    thvelsen_config_sensor(var_thahup->var_velocity,
    			   0,
    			   THAHUP_VEL_DP1_CHANNEL,
    			   update_dp1,
    			   THAHUP_MIN_RNG,
    			   THAHUP_MAX_RNG);
    
    thvelsen_config_sensor(var_thahup->var_velocity,
    			   1,
    			   THAHUP_VEL_DP2_CHANNEL,
    			   update_dp2,
    			   THAHUP_MIN_RNG,
    			   THAHUP_MAX_RNG);

    thvelsen_config_sensor(var_thahup->var_velocity,
    			   2,
    			   THAHUP_VEL_DP3_CHANNEL,
    			   update_dp3,
    			   THAHUP_MIN_RNG,
    			   THAHUP_MAX_RNG);

    thvelsen_config_sensor(var_thahup->var_velocity,
    			   3,
    			   THAHUP_VEL_DP4_CHANNEL,
    			   update_dp4,
    			   THAHUP_MIN_RNG,
    			   THAHUP_MAX_RNG);

    /* Configure timing */
    if(ERR_CHECK(NICfgSampClkTiming(var_thahup->var_intask,
    				   "OnboardClock",
    				   THAHUP_SAMPLES_PERSECOND,
    				   DAQmx_Val_Rising,
    				   DAQmx_Val_ContSamps,
    				   1)))
    	{
    	    thahup_clear_tasks();
    	    return 1;
    	}

    /* Register callbacks */
#if defined(WIN32) || defined(_WIN32)    
    if(ERR_CHECK(NIRegisterEveryNSamplesEvent(var_thahup->var_intask,
    					      DAQmx_Val_Acquired_Into_Buffer,
    					      1,
    					      0,
    					      EveryNCallback,
    					      NULL)))
    	{
    	    thahup_clear_tasks();
    	    return 1;
    	}
    
    if(ERR_CHECK(NIRegisterDoneEvent(var_thahup->var_intask,
    				     0,
    				     DoneCallback,
    				     NULL)))
    	{
    	    thahup_clear_tasks();
    	    return 1;
    	}
#endif

    /* initialistation complete */
    printf("%s\n","initialisation complete..");

    /* actuator control signals */
    thahup_actout_signals();

    /* initialise structs for linear regression */
    thahup_linreg();

    /* initialise PID control struct */
    thpid_init(&var_thpid);

    if(obj)
        *obj = var_thahup;

    /* initialise mutex */
#if defined (WIN32) || defined (_WIN32)
    mutex = CreateMutex(NULL,				/* default security */
			FALSE,				/* initially not owned */
			NULL);				/* no name */

    var_sem = CreateSemaphore(NULL,			/* default security */
			      0,			/* initial sem count */
			      10,			/* maximum sem count */
			      NULL);
#else    
    pthread_mutex_init(&mutex, NULL);

    /* initialise semaphore */
    sem_init(&var_sem, 0, 0);
#endif
    /* enable all sensors by default */
    thvelsen_enable_sensor(var_thahup->var_velocity, 0);
    thvelsen_enable_sensor(var_thahup->var_velocity, 1);
    thvelsen_enable_sensor(var_thahup->var_velocity, 2);
    thvelsen_enable_sensor(var_thahup->var_velocity, 3);    
    return 0;
}

/* stop all running tasks */
void thahup_delete()
{
    printf("%s\n","deleting sensors...");

    /* destroy sensors */
    thvelsen_delete(&var_thahup->var_velocity);
    thgsens_delete(&var_thahup->var_stsensor);
    thgsens_delete(&var_thahup->var_tmpsensor);

    /* clean up */
    printf("%s\n","clean up");

    /* set function pointers to NULL */
    var_thahup->var_volupdate = NULL;
    var_thahup->var_velupdate = NULL;
    var_thahup->var_v0_arr = NULL;
    var_thahup->var_v1_arr = NULL;
    var_thahup->var_v2_arr = NULL;
    var_thahup->var_v2_arr = NULL;    
    var_thahup->var_s_arr = NULL;
    var_thahup->var_t_arr = NULL;
    var_thahup->var_fp = NULL;
    var_thahup->var_result_buff = NULL;
    
    /* delete signal array */
    free(var_thahup->var_actout);
    var_thahup->var_actout = NULL;

    /* remote object pointer set to NULL */
    var_thahup->var_sobj = NULL;

    thlinreg_delete(&var_theq);
    free(var_thxy);

    thpid_delete(&var_thpid);

    /* clear tasks */
    printf("%s\n","clear NIDAQmx tasks");
    
    thahup_clear_tasks();

#if defined (WIN32) || defined (_WIN32)
    CloseHandle(mutex);				/* destroy mutex */
    CloseHandle(var_sem);			/* destroy semaphore */
#else
    pthread_mutex_destroy(&mutex);
    sem_destroy(&var_sem);
#endif
    printf("%s\n","delete thor..");
    free(var_thahup);
    
    var_thahup = NULL;
}

/* Duct diameter */
inline void thahup_set_ductdia(thahup* obj,
			       double val)
{
    if(obj)
	obj->var_ductdia = val;
    else
	var_thahup->var_ductdia = val;
}

/* start test */
int thahup_start(thahup* obj)
{
    int i = 0;
    /* check if sensor range is assigned */
    if(!var_thahup->var_stsensor->var_okflg)
	{
	    printf("%s\n","static sensor range not set");
	    return 1;
	}

    if(!var_thahup->var_tmpsensor->var_okflg)
	{
	    printf("%s\n","temperature sensor range not set");
	    return 1;
	}

    if(!var_thahup->var_velocity->var_okflg)
	{
	    printf("%s\n","velocity sensors range not set");
	    return 1;
	}

    /* reset flags */
    var_thahup->var_stflg = 1;		/* start flag to on */
    var_thahup->var_idlflg = 0;		/* idle flag to off */

    /* create arrays */
    var_thahup->var_v0_arr = (double*) calloc(THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE,
					      sizeof(double));
    var_thahup->var_v1_arr = (double*) calloc(THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE,
					      sizeof(double));
    var_thahup->var_v2_arr = (double*) calloc(THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE,
					      sizeof(double));
    var_thahup->var_v3_arr = (double*) calloc(THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE,
					      sizeof(double));    
    var_thahup->var_s_arr = (double*) calloc(THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE,
					      sizeof(double));
    var_thahup->var_t_arr = (double*) calloc(THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE,
					      sizeof(double));    
    /* initialise array values */
    for(i=0;i<(THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE);i++)
	{
	    var_thahup->var_v0_arr[i] = 0.0;
	    var_thahup->var_v1_arr[i] = 0.0;
	    var_thahup->var_v2_arr[i] = 0.0;
	    var_thahup->var_v3_arr[i] = 0.0;
	    var_thahup->var_s_arr[i] = 0.0;
	    var_thahup->var_t_arr[i] = 0.0;
	}

    /* initialise thread attributes */
#if defined (WIN32) || defined (_WIN32)
    thread = CreateThread(NULL,			/* default security attribute */
    		     0,				/* use default stack size */
    		     thahup_async_start,	/* thread function */
    		     NULL,			/* argument to thread function */
    		     0,				/* default creation flag */
    		     &var_thahup->var_thrid);	/* thread id */
#else    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    var_thahup->var_thrid =
	pthread_create(&thread, &attr, thahup_async_start, NULL);
#endif

    return 0;
}


/* stop the test */
int thahup_stop(thahup* obj)
{
    float64 var_act_st_val = 0.0;	/* actuator stop val */
    int32 spl_write = 0;
    fprintf(stderr, "closing initialised\n");
    /* free buffers */
    free(var_thahup->var_v0_arr);
    free(var_thahup->var_v1_arr);
    free(var_thahup->var_v2_arr);
    free(var_thahup->var_v3_arr);
    free(var_thahup->var_s_arr);
    free(var_thahup->var_t_arr);
    /* set stop flag and join thread */

    /* lock mutex */
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(mutex, INFINITE);
#else
    pthread_mutex_lock(&mutex);
#endif
    var_thahup->var_stflg = 0;
    /* if process waiting on semaphore post to
     * exit */
    if(gcounter == 0 && var_thahup->var_calflg == 0)
	{
#if defined (WIN32) || defined (_WIN32)
	    ReleaseSemaphore(var_sem, 1, NULL);
#else
	    sem_post(&var_sem);
#endif
	}
#if defined (WIN32) || defined (_WIN32)
   ReleaseMutex(mutex);

   /* Call to terminate thread */
   TerminateThread(thread, 0);
#else
    pthread_mutex_unlock(&mutex);
#endif

    /* stop actuator */
    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thahup->var_outask,
				       1,
				       0,
				       10.0,
				       DAQmx_Val_GroupByChannel,
				       &var_act_st_val,
				       &spl_write,
				       NULL)))
	printf("%s\n","actuator not stopped");

    /* stop task */
    ERR_CHECK(NIStopTask(var_thahup->var_outask));
    ERR_CHECK(NIStopTask(var_thahup->var_intask));
    /* wait until thread finishes if test in progress */
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

    /* function stop notify */
    printf("%s\n","test stopped");

    /* stopped complete */
    printf("%s\n","complete..");
    return 0;
}


/* set actuator control voltage */
int thahup_set_actctrl_volt(double percen)
{
    if(percen < 0 || percen > 100 || !var_thahup)
	return 1;

    /* lock mutex */
    printf("%s\n","setting voltage");
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(mutex, INFINITE);
#else
    pthread_mutex_lock(&mutex);
#endif
    var_thahup->var_actsignal = 9.95 * percen / 100;
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(mutex);
#else
    pthread_mutex_unlock(&mutex);
#endif

    return 0;
}

/* reset sensors */
int thahup_reset_sensors(thahup* obj)
{
    var_thahup->var_actsignal = 0.0;
    var_thahup->var_act_fd_val = 0.0;

    thgsens_reset_value(var_thahup->var_velocity->var_v1);
    thgsens_reset_value(var_thahup->var_velocity->var_v2);
    thgsens_reset_value(var_thahup->var_velocity->var_v3);
    thgsens_reset_value(var_thahup->var_velocity->var_v4);
    
    thgsens_reset_all(var_thahup->var_stsensor);
    thgsens_reset_all(var_thahup->var_tmpsensor);

    return 0;
}

/*=======================================================*/
/* Every N sample callback */
int32 CVICALLBACK EveryNCallback(TaskHandle taskHandle,
				 int32 everyNsamplesEventType,
				 uInt32 nSamples,
				 void *callbackData)
{
    int32 spl_read = 0;		/* samples read */
    
    /* Read data into buffer */
    if(ERR_CHECK(NIReadAnalogF64(var_thahup->var_intask,
				 1,
				 10.0,
				 DAQmx_Val_GroupByScanNumber,
				 val_buff,
				 THAHUP_NUM_INPUT_CHANNELS,
				 &spl_read,
				 NULL)))
	return 1;

    /* Call to set values */
    thahup_set_values();
    if(++s_counter >= (THAHUP_SAMPLES_PERSECOND * THAHUP_UPDATE_RATE))
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
