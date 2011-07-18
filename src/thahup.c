/* Implementation of ahu test object */

#include "thahup.h"
#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <pthread.h>
#include <math.h>

/* object */
static thahup* var_thahup = NULL;			/* ahu test object */

#define THAHUP_ACT_CTRL_CHANNEL "dev1/ao0"
/* channel names for sensors */
#define THAHUP_TMP_CHANNEL "dev1/ai0"			/* temperature sensor */

#define THAHUP_VEL_DP1_CHANNEL "dev1/ai1"		/* differential pressure velocity */
#define THAHUP_VEL_DP2_CHANNEL "dev1/ai2"		/* differential pressure velocity */
#define THAHUP_VEL_DP3_CHANNEL "dev1/ai3"		/* differential pressure velocity */
#define THAHUP_VEL_DP4_CHANNEL "dev1/ai4"		/* differential pressure velocity */

#define THAHUP_ST_CHANNEL "dev1/ai5"			/* static pressure sensor */

#define THAHUP_MIN_RNG 0.0				/* default minimum range for velocity sensors */
#define THAHUP_MAX_RNG 500.0				/* default maximum range for velocity sensors */

/*******************************************/
#define THAHUP_BUFF_SZ 2048				/* message buffer */
#define THAHUP_RATE 1000.0				/* actuator rate control */

#define THAHUP_NUM_INPUT_CHANNELS 6			/* number of channels */

#define THAHUP_CHECK_VSENSOR(obj) \
    obj? (obj->var_flg? obj->var_val : 0.0) : 0.0

static char err_msg[THAHUP_BUFF_SZ];
static unsigned int counter = 0;			/* counter */
static unsigned int gcounter = 0;

/* buffer to hold input values */
static float64 val_buff[THAHUP_NUM_INPUT_CHANNELS];

/* thread and thread attributes objects */
static pthread_attr_t attr;
static pthread_t thread;
static pthread_mutex_t mutex;

/* start test flag */
static int start_test = 0;

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
    /* set values temperature */
    var_thahup->var_tmpsensor->var_raw =
	(val_buff[0]>0? val_buff[0] : 0.0);
    
    /* set values of velocity sensors */
    if(var_thahup->var_velocity->var_v1->var_flg)
	{
	    var_thahup->var_velocity->var_v1->var_raw =
		(val_buff[1]>0? val_buff[1] : 0.0);
	}

    if(var_thahup->var_velocity->var_v2->var_flg)
	{
	    var_thahup->var_velocity->var_v2->var_raw =
		(val_buff[2]>0? val_buff[2] : 0.0);
	}

    if(var_thahup->var_velocity->var_v3->var_flg)
	{
	    var_thahup->var_velocity->var_v3->var_raw =
		(val_buff[3]>0? val_buff[3] : 0.0);
	}

    if(var_thahup->var_velocity->var_v4->var_flg)
	{
	    var_thahup->var_velocity->var_v4->var_raw =
		(val_buff[4]>0? val_buff[4] : 0.0);
	}

    var_thahup->var_stsensor->var_raw =
	(val_buff[5]>0? val_buff[5] : 0.0);

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
    var_thahup->var_volflow_val =
	M_PI * pow(var_thahup->var_ductdia / 2000, 2) *
	var_thahup->var_velocity_val;

    if(var_thahup->var_volupdate)
	var_thahup->var_volupdate(var_thahup->var_sobj, &var_thahup->var_volflow_val);

}

/* get values */
static inline void thahup_write_results()
{
    /* check if file pointer was assigned */
    if(var_thahup->var_fp)
	{
	    fprintf(var_thahup->var_fp, "%f,%f,%f,%f,%f,%f,%f\n",
		    THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v1),
		    THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v2),
		    THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v3),
		    THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v4),
		    var_thahup->var_velocity_val,
		    var_thahup->var_volflow_val,
		    var_thahup->var_temp_val);
	}

    /* update screen */
    printf("%i\t%7.2f\t%7.2f\t%7.2f\t%7.2f\t%7.2f\t%7.2f\t%7.2f\n",
	   gcounter,
	   THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v1),
	   THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v2),
	   THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v3),
	   THAHUP_CHECK_VSENSOR(var_thahup->var_velocity->var_v4),
	   var_thahup->var_velocity_val,
	   var_thahup->var_volflow_val,
	   var_thahup->var_temp_val);
}


/* start test asynchronously */
void* thahup_async_start(void* obj)
{
    counter = 0;		/* reset counter */
    gcounter = 0;		/* reset counter */

    /* flag to indicate if test started */
    start_test = 1;

    /* start both acquiring and writing tasks */
    if(ERR_CHECK(NIStartTask(var_thahup->var_outask)))
	return NULL;
    if(ERR_CHECK(NIStartTask(var_thahup->var_intask)))
	return NULL;

    float64 var_act_st_val = 0.0;	/* actuator stop val */
    int32 spl_read = 0;			/* number of samples read */
#if !defined (WIN32) || !defined(_WIN32)
    int32 spl_write = 0;		/* number of samples written */
#endif
    /* output to screen */
    printf("Counter\tDP1\tDP2\tDP3\tDP4\tV\tVol\tTmp\n");

    /* use software timing */
    while(var_thahup->var_stflg)
	{
#if defined(WIN32) || defined(_WIN32)
	    Sleep(500);
#else
	    sleep(1);
#endif
	    /* check control option */
	    if(var_thahup->var_stctrl == thahup_auto)
		{
		    var_thahup->var_actsignal =
			var_thahup->var_actout[counter];

		    /* reset counter if exceed limit */
		    if(var_thahup->var_idlflg == 0)
			{
			    if(++counter >= THAHUP_RATE)
				counter = 0;
			}
		}

	    /* write to out channel */
#if defined (WIN32) || defined (_WIN32)
	    if(ERR_CHECK(NIWriteAnalogF64(var_thahup->var_outask,
					  0,
					  10.0,
					  (float64) var_thahup->var_actsignal,
					  NULL)))
		break;
#else
	    if(ERR_CHECK(NIWriteAnalogF64(var_thahup->var_outask,
					  1,
					  0,
					  10.0,
					  DAQmx_Val_GroupByChannel,
					  (float64*) &var_thahup->var_actsignal,
					  &spl_write,
					  NULL)))
		break;
#endif

	    /* read channel values */
	    if(ERR_CHECK(NIReadAnalogF64(var_thahup->var_intask,
					 1,
					 10.0,
					 DAQmx_Val_GroupByScanNumber,
					 val_buff,
					 THAHUP_NUM_INPUT_CHANNELS,
					 &spl_read,
					 NULL)))
		break;

	    /* call function to assign values to sensors */
	    thahup_set_values();

	    /* check control mode - if automatic, set test to
	       idle when static pressure reach desired */
	    if(var_thahup->var_stctrl == thahup_auto &&
	       var_thahup->var_static_val > var_thahup->var_stopval)
		var_thahup->var_idlflg = 1;

	    /* call to write results to file and screen */
	    thahup_write_results();
	    gcounter++;
	}

    /* stop actuator */
#if defined (WIN32) || defined (_WIN32)
    if(ERR_CHECK(NIWriteAnalogF64(var_thahup->var_outask,
				  0,
				  10.0,
				  var_act_st_val,
				  NULL)))
	printf("%s\n","actuator not stopped");
#else
    if(ERR_CHECK(NIWriteAnalogF64(var_thahup->var_outask,
				  1,
				  0,
				  10.0,
				  DAQmx_Val_GroupByChannel,
				  &var_act_st_val,
				  &spl_write,
				  NULL)))
	printf("%s\n","actuator not stopped");
#endif


    /* stop task */
    ERR_CHECK(NIStopTask(var_thahup->var_outask));
    ERR_CHECK(NIStopTask(var_thahup->var_intask));

    /* indicate test stopped */
    start_test = 0;
    return NULL;
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

    /* create velocity sensor */
    if(!thvelsen_new(&var_thahup->var_velocity,
		     &var_thahup->var_intask,
		     update_vel,
		     sobj))
	{
	    printf ("%s\n","error creating velocity sensor");
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
	    thvelsen_delete(&var_thahup->var_velocity);
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
	    thvelsen_delete(&var_thahup->var_velocity);
	    thgsens_delete(&var_thahup->var_stsensor);
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

    var_thahup->var_sobj = sobj;
    var_thahup->var_stctrl = ctrl_st;

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

    /* initialistation complete */
    printf("%s\n","initialisation complete..");

    /* actuator control signals */
    thahup_actout_signals();

    if(obj)
        *obj = var_thahup;

    /* initialise mutex */
    pthread_mutex_init(&mutex, NULL);

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

    var_thahup->var_fp = NULL;

    /* delete signal array */
    free(var_thahup->var_actout);
    var_thahup->var_actout = NULL;

    /* remote object pointer set to NULL */
    var_thahup->var_sobj = NULL;

    /* clear tasks */
    printf("%s\n","clear NIDAQmx tasks");
    
    thahup_clear_tasks();

    pthread_mutex_destroy(&mutex);

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

    /* initialise thread attributes */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    var_thahup->var_thrid =
	pthread_create(&thread, &attr, thahup_async_start, NULL);

    return 0;
}


/* stop the test */
int thahup_stop(thahup* obj)
{
    /* set stop flag and join thread */

    /* lock mutex */
    pthread_mutex_lock(&mutex);
    var_thahup->var_stflg = 0;
    pthread_mutex_unlock(&mutex);

    /* wait until thread finishes if test in progress */
    if(start_test)
	pthread_join(thread, NULL);

    pthread_attr_destroy(&attr);    

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
    pthread_mutex_lock(&mutex);
    var_thahup->var_actsignal = 9.95 * percen / 100;
    pthread_mutex_unlock(&mutex);

    return 0;
}

/* reset sensors */
int thahup_reset_sensors(thahup* obj)
{
    var_thahup->var_actsignal = 0.0;

    thgsens_reset_value(var_thahup->var_velocity->var_v1);
    thgsens_reset_value(var_thahup->var_velocity->var_v2);
    thgsens_reset_value(var_thahup->var_velocity->var_v3);
    thgsens_reset_value(var_thahup->var_velocity->var_v4);
    
    thgsens_reset_all(var_thahup->var_stsensor);
    thgsens_reset_all(var_thahup->var_tmpsensor);

    return 0;
}
