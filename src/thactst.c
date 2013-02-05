/* Implementation of interfaces for damper slam shut test */

#if defined (WIN32) || defined (_WIN32)
#include <windows.h>
#endif
#include <math.h>
#include "thactst.h"

#define THACTST_NUM_OUT_CHANNELS 2				/* output channels */
#define THACTST_NUM_INPUT_CHANNELS 4				/* input channels */

#define THACTST_OPEN_SOV 4.0					/* open SOV voltage */
#define THACTST_CLOSE_SOV 0.0					/* close SOV voltage */

#define THACTST_FAN_CTRL_CHANNEL "Dev1/ao0"
#define THACTST_SOV_CTRL_CHANNEL "Dev1/ao1"

#define THACTST_TMP_CHANNEL "Dev1/ai1"				/* temperature channel */
#define THACTST_ST_CHANNEL "Dev1/ai2"				/* static pressure channel */
#define THACTST_VEL_DP1_CHANNEL "Dev1/ai3"			/* dp pressure velocity */
#define THACTST_VEL_DP2_CHANNEL "Dev1/ai4"			/* dp pressure velocity */

#define THACTST_MIN_RNG 0.0					/* default minimum range for velocity probes */
#define THACTST_MAX_RNG 1600.0					/* default maximum range for velocity probes */
#define THACTST_BUFF_SZ 2048
#define THACTST_ACT_RATE 1000					/* actuator rate */
#define THACTST_SAMPLES_PERSECOND 8
#define THACTST_UPDATE_RATE 3
#define THACTST_MAX_VELOCITY 5.0
#define THACTST_FAN_SPEED_ADJ 9.98
#define THACTST_SLEEP_TIME 500
#define THACTST_SLEEP_FAN_TIME 500
#define THACTST_MAX_WAIT 9

/* Object implementation */
struct _thactst
{
    TaskHandle var_outtask;		       			/* analog output task */
    TaskHandle var_intask;					/* analog input task */

    thgsens* var_stsensor;					/* static pressure sensor */
    thgsens* var_tmpsensor;					/* temperature sensor */
    thvelsen* var_velocity;					/* velocity sensors */

    unsigned int var_stflg;					/* start flag */
    unsigned int var_idlflg;					/* idle flag */
    double var_ductdia;						/* duct diameter */
    double var_width;						/* damper width */
    double var_height;						/* damper height */
    double var_max_v;						/* maximum velocity at damper */

    double var_tmp;						/* temperature */
    double var_st_pre;						/* static pressure */
    double var_v_duct;						/* velocity in duct */
    double var_v_dmp;						/* velocity damper */
    double var_vol;						/* volume flow */
    double var_out_v[THACTST_NUM_OUT_CHANNELS];			/* output voltage */
    double* var_ext_percen;					/* persentage */
    double* var_result_buff;					/* result buffer */
    double* var_fan_ctrl_buff;
    double* var_v0_arr;
    double* var_v1_arr;
    double* var_st_arr;
    double* var_tmp_arr;
    FILE* var_fp;						/* file pointer */
#if defined (WIN32) || defined (_WIN32)
    DWORD var_thid;						/* thread id */
#else
    int var_thid;
#endif
};

/* local variables */
static unsigned int _init_flg = 0;			        /* initialise flag */
static unsigned int _g_counter;					/* general counter */
static unsigned int _s_counter;
static unsigned int _max_flg;					/* maximum flag for controlling averaging buffers */
static struct _thactst var_thactst;				/* object */
static double _var_st_x[] = THORNIFIX_ST_X;
static double _var_st_y[] = THORNIFIX_ST_Y;
static double _var_val_buff[THACTST_NUM_INPUT_CHANNELS];	/* read buffer */
#if defined (WIN32) || defined (_WIN32)
static HANDLE _mutex = NULL;
static HANDLE _thread = NULL;
#endif

/* Private functions */
static void _thactst_clear_tasks();				/* clear tasks */
static void _thactst_set_value();
static void _thactst_write_results();
#if defined (WIN32) || defined (_WIN32)
DWORD WINAPI _thactst_async_start(LPVOID obj);
#else
void* _thactst_async_start(void* obj);
#endif
static int32 CVICALLBACK _every_n_callback(TaskHandle taskHandle,
					   int32 everyNsamplesEventType,
					   uInt32 nSamples,
					   void *callbackData);

static int32 CVICALLBACK _done_callback(TaskHandle taskHandle,
					int32 status,
					void *callbackData);
static int _thactst_actout_signal();


#define THACTST_DELETE_ALL			\
    thgsens_delete(&var_thactst.var_tmpsensor);	\
    thgsens_delete(&var_thactst.var_stsensor);	\
    thvelsen_delete(&var_thactst.var_velocity);	\
    _thactst_clear_tasks()

/* Constructor */
int thactst_initialise(FILE* fp, thactst* obj, void* sobj)
{
    /* create output task */
    fprintf(stderr, "Creating and initialising tasks\n");
    if(ERR_CHECK(NICreateTask("", &var_thactst.var_outtask)))
	return 1;

    /* create input task */
    if(ERR_CHECK(NICreateTask("", &var_thactst.var_intask)))
	{
	    _thactst_clear_tasks();
	    return 1;
	}

    /* intialise variables */
    fprintf(stderr, "Initialising private variables..\n");
    var_thactst.var_stsensor = NULL;
    var_thactst.var_tmpsensor = NULL;
    var_thactst.var_velocity = NULL;

    var_thactst.var_stflg = 0;
    var_thactst.var_idlflg = 0;
    var_thactst.var_ductdia = 0.0;
    var_thactst.var_width = 0.0;
    var_thactst.var_height = 0.0;
    var_thactst.var_max_v = THACTST_MAX_VELOCITY;

    var_thactst.var_st_pre = 0.0;
    var_thactst.var_v_duct = 0.0;
    var_thactst.var_v_dmp = 0.0;
    var_thactst.var_vol = 0.0;
    var_thactst.var_out_v[0] = 0.0;
    var_thactst.var_out_v[1] = 0.0;
    var_thactst.var_result_buff = NULL;
    var_thactst.var_fp = NULL;
    var_thactst.var_ext_percen = NULL;

    fprintf(stderr, "Creating fan control signal\n");
    _thactst_actout_signal();
    
    /* create fan control channel */
    if(ERR_CHECK(NICreateAOVoltageChan(var_thactst.var_outtask,
				       THACTST_FAN_CTRL_CHANNEL,
				       "",
				       0.0,
				       10.0,
       				       DAQmx_Val_Volts,
				       NULL)))
	{
	    fprintf(stderr, "create output channel failed!\n");
	    _thactst_clear_tasks();
	    return 1;
	}

    /* create SOV control channel */
    if(ERR_CHECK(NICreateAOVoltageChan(var_thactst.var_outtask,
				       THACTST_SOV_CTRL_CHANNEL,
				       "",
				       0.0,
				       10.0,
				       DAQmx_Val_Volts,
				       NULL)))
	{
	    fprintf(stderr, "create SOV channel failed!\n");
	    _thactst_clear_tasks();
	    return 1;
	}
    fprintf(stderr, "Output channels configure complete..\n");
    /* create temperature sensor */
    if(!thgsens_new(&var_thactst.var_tmpsensor,
		    THACTST_TMP_CHANNEL,
		    &var_thactst.var_intask,
		    NULL,
		    sobj))
	{
	    printf ("%s\n","error creating temperature sensor");
	    _thactst_clear_tasks();
	    return 1;
	}

    /* create static sensor */
    if(!thgsens_new(&var_thactst.var_stsensor,
		    THACTST_ST_CHANNEL,
		    &var_thactst.var_intask,
		    NULL,
		    sobj))
	{
	    printf ("%s\n","error creating static sensor");
	    thgsens_delete(&var_thactst.var_tmpsensor);
	    _thactst_clear_tasks();
	    return 1;
	}
    thgsens_set_calibration_buffers(var_thactst.var_stsensor, _var_st_x, _var_st_y, THORNIFIX_S_CAL_SZ);

    /* create velocity sensor */
    if(!thvelsen_new(&var_thactst.var_velocity,
		     &var_thactst.var_intask,
		     NULL,
		     sobj))
	{
	    printf ("%s\n","error creating velocity sensor");
	    thgsens_delete(&var_thactst.var_tmpsensor);
	    thgsens_delete(&var_thactst.var_stsensor);
	    _thactst_clear_tasks();
	    return 1;
	}

    fprintf(stderr,"configuring velocity sensors..\n");
    /* configure velocity sensors */
    thvelsen_config_sensor(var_thactst.var_velocity,
    			   0,
    			   THACTST_VEL_DP1_CHANNEL,
    			   NULL,
    			   THACTST_MIN_RNG,
    			   THACTST_MAX_RNG);

    thvelsen_config_sensor(var_thactst.var_velocity,
    			   1,
    			   THACTST_VEL_DP2_CHANNEL,
    			   NULL,
    			   THACTST_MIN_RNG,
    			   THACTST_MAX_RNG);
    fprintf(stderr, "All sensors configured\n");
    /* Configure timing */
    if(ERR_CHECK(NICfgSampClkTiming(var_thactst.var_intask,
				    "OnboardClock",
				    THACTST_SAMPLES_PERSECOND,
				    DAQmx_Val_Rising,
				    DAQmx_Val_ContSamps,
				    1)))
    	{
	    THACTST_DELETE_ALL;
    	    return 1;
    	}
    /* register callbacks */
#if defined (WIN32) || defined (_WIN32)
    if(ERR_CHECK(NIRegisterEveryNSamplesEvent(var_thactst.var_intask,
    					      DAQmx_Val_Acquired_Into_Buffer,
    					      1,
    					      0,
    					      _every_n_callback,
    					      NULL)))
    	{
	    THACTST_DELETE_ALL;
    	    return 1;
    	}

    if(ERR_CHECK(NIRegisterDoneEvent(var_thactst.var_intask,
    				     0,
    				     _done_callback,
    				     NULL)))
    	{
	    THACTST_DELETE_ALL;
    	    return 1;
    	}
    _mutex = CreateMutex(NULL, FALSE, NULL);
#endif

    /* initialisation complete */
    fprintf(stderr, "Initialisation complete..\n");

    /* enable all sensors by default */
    thvelsen_enable_sensor(var_thactst.var_velocity, 0);
    thvelsen_enable_sensor(var_thactst.var_velocity, 1);
    _init_flg = 1;
    return 0;
}

/* stop all running tasks */
void thactst_delete(void)
{
    if(_init_flg == 0)
	return;
    _init_flg = 0;
    printf("%s\n","deleting sensors...");

    /* destroy sensors */
    thvelsen_delete(&var_thactst.var_velocity);
    thgsens_delete(&var_thactst.var_stsensor);
    thgsens_delete(&var_thactst.var_tmpsensor);

    /* clean up */
    printf("%s\n","clean up");
    var_thactst.var_stsensor = NULL;
    var_thactst.var_tmpsensor = NULL;
    var_thactst.var_velocity = NULL;
    var_thactst.var_result_buff = NULL;
    var_thactst.var_ext_percen = NULL;
    if(var_thactst.var_fan_ctrl_buff)
	free(var_thactst.var_fan_ctrl_buff);
    var_thactst.var_fan_ctrl_buff = NULL;
    _thactst_clear_tasks();
#if defined (WIN32) || defined (_WIN32)
    CloseHandle(_mutex);
#endif
    printf("delete thor..\n");
}

/* Set duct diameter */
int thactst_set_diameter(double dia)
{
    var_thactst.var_ductdia = dia;
    return 0;
}

/* Set damper size */
int thactst_set_damper_sz(double width, double height)
{
    var_thactst.var_width = width;
    var_thactst.var_height = height;
    return 0;
}

/* set maximum velocity */
int thactst_set_max_velocity(double val)
{
    var_thactst.var_max_v = val;
    return 0;
}

/* set fan speed */
int thactst_set_fan_speed(double* percen)
{
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_mutex, INFINITE);
#endif
    if(var_thactst.var_ext_percen == NULL)
	var_thactst.var_ext_percen = percen;
    var_thactst.var_out_v[0] = THACTST_FAN_SPEED_ADJ * (*percen) / 100;
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(_mutex);
#endif
    return 0;
}

/* Set result buffer */
int thactst_set_result_buff(double* val)
{
    vat_thactst.var_result_buff = val;
    return 0;
}
/* Start fan test */
int thactst_start(void)
{
    int i;
    /* check if sensor flags are set */
    if(!var_thactst.var_stsensor->var_okflg)
	{
	    printf("static sensor range not set\n");
	    return 1;
	}

    if(!var_thactst.var_tmpsensor->var_okflg)
	{
	    printf("temperature sensor not in range\n");
	    return 1;
	}

    if(!var_thactst.var_velocity->var_okflg)
	{
	    printf("velocity sensor not in range\n");
	    return 1;
	}
    /* create arrays */
    var_thactst.var_v0_arr = (double*) calloc(THACTST_SAMPLES_PERSECOND * THACTST_UPDATE_RATE, sizeof(double));
    var_thactst.var_v1_arr = (double*) calloc(THACTST_SAMPLES_PERSECOND * THACTST_UPDATE_RATE, sizeof(double));
    var_thactst.var_st_arr = (double*) calloc(THACTST_SAMPLES_PERSECOND * THACTST_UPDATE_RATE, sizeof(double));
    var_thactst.var_tmp_arr = (double*) calloc(THACTST_SAMPLES_PERSECOND * THACTST_UPDATE_RATE, sizeof(double));

    for(i=0; i<(THACTST_SAMPLES_PERSECOND * THACTST_UPDATE_RATE); i++)
	{
	    var_thactst.var_v0_arr[i] = 0.0;
	    var_thactst.var_v1_arr[i] = 0.0;
	    var_thactst.var_st_arr[i] = 0.0;
	    var_thactst.var_tmp_arr[i] = 0.0;
	}

    /* initialise thread attributes */
#if defined (WIN32) || defined (_WIN32)
    _thread = CreateThread(NULL,							/* default security attributes */
			   0,								/* use default stack size */
			   _thactst_async_start,					/* thread function */
			   NULL,							/* argument to thread function */
			   0,								/* default creation flag */
			   &var_thactst.var_thid);					/* thread id */
#endif

    return 0;
}

/* stop test */
int thactst_stop(void)
{
    int i;
    int32 _spl_write = 0;
    if(!var_thactst.var_stflg)
	return 0;
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_mutex, INFINITE);
#endif
    var_thactst.var_stflg = 0;
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(_mutex);
    /* Call to terminate thread */
    TerminateThread(_thread, 0);
    CloseHandle(_thread);
#endif

    /* free memory */
    free(var_thactst.var_v0_arr);
    free(var_thactst.var_v1_arr);
    free(var_thactst.var_st_arr);
    free(var_thactst.var_tmp_arr);

    /* stop actuator and fan */
#if defined (WIN32) || defined (_WIN32)
    var_thactst.var_out_v[1] = 0.0;
#endif
    i = 0;
    while(1)
	{
	    /* stop actuator */
	    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thactst.var_outtask,
					       1,
					       0,
					       10.0,
					       DAQmx_Val_GroupByChannel,
					       var_thactst.var_out_v,
					       &_spl_write,
					       NULL)))
		printf("Unable to stop fan\n");
	    if(i > THACTST_MAX_WAIT)
		break;
#if defined (WIN32) || defined (_WIN32)
	    Sleep(THACTST_SLEEP_TIME);
#endif
	    i++;
	    if(i > THACTST_MAX_WAIT)
		var_thactst.var_out_v[1] = THACTST_CLOSE_SOV;
	    printf("waiting for pressure decay..\r");
	}
    printf("\nactuator closed\n");
    /* stop running tasks */
    ERR_CHECK(NIStopTask(var_thactst.var_outtask));
    ERR_CHECK(NIStopTask(var_thactst.var_intask));

    printf("test stopped\n");
    return 0;
}

/* Close actuator */
int thactst_close_act(void)
{
    int i =0;
    int32 _spl_write;
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_mutex, INFINITE);
#endif
    var_thactst.var_out_v[1] = THACTST_CLOSE_SOV;
#if defined(WIN32) || defined (_WIN32)
    ReleaseMutex(_mutex);
#endif
    while(1)
	{
	    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thactst.var_outtask,
					       1,
					       0,
					       10.0,
					       DAQmx_Val_GroupByChannel,
					       var_thactst.var_out_v,
					       &_spl_write,
					       NULL)))
		printf("Unable to stop damper\n");
	    if(i>0)
		break;
#if defined (WIN32) || defined (_WIN32)
	    Sleep(THACTST_SLEEP_FAN_TIME);
	    WaitForSingleObject(_mutex, INFINITE);
#endif
	    var_thactst.var_out_v[0] = 0.0;
	    var_thactst.var_out_v[1] = 0.0;
#if defined (WIN32) || defined (_WIN32)
	    ReleaseMutex(_mutex);
#endif
	    i++;
	}
    return 0;
}

/*====================================================================================================*/
/****************************************** Private Methods *******************************************/

/* Clear Tasks */
static void _thactst_clear_tasks()
{
    ERR_CHECK(NIClearTask(var_thactst.var_outtask));
    ERR_CHECK(NIClearTask(var_thactst.var_intask));
    return;
}

/* assigning values to abstract snesors */
static void _thactst_set_value()
{
    /* wrap in mutex */
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_mutex, INFINITE);
#endif

    var_thactst.var_tmp_arr[_s_counter] = (_var_val_buff[THACTST_TMP_IX]>0? _var_val_buff[THACTST_TMP_IX] : 0.0);
    var_thactst.var_st_arr[_s_counter] = (_var_val_buff[THACTST_ST_IX]>0? _var_val_buff[THACTST_ST_IX] : 0.0);
    var_thactst.var_v0_arr[_s_counter] = (_var_val_buff[THACTST_DP1_IX]>0? _var_val_buff[THACTST_DP1_IX] : 0.0);
    var_thactst.var_v1_arr[_s_counter] = (_var_val_buff[THACTST_DP2_IX]>0? _var_val_buff[THACTST_DP2_IX] : 0.0);

    /* set sensor raw voltages */
    thgsens_add_value(var_thactst.var_tmpsensor,
		      Mean(var_thactst.var_tmp_arr,
			   (_max_flg? THACTST_SAMPLES_PERSECOND * THACTST_UPDATE_RATE : _s_counter)));
    thgsens_add_value(var_thactst.var_stsensor,
		      Mean(var_thactst.var_st_arr,
			   (_max_flg? THACTST_SAMPLES_PERSECOND * THACTST_UPDATE_RATE : _s_counter)));
    if(var_thactst.var_velocity->var_v1->var_flg)
	{
	    thgsens_add_value(var_thactst.var_velocity->var_v1,
			      Mean(var_thactst.var_v0_arr,
				   (_max_flg? THACTST_SAMPLES_PERSECOND * THACTST_UPDATE_RATE : _s_counter)));
	}

    if(var_thactst.var_velocity->var_v2->var_flg)
	{
	    thgsens_add_value(var_thactst.var_velocity->var_v2,
			      Mean(var_thactst.var_v1_arr,
				   (_max_flg? THACTST_SAMPLES_PERSECOND * THACTST_UPDATE_RATE : _s_counter)));
	}

    var_thactst.var_tmp = thgsens_get_value2(var_thactst.var_tmpsensor);
    var_thactst.var_st_pre = thgsens_get_value2(var_thactst.var_stsensor);
    var_thactst.var_v_duct = thvelsen_get_velocity(var_thactst.var_velocity);
    if(var_thactst.var_ductdia > 0.0 && var_thactst.var_width > 0.0 && var_thactst.var_height > 0.0)
	{
	    var_thactst.var_v_dmp = (var_thactst.var_v_duct * M_PI * pow(var_thactst.var_ductdia / 2,2)) /
		(var_thactst.var_width * var_thactst.var_height);
	    var_thactst.var_vol = (var_thactst.var_v_duct * M_PI * pow(var_thactst.var_ductdia / 2,2) / 1000000);
	}

    /* release mutex */
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(_mutex);
#endif
    return;
}

/* Write results */
static void _thactst_write_results()
{
    if(_g_counter == 0 && var_thactst.var_fp)
	fprintf(var_thactst.var_fp, "Item\tTMP\tST\tDP1\tDP2\tVol\tV(Dmp)\n");
    /* check if file pointer was assigned */
    if(var_thactst.var_fp)
	{
	    fprintf(var_thactst.var_fp, "%i\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n",
		    _g_counter,									/* counter */
		    var_thactst.var_tmp,							/* temperature */
		    var_thactst.var_st_pre,							/* static pressure */
		    thgsens_get_value2(var_thactst.var_velocity->var_v1),			/* DP1 */
		    thgsens_get_value2(var_thactst.var_velocity->var_v2),			/* DP2 */
		    var_thactst.var_vol,							/* Volume flow */
		    var_thactst.var_v_dmp);							/* Velocity Damper */
	}

    /* if result buffer was assigned */
    if(var_thactst.var_result_buff)
	{
	    var_thactst.var_result_buff[THACTST_TMP_IX]  = thgsens_get_value2(var_thactst.var_tmpsensor);
	    var_thactst.var_result_buff[THACTST_ST_IX] = thgsens_get_value2(var_thactst.var_stsensor);
	    var_thactst.var_result_buff[THACTST_DP1_IX] = thgsens_get_value2(var_thactst.var_velocity->var_v1);
	    var_thactst.var_result_buff[THACTST_DP2_IX] = thgsens_get_value2(var_thactst.var_velocity->var_v2);
	    var_thactst.var_result_buff[THACTST_VOL_IX] = var_thactst.var_vol;
	    var_thactst.var_result_buff[THACTST_V_IX] = var_thactst.var_v_dmp;
	}
    return;
}

#if defined (WIN32) || defined (_WIN32)
DWORD WINAPI _thactst_async_start(LPVOID obj)
#else
    void* _thactst_async_start(void* obj)
#endif
{
    int _counter;
    int32 _spl_write;
    _g_counter = 0;
    _max_flg = 0;
    var_thactst.var_stflg = 1;

    /* start both tasks */
    if(ERR_CHECK(NIStartTask(var_thactst.var_outtask)))
#if defined (WIN32) || defined (_WIN32)
	return FALSE;
#else
    return NULL;
#endif

    if(ERR_CHECK(NIStartTask(var_thactst.var_intask)))
	{
	    NIStopTask(var_thactst.var_outtask);
#if defined (WIN32) || defined (_WIN32)
	    return FALSE;
#else
	    return NULL;
#endif
	}

    /* actuator and fan control voltage */
    _counter = 0;
    var_thactst.var_out_v[0] = 0.0;
    var_thactst.var_out_v[1] = THACTST_OPEN_SOV;
    /* use software timing */
    while(var_thactst.var_stflg)
	{
	    /* write to out channel */
	    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thactst.var_outtask,
					       1,
					       0,
					       10.0,
					       DAQmx_Val_GroupByChannel,
					       var_thactst.var_out_v,
					       &_spl_write,
					       NULL)))
		break;
#if defined (WIN32) || defined (_WIN32)
	    Sleep(THACTST_SLEEP_TIME);
#endif
	    _g_counter++;
	    _thactst_write_results();

	    if(var_thactst.var_v_dmp > var_thactst.var_max_v)
		continue;

	    _counter++;
	    if(_counter > THACTST_ACT_RATE)
		_counter = 0;
	    var_thactst.var_out_v[0] = var_thactst.var_fan_ctrl_buff[_counter];
	    *var_thactst.var_ext_percen = 100 * var_thactst.var_fan_ctrl_buff[_counter] / 10;
	}

#if defined (WIN32) || defined (_WIN32)
	return FALSE;
#else
	return NULL;
#endif	
}

int32 CVICALLBACK _every_n_callback(TaskHandle taskHandle,
				 int32 everyNsamplesEventType,
				 uInt32 nSamples,
				 void *callbackData)
{
    int32 _spl_read = 0;		/* samples read */
    
    /* Read data into buffer */
    if(ERR_CHECK(NIReadAnalogF64(var_thactst.var_intask,
				 1,
				 10.0,
				 DAQmx_Val_GroupByScanNumber,
				 _var_val_buff,
				 THACTST_NUM_INPUT_CHANNELS,
				 &_spl_read,
				 NULL)))
	return 1;

    /* Call to set values */
    _thactst_set_value();
    if(++_s_counter >= (THACTST_SAMPLES_PERSECOND * THACTST_UPDATE_RATE))
	{
	    _s_counter = 0;
	    _max_flg = 1;
	}    
    return 0;
}

/* DoneCallback */
int32 CVICALLBACK _done_callback(TaskHandle taskHandle,
			       int32 status,
			       void *callbackData)
{
    /* Check if data accquisition was stopped
     * due to error */
    if(ERR_CHECK(status))
	return 0;

    return 0;
}

static int _thactst_actout_signal()
{
    int i;
    var_thactst.var_fan_ctrl_buff = (double*)
	calloc(THACTST_ACT_RATE, sizeof(double));
    for(i=0; i<THACTST_ACT_RATE; i++)
	{
	    var_thactst.var_fan_ctrl_buff[i] =
		9.98 * sin((double) i * M_PI / (2 * THACTST_ACT_RATE));
	}
    return 0;
}
