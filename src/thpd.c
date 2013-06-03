/* Implementation of pressure drop program */
#include "thornifix.h"
#include "thpd.h"
#include "thgsens.h"
#include "thvelsen.h"		/* velocity sensor */
#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#endif
#include <math.h>

#define THPD_NUM_OUT_CHANNELS 1					/* output channels */
#define THPD_NUM_INPUT_CHANNELS 4				/* input channels */

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

/* class implementation */
struct _thpd
{
    unsigned int var_int_flg;					/* internal flag */
    unsigned int var_gcount;					/* generic counter */
    unsigned int var_scount;					/* sample counter */

    /* Diff flag is set to on, by default. This shall read a differential
     * reading by each differential pressure sensor. If the flag was set to off,
     * each sensor shall read pressure against atmosphere. */
    unsigned int var_diff_flg;					/* diff reading flag */

    TaskHandle var_outtask;
    TaskHandle var_intask;

    FILE* var_fp;
    double var_static;
    double var_temp;
    double var_v1;
    double var_v2;
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
#else
    int var_thid;
#endif
};

/* local variables */
static unsigned int _init_flg = 0;			        /* initialise flag */
static unsigned int _g_counter = 0;				/* general counter */
static unsigned int _s_counter = 0;
static unsigned int _max_flg = 0;				/* maximum flag for controlling averaging buffers */
static unsigned int _set_flg = 0;				/* flag to indicate file headers added */
static struct _thpd var_thpid;					/* object */
static double _var_st_x[] = THORNIFIX_ST_X;
static double _var_st_y[] = THORNIFIX_ST_Y;
static double _var_val_buff[THACTST_NUM_INPUT_CHANNELS];	/* read buffer */
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

/* Constructor */
thpd* thpd_initialise(thpd* obj, void* sobj)
{
    /* create output task */
    fprintf(stderr, "Creating and initialising tasks\n");
    if(ERR_CHECK(NICreateTask("", &var_thpd.var_outtask)))
	return 1;

    /* create input tasks */
    if(ERR_CHECK(NIClearTask("", &var_thpd.var_intask)))
	{
	    _thpd_clear_tasks();
	    return 1;
	}

    /* initialising variables */
    fprintf(stderr, "Initialising internal variables...\n");
    var_thpd.var_int_flg = 0;
    var_thpd.var_gcount = 0;
    var_thpd.var_scount = 0;
    var_thpd.var_diff_flg = 0;

    var_thpd.var_fp = NULL;
    var_thpd.var_static = 0.0;
    var_thpd.var_temp = 0.0;
    var_thpd.var_v1 = 0.0;
    var_thpd.var_v2 = 0.0;
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
	    _thactst_clear_tasks();
	    return 1;
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
	    return 1;
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
	    return 1;
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
	    return 1;
	}

    fprintf(stderr, "Configuring velocity sensors...\n");
    thvelsen_config_sensor(var_thpd.var_v_sen,
			   0,
			   THPD_VEL_DP1_CHANNEL,
			   NULL,
			   THPD_MIN_RNG,
			   THPD_MAX_RNG);

    thvelsen_config_sensor(var_thpd.var_v_sen.
			   1,
			   THPD_VEL_DP2_CHANNEL,
			   NULL,
			   THPD_MIN_RNG,
			   THPD_MAX_RNG);

    fprintf(stderr "Velocity sensors configuration complete\nCreating DP sensors\n");
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
	    return 1;
	}

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
	    return 1;
	}

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
    	    return 1;
    	}

#if defined (WIN32) || defined (_WIN32)
    if(ERR_CHECK(NIRegisterEveryNSamplesEvent(var_thpd.var_intask,
    					      DAQmx_Val_Acquired_Into_Buffer,
    					      1,
    					      0,
    					      _every_n_callback,
    					      NULL)))
    	{
	    THACTST_DELETE_ALL;
    	    return 1;
    	}

    if(ERR_CHECK(NIRegisterDoneEvent(var_thpd.var_intask,
    				     0,
    				     _done_callback,
    				     NULL)))
    	{
	    THACTST_DELETE_ALL;
    	    return 1;
    	}
    _mutex = CreateMutex(NULL, FALSE, NULL);    
#endif

    /* enable all sensors by default */
    thvelsen_enable_sensor(var_thpd.var_v_sen, 0);
    thvelsen_enable_sensor(var_thpd.var_v_sen, 1);
    _init_flg = 1;

    fprintf(stderr, "Initialisation complete..\n");
    return 0;
}

/* Delete object */
void thpd_delete(void);
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
    CloseHandle(_mutex);
#endif
    printf("delete thor..\n");    
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
    return 0;
}

/* Duct diameter */
int thpd_set_duct_dia(double dia)
{
    var_thpd.var_ddia = dia;
    return 0;
}

/* Start program */
int thpd_start(void)
{
    int i;
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
    var_thpd.var_st_arr = (double**) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));
    var_thpd.var_v0_arr = (double**) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));
    var_thpd.var_v1_arr = (double**) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));
    var_thpd.var_p0_arr = (double**) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));
    var_thpd.var_p1_arr = (double**) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));
    var_thpd.var_tmp_arr = (double**) calloc(THPD_SAMPLES_PERSECOND * THPD_UPDATE_RATE, sizeof(double));

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

/*====================================================================================================*/
/****************************************** Private Methods *******************************************/

/* clear tasks */
static void _thpd_clear_tasks()					/* clear tasks */
{
    ERR_CHECK(NIClearTask(var_thpd.var_outtask));
    ERR_CHECK(NIClearTask(var_thpd.var_intask));
    return;
}
