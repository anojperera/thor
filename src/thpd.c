/* Implementation of pressure drop program */
#include "thornifix.h"
#include "thpd.h"
#include "thgsens.h"
#include "thvelsen.h"		/* velocity sensor */
#if !defined(WIN32) || !defined(_WIN32)
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#endif
#include <math.h>

#define THPD_TMP_IX 0
#define THPD_ST_IX 1
#define THPD_DP1_IX 2
#define THPD_DP2_IX 3
#define THPD_DP3_IX 4
#define THPD_DP4_IX 5

#define THPD_NUM_OUT_CHANNELS 1					/* output channels */
#define THPD_NUM_INPUT_CHANNELS 6				/* input channels */

#define THPD_FAN_CTRL_CHANNEL "Dev1/ao0"			/* fan control channel */

#define THPD_TMP_CHANNEL "Dev1/ai1"				/* temperature channel */
#define THPD_ST_CHANNEL "Dev1/ai2"				/* static pressure channel */
#define THPD_VEL_DP1_CHANNEL "Dev1/ai3"				/* dp pressure velocity */
#define THPD_VEL_DP2_CHANNEL "Dev1/ai4"				/* dp pressure velocity */
#define THPD_VEL_DP3_CHANNEL "Dev1/ai5"				/* dp pressure velocity */
#define THPD_VEL_DP4_CHANNEL "Dev1/ai6"				/* dp pressure velocity */

#define THPD_MIN_RNG 0.0					/* default minimum range for velocity probes */
#define THPD_MAX_RNG 1600.0					/* default maximum range for velocity probes */
#define THPD_MIN_ST_RNG 0.0
#define THPD_MAX_ST_RNG 5000.0
#define THPD_MIN_TMP_RNG -10.0
#define THPD_MAX_TMP_RNG 40.0
#define THPD_DUCT_DIA 300.0					/* default duct diameter */
#define THPD_BUFF_SZ 2048
#define THPD_ACT_RATE 1000					/* actuator rate */
#define THPD_SAMPLES_PERSECOND 8
#define THPD_UPDATE_RATE 3
#define THPD_SLEEP_TIME 1000

/* class implementation */
struct _thpd
{
    unsigned int var_int_flg;					/* internal flag */
    unsigned int var_gcount;					/* generic counter */
    unsigned int var_scount;					/* sample counter */
    unsigned int var_stflg;					/* start flag */

    /* Diff flag is set to on, by default. This shall read a differential
     * reading by each differential pressure sensor. If the flag was set to off,
     * each sensor shall read pressure against atmosphere. */
    unsigned int var_diff_flg;					/* diff reading flag */

    TaskHandle var_outtask;
    TaskHandle var_intask;

    FILE* var_fp;
    double var_static;
    double var_temp;
    double var_v0;				/* duct velocity */
    double var_v1;				/* damper velocity */
    double var_p1;
    double var_p2;
    double var_air_flow;

    /* result buffers */
    double* var_st_arr;
    double* var_v0_arr;
    double* var_v1_arr;
    double* var_p0_arr;
    double* var_p1_arr;
    double* var_tmp_arr;

    /* duct diameter and damper dimensions */
    double var_ddia;
    double var_dwidth;
    double var_dheight;
    double var_darea;
    double var_farea;

    /* result buffer */
    double* var_result_buff;
    double* var_fan_ctrl_buff;
    double var_out_v[THPD_NUM_OUT_CHANNELS];
    
    /* Settling and reading times are control timers.
     * Readin time is used reading number of samples
     * and use moving average to smooth it */
    double var_settle;
    double var_read_time;

    /*Sensor objects */
    thgsens* var_st_sen;
    thgsens* var_tmp_sen;
    thvelsen* var_v_sen;
    thgsens* var_p1_sen;
    thgsens* var_p2_sen;

    void* var_sobj;						/* external objects */

#if defined (WIN32) || defined (_WIN32)
    DWORD var_thid;						/* thread id */
    HANDLE var_mutex;						/* mutex for protecting result buffer */
#else
    int var_thid;
#endif
    int (*callback_complete)(void*) _complete;
};

/* local variables */
static unsigned int _init_flg = 0;			        /* initialise flag */
static unsigned int _g_counter = 0;				/* general counter */
static unsigned int _s_counter = 0;
static unsigned int _max_flg = 0;				/* maximum flag for controlling averaging buffers */
static unsigned int _set_flg = 0;				/* flag to indicate file headers added */
static struct _thpd var_thpd;					/* object */
static double _var_st_x[] = THORNIFIX_ST_X;
static double _var_st_y[] = THORNIFIX_ST_Y;
static double _var_p1_x[] = THORNIFIX_S3_X;
static double _var_p1_y[] = THORNIFIX_S3_Y;
static double _var_p2_x[] = THORNIFIX_S4_X;
static double _var_p2_y[] = THORNIFIX_S4_Y;
static double _var_val_buff[THPD_NUM_INPUT_CHANNELS];		/* read buffer */
#if defined (WIN32) || defined (_WIN32)
static HANDLE _mutex = NULL;
static HANDLE _thread = NULL;
#endif

/* Private functions */
static void _thpd_clear_tasks();				/* clear tasks */
static void _thpd_set_value();
static void _thpd_write_results();
#if defined (WIN32) || defined (_WIN32)
DWORD WINAPI _thpd_async_start(LPVOID obj);
#else
void* _thpd_async_start(void* obj);
#endif
static int32 CVICALLBACK _every_n_callback(TaskHandle taskHandle,
					   int32 everyNsamplesEventType,
					   uInt32 nSamples,
					   void *callbackData);

static int32 CVICALLBACK _done_callback(TaskHandle taskHandle,
					int32 status,
					void *callbackData);
static int _thpd_actout_signal();

#define THPD_DELETE_ALL				\
    thgsens_delete(&var_thpd.var_tmp_sen);	\
    thgsens_delete(&var_thpd.var_st_sen);	\
    thvelsen_delete(&var_thpd.var_v_sen);	\
    thgsens_delete(&var_thpd.var_p1_sen);	\
    thgsens_delete(&var_thpd.var_p2_sen);	\
    _thpd_clear_tasks()	        

/* macro for checking the value */
#define THPD_CHK_VAL(val)			\
    (val>0.0? val : 0.0)

/* Max value for array */
#define THPD_MAX_MEAN \
    (_max_flg? (THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE) : _s_counter)

/* Constructor */
thpd* thpd_initialise(void* sobj)
{
    /* create output task */
    fprintf(stderr, "Creating and initialising tasks\n");
    if(ERR_CHECK(NICreateTask("", &var_thpd.var_outtask)))
	return NULL;

    /* create input tasks */
    if(ERR_CHECK(NICreateTask("", &var_thpd.var_intask)))
	{
	    _thpd_clear_tasks();
	    return NULL;
	}

    /* initialising variables */
    fprintf(stderr, "Initialising internal variables...\n");
    var_thpd.var_int_flg = 0;
    var_thpd.var_gcount = 0;
    var_thpd.var_scount = 0;
    var_thpd.var_diff_flg = 0;
    var_thpd.var_stflg = 0;

    var_thpd.var_fp = NULL;
    var_thpd.var_static = 0.0;
    var_thpd.var_temp = 0.0;
    var_thpd.var_v0 = 0.0;
    var_thpd.var_v1 = 0.0;
    var_thpd.var_p1 = 0.0;
    var_thpd.var_p2 = 0.0;
    var_thpd.var_air_flow = 0.0;

    var_thpd.var_ddia = 0.0;
    var_thpd.var_dwidth = 0.0;
    var_thpd.var_dheight = 0.0;
    var_thpd.var_darea = 0.0;
    var_thpd.var_farea = 0.0;

    var_thpd.var_settle = 0.0;
    var_thpd.var_read_time = 0.0;

    var_thpd.var_st_sen = NULL;
    var_thpd.var_tmp_sen = NULL;
    var_thpd.var_v_sen = NULL;
    var_thpd.var_fan_ctrl_buff = NULL;
    var_thpd.var_out_v[0] = 0.0;
    var_thpd.var_p1_sen = NULL;
    var_thpd.var_p2_sen = NULL;

    var_thpd.var_sobj = sobj;

    fprintf(stderr, "Creating fan control signal\n");
    _thpd_actout_signal();

    /* create fan control channel */
    if(ERR_CHECK(NICreateAOVoltageChan(var_thpd.var_outtask,
				       THPD_FAN_CTRL_CHANNEL,
				       "",
				       0.0,
				       10.0,
       				       DAQmx_Val_Volts,
				       NULL)))
	{
	    fprintf(stderr, "create output channel failed!\n");
	    _thpd_clear_tasks();
	    return NULL;
	}
    fprintf(stderr, "Output channels configure complete..\n");

    /* create temperature sensor */
    if(!thgsens_new(&var_thpd.var_tmp_sen,
		    THPD_TMP_CHANNEL,
		    &var_thpd.var_intask,
		    NULL,
		    sobj))
	{
	    fprintf(stderr, "Error creating temperature sensor\n");
	    _thpd_clear_tasks();
	    return NULL;
	}
    thgsens_set_range(var_thpd.var_tmp_sen, THPD_MIN_TMP_RNG, THPD_MAX_TMP_RNG);

    /* create static pressure */
    if(!thgsens_new(&var_thpd.var_st_sen,
		    THPD_ST_CHANNEL,
		    &var_thpd.var_intask,
		    NULL,
		    sobj))
	{
	    fprintf(stderr, "Error creating static sensor\n");
	    thgsens_delete(&var_thpd.var_tmp_sen);
	    _thpd_clear_tasks();
	    return NULL;
	}
    thgsens_set_range(var_thpd.var_st_sen, THPD_MIN_ST_RNG, THPD_MAX_ST_RNG);
    thgsens_set_calibration_buffers(var_thpd.var_st_sen, _var_st_x, _var_st_y, THORNIFIX_S_CAL_SZ);

    /* create velocity sensors */
    if(!thvelsen_new(&var_thpd.var_v_sen,
		     &var_thpd.var_intask,
		     NULL,
		     sobj))
	{
	    fprintf(stderr, "Error creating velocity sensor\n");
	    thgsens_delete(&var_thpd.var_tmp_sen);
	    thgsens_delete(&var_thpd.var_st_sen);
	    _thpd_clear_tasks();
	    return NULL;
	}

    fprintf(stderr, "Configuring velocity sensors...\n");
    thvelsen_config_sensor(var_thpd.var_v_sen,
			   0,
			   THPD_VEL_DP1_CHANNEL,
			   NULL,
			   THPD_MIN_RNG,
			   THPD_MAX_RNG);

    thvelsen_config_sensor(var_thpd.var_v_sen,
			   1,
			   THPD_VEL_DP2_CHANNEL,
			   NULL,
			   THPD_MIN_RNG,
			   THPD_MAX_RNG);

    fprintf(stderr, "Velocity sensors configuration complete\nCreating DP sensors\n");
    /* Create HP static sensor */
    if(!thgsens_new(&var_thpd.var_p1_sen,
		    THPD_VEL_DP3_CHANNEL,
		    &var_thpd.var_intask,
		    NULL,
		    sobj))
	{
	    thgsens_delete(&var_thpd.var_tmp_sen);
	    thgsens_delete(&var_thpd.var_st_sen);
	    thvelsen_delete(&var_thpd.var_v_sen);
	    _thpd_clear_tasks();
	    return NULL;
	}
    thgsens_set_calibration_buffers(var_thpd.var_p1_sen, _var_p1_x, _var_p1_y, THORNIFIX_S_CAL_SZ);
    thgsens_set_range(var_thpd.var_p1_sen, THPD_MIN_RNG, THPD_MAX_RNG);
    
    /* Create LP static sensor */
    if(!thgsens_new(&var_thpd.var_p2_sen,
		    THPD_VEL_DP4_CHANNEL,
		    &var_thpd.var_intask,
		    NULL,
		    sobj))
	{
	    thgsens_delete(&var_thpd.var_tmp_sen);
	    thgsens_delete(&var_thpd.var_st_sen);
	    thvelsen_delete(&var_thpd.var_v_sen);
	    thgsens_delete(&var_thpd.var_p1_sen);
	    _thpd_clear_tasks();
	    return NULL;
	}
    thgsens_set_calibration_buffers(var_thpd.var_p2_sen, _var_p2_x, _var_p2_y, THORNIFIX_S_CAL_SZ);
    thgsens_set_range(var_thpd.var_p2_sen, THPD_MIN_RNG, THPD_MAX_RNG);
    
    fprintf(stderr, "All sensors configured\nConfiguring timer..\n");
    /* Configure timing */
    if(ERR_CHECK(NICfgSampClkTiming(var_thpd.var_intask,
				    "OnboardClock",
				    THPD_SAMPLES_PERSECOND,
				    DAQmx_Val_Rising,
				    DAQmx_Val_ContSamps,
				    1)))
    	{
	    THPD_DELETE_ALL;
    	    return NULL;
    	}

#if defined (WIN32) || defined (_WIN32)
    if(ERR_CHECK(NIRegisterEveryNSamplesEvent(var_thpd.var_intask,
    					      DAQmx_Val_Acquired_Into_Buffer,
    					      1,
    					      0,
    					      _every_n_callback,
    					      NULL)))
    	{
	    THPD_DELETE_ALL;
    	    return NULL;
    	}

    if(ERR_CHECK(NIRegisterDoneEvent(var_thpd.var_intask,
    				     0,
    				     _done_callback,
    				     NULL)))
    	{
	    THPD_DELETE_ALL;
    	    return NULL;
    	}
    _mutex = CreateMutex(NULL, FALSE, NULL);
    var_thpd.var_mutex = NULL;
#endif

    /* enable all sensors by default */
    thvelsen_enable_sensor(var_thpd.var_v_sen, 0);
    thvelsen_enable_sensor(var_thpd.var_v_sen, 1);
    _init_flg = 1;

    fprintf(stderr, "Initialisation complete..\n");
    return &var_thpd;
}

/* Delete object */
void thpd_delete(void)
{
    if(_init_flg == 0)
	return;

    _init_flg = 0;
    THPD_DELETE_ALL;
    _thpd_clear_tasks();
    
    /* clean up */
    var_thpd.var_st_sen = NULL;
    var_thpd.var_tmp_sen = NULL;
    var_thpd.var_v_sen = NULL;
    var_thpd.var_p1_sen = NULL;
    var_thpd.var_p2_sen = NULL;
    var_thpd.var_sobj = NULL;

    if(var_thpd.var_fan_ctrl_buff != NULL)
	free(var_thpd.var_fan_ctrl_buff);
    var_thpd.var_fan_ctrl_buff = NULL;
#if defined (WIN32) || defined (_WIN32)
    var_thpd.var_mutex = NULL;
    CloseHandle(_mutex);
#endif
    printf("\ndelete thor..\n");    
}

/* set settling time */
int thpd_set_settle_time(double stime)
{
    var_thpd.var_settle = stime;
    return 0;
}

/* set reading time */
int thpd_set_read_time(double rtime)
{
    var_thpd.var_read_time = rtime;
    return 0;
}

/* set buffer */
int thpd_set_buffer(double* buff)
{
    var_thpd.var_result_buff = buff;
    return 0;
}

/* Damper size */
int thpd_set_damper_size(double width, double height)
{
    var_thpd.var_dwidth = width;
    var_thpd.var_dheight = height;
    var_thpd.var_farea = (var_thpd.var_dwidth * var_thpd.var_dheight) / 1000000;
    return 0;
}

/* Duct diameter */
int thpd_set_duct_dia(double dia)
{
    var_thpd.var_ddia = dia;
    var_thpd.var_darea = (M_PI * pow((var_thpd.var_ddia/2), 2)) / 1000000;
    return 0;
}

/* Start program */
int thpd_start(void)
{
    int i;
    /* Test is running so return function */
    if(var_thpd.var_stflg > 1)
	return 0;
    if(_init_flg == 0)
	{
	    fprintf(stderr, "Object not initialised\n");
	    return 1;
	}
    if(!var_thpd.var_st_sen->var_okflg)
	{
	    fprintf(stderr, "Static sensor not in range\n");
	    return 1;
	}

    if(!var_thpd.var_tmp_sen->var_okflg)
	{
	    fprintf(stderr, "Temperature sensor not in range\n");
	    return 1;
	}

    if(!var_thpd.var_v_sen->var_okflg)
	{
	    fprintf(stderr, "Velocity sensors not in range\n");
	    return 1;
	}

    /* create moving average buffers */
    var_thpd.var_st_arr = (double*) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));
    var_thpd.var_v0_arr = (double*) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));
    var_thpd.var_v1_arr = (double*) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));
    var_thpd.var_p0_arr = (double*) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));
    var_thpd.var_p1_arr = (double*) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));
    var_thpd.var_tmp_arr = (double*) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));

    /* initialise buffers */
    for(i=0; i<THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE; i++)
	{
	    var_thpd.var_st_arr[i] = 0.0;
	    var_thpd.var_v0_arr[i] = 0.0;
	    var_thpd.var_v1_arr[i] = 0.0;
	    var_thpd.var_p0_arr[i] = 0.0;
	    var_thpd.var_p1_arr[i] = 0.0;
	    var_thpd.var_tmp_arr[i] = 0.0;
	}

#if defined (WIN32) || defined (_WIN32)
    _thread = CreateThread(NULL,							/* default security attributes */
			   0,								/* use default stack size */
			   _thpd_async_start,						/* thread function */
			   NULL,							/* argument to thread function */
			   0,								/* default creation flag */
			   &var_thpd.var_thid);						/* thread id */
#endif

    return 0;
}


/* Stop test */
int thpd_stop(void)
{
    int32 _spl_write = 0;
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_mutex, INFINITE);
    if(!var_thpd.var_stflg)
	{
	    ReleaseMutex(_mutex);
	    return 0;
	}
#endif
    var_thpd.var_stflg = 0;
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(_mutex);
    /* Call to terminate thread */
    TerminateThread(_thread, 0);
    CloseHandle(_thread);
#endif

    /* stop fan */
    var_thpd.var_out_v[0] = 0.0;

    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thpd.var_outtask,
				       1,
				       0,
				       10.0,
				       DAQmx_Val_GroupByChannel,
				       var_thpd.var_out_v,
				       &_spl_write,
				       NULL)))
	fprintf(stderr, "Unable to stop fan\n");    
    /* stop running tasks */
    ERR_CHECK(NIStopTask(var_thpd.var_outtask));
    ERR_CHECK(NIStopTask(var_thpd.var_intask));

    free(var_thpd.var_st_arr);
    free(var_thpd.var_v0_arr);
    free(var_thpd.var_v1_arr);
    free(var_thpd.var_p0_arr);
    free(var_thpd.var_p1_arr);
    free(var_thpd.var_tmp_arr);
    return 0;
}

/* File pointer */
int thpd_set_file_pointer(FILE* fp)
{
    var_thpd.var_fp = fp;
    return 0;
}

/* Set mutex for result buffer */
int thpd_set_mutex(HANDLE mutex)
{
    var_thpd.var_mutex = mutex;
    return 0;
}
/*====================================================================================================*/
/****************************************** Private Methods *******************************************/

/* clear tasks */
static void _thpd_clear_tasks()					/* clear tasks */
{
    ERR_CHECK(NIClearTask(var_thpd.var_outtask));
    ERR_CHECK(NIClearTask(var_thpd.var_intask));
    return;
}

/* Set values */
static void _thpd_set_value()
{
    /* wrap in mutex */
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_mutex, INFINITE);
#endif
    var_thpd.var_tmp_arr[_s_counter] = THPD_CHK_VAL(_var_val_buff[THPD_TMP_IX]);
    var_thpd.var_st_arr[_s_counter] = THPD_CHK_VAL(_var_val_buff[THPD_ST_IX]);
    var_thpd.var_v0_arr[_s_counter] = THPD_CHK_VAL(_var_val_buff[THPD_DP1_IX]);
    var_thpd.var_v1_arr[_s_counter] = THPD_CHK_VAL(_var_val_buff[THPD_DP2_IX]);
    var_thpd.var_p0_arr[_s_counter] = THPD_CHK_VAL(_var_val_buff[THPD_DP3_IX]);
    var_thpd.var_p1_arr[_s_counter] = THPD_CHK_VAL(_var_val_buff[THPD_DP4_IX]);

    thgsens_add_value(var_thpd.var_tmp_sen, Mean(var_thpd.var_tmp_arr, THPD_MAX_MEAN));
    thgsens_add_value(var_thpd.var_st_sen, Mean(var_thpd.var_st_arr, THPD_MAX_MEAN));
    if(var_thpd.var_v_sen->var_v1->var_flg > 0)
	thgsens_add_value(var_thpd.var_v_sen->var_v1, Mean(var_thpd.var_v0_arr, THPD_MAX_MEAN));
    if(var_thpd.var_v_sen->var_v2->var_flg > 0)
	thgsens_add_value(var_thpd.var_v_sen->var_v2, Mean(var_thpd.var_v1_arr, THPD_MAX_MEAN));
    thgsens_add_value(var_thpd.var_p1_sen, Mean(var_thpd.var_p0_arr, THPD_MAX_MEAN));
    thgsens_add_value(var_thpd.var_p2_sen, Mean(var_thpd.var_p1_arr, THPD_MAX_MEAN));

    /* Get values */
    var_thpd.var_temp = thgsens_get_value2(var_thpd.var_tmp_sen);
    var_thpd.var_static = thgsens_get_value2(var_thpd.var_st_sen);
    var_thpd.var_v0 = thvelsen_get_velocity(var_thpd.var_v_sen);
    
    var_thpd.var_air_flow = var_thpd.var_v0 * var_thpd.var_darea;
    /* check if damper area was set if not set velocity at target 
     * same as duct velocity */
    if(var_thpd.var_farea > 0.0)
	var_thpd.var_v1 = var_thpd.var_air_flow / var_thpd.var_farea;
    else
	var_thpd.var_v1 = var_thpd.var_v0;
    var_thpd.var_p1 = thgsens_get_value2(var_thpd.var_p1_sen);
    var_thpd.var_p2 = thgsens_get_value2(var_thpd.var_p2_sen);

#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(_mutex);
#endif
    return;
    
}

/* Write values to buffer */
static void _thpd_write_results()
{
#if defined (WIN32) || defined (_WIN32)
    if(var_thpd.var_mutex != NULL)
	WaitForSingleObject(var_thpd.var_mutex, INFINITE);
#endif    
    /* if file pointer was set write results to file */
    /* IX	V_DP1	V_DP2	DP1	DP2	V	VOL	DP	TMP */
    if(var_thpd.var_fp)
	{
	    fprintf(var_thpd.var_fp, "%i\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n",
		    _g_counter,
		    thgsens_get_value2(var_thpd.var_v_sen->var_v1),
		    thgsens_get_value2(var_thpd.var_v_sen->var_v2),
		    var_thpd.var_p1,
		    var_thpd.var_p2,
		    var_thpd.var_v0,
		    var_thpd.var_air_flow,
		    var_thpd.var_p1 - var_thpd.var_p2,
		    var_thpd.var_temp);
	}

    if(var_thpd.var_result_buff)
	{
	    var_thpd.var_result_buff[THPD_RESULT_VDP1_IX] = thgsens_get_value2(var_thpd.var_v_sen->var_v1);
	    var_thpd.var_result_buff[THPD_RESULT_VDP2_IX] = thgsens_get_value2(var_thpd.var_v_sen->var_v2);
	    var_thpd.var_result_buff[THPD_RESULT_DP1_IX] = thgsens_get_value2(var_thpd.var_p1_sen);
	    var_thpd.var_result_buff[THPD_RESULT_DP2_IX] = thgsens_get_value2(var_thpd.var_p2_sen);
	    var_thpd.var_result_buff[THPD_RESULT_VEL_IX] = thvelsen_get_velocity(var_thpd.var_v_sen);
	    var_thpd.var_result_buff[THPD_RESULT_VOL_IX] = var_thpd.var_air_flow;
	    var_thpd.var_result_buff[THPD_RESULT_DP_IX] = var_thpd.var_result_buff[THPD_RESULT_DP1_IX] -
		var_thpd.var_result_buff[THPD_RESULT_DP2_IX];
	    var_thpd.var_result_buff[THPD_RESULT_TMP_IX] = thgsens_get_value2(var_thpd.var_tmp_sen);

	}
#if defined (WIN32) || defined (_WIN32)
    if(var_thpd.var_mutex != NULL)
	ReleaseMutex(var_thpd.var_mutex);
#endif    
}

/* Async start function */
#if defined (WIN32) || defined (_WIN32)
DWORD WINAPI _thpd_async_start(LPVOID obj)
#else
void* _thpd_async_start(void* obj)
#endif
{
    int _counter;
    int32 _spl_write;
    _g_counter = 0;
    _max_flg = 0;
    var_thpd.var_stflg = 1;
    _set_flg = 0;

    /* start both tasks */
    if(ERR_CHECK(NIStartTask(var_thpd.var_outtask)))
#if defined (WIN32) || defined (_WIN32)
	return FALSE;
#else
    return NULL;
#endif

    if(ERR_CHECK(NIStartTask(var_thpd.var_intask)))
	{
	    NIStopTask(var_thpd.var_outtask);
#if defined (WIN32) || defined (_WIN32)
	    return FALSE;
#else
	    return NULL;
#endif
	}

    /* use software timing */
    _counter = 0;
    var_thpd.var_out_v[0] = 0.0;
    while(1)
	{
#if defined (WIN32) || defined (_WIN32)
	    WaitForSingleObject(_mutex, INFINITE);
	    if(var_thpd.var_stflg == 0)
		{
		    ReleaseMutex(_mutex);
		    break;
		}
	    ReleaseMutex(_mutex);
#endif
	    /* write to out channel */
	    if(ERR_CHECK(NIWriteAnalogArrayF64(var_thpd.var_outtask,
					       1,
					       0,
					       10.0,
					       DAQmx_Val_GroupByChannel,
					       var_thpd.var_out_v,
					       &_spl_write,
					       NULL)))
		break;

#if defined (WIN32) || defined (_WIN32)
	    Sleep(THPD_SLEEP_TIME);
#endif
	    if(++_counter < THPD_ACT_RATE)
		{
		    var_thpd.var_out_v[0] = var_thpd.var_fan_ctrl_buff[_counter];
		    _thpd_write_results();
		    
		}
	    _g_counter++;
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
    if(ERR_CHECK(NIReadAnalogF64(var_thpd.var_intask,
				 1,
				 10.0,
				 DAQmx_Val_GroupByScanNumber,
				 _var_val_buff,
				 THPD_NUM_INPUT_CHANNELS,
				 &_spl_read,
				 NULL)))
	return 1;

    /* Call to set values */
    _thpd_set_value();
    if(++_s_counter >= (THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE))
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


/* Generate signals for fan control */
static int _thpd_actout_signal()
{
    int i;
    var_thpd.var_fan_ctrl_buff = (double*)
	calloc(THPD_ACT_RATE, sizeof(double));
    for(i=0; i<THPD_ACT_RATE; i++)
	{
	    var_thpd.var_fan_ctrl_buff[i] =
		9.98 * sin((double) i * M_PI / (2 * THPD_ACT_RATE));
	}
    return 0;
}    
