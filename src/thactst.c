/* Implementation of interfaces for damper slam shut test */

#if defined (WIN32) || defined (_WIN32)
#include <windows.h>
#endif

#define THACTST_NUM_OUT_CHANNELS 2				/* output channels */
#define THACTST_NUM_INPUT_CHANNELS 4				/* input channels */

#define THACTST_OPEN_SOV 5.3					/* open SOV voltage */
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
#define THACTST_SAMPLES_PERSECOND 8
#define THACTST_UPDATE_RATE 3
#define THACTST_MAX_VELOCITY 5.0

/* Object implementation */
struct _thactst
{
    TaskHandle var_outtask;		       			/* analog output task */
    TaskHandle var_intask;					/* analog input task */

    thgsens* var_stsensor;					/* static pressure sensor */
    thgsens* var_tmpsensor;					/* temperature sensor */
    thvelsen* var_velocity;					/* velocity sensors */

    double var_ductdia;						/* duct diameter */
    double var_width;						/* damper width */
    double var_height;						/* damper height */
    double var_max_v;						/* maximum velocity at damper */

    double var_st_pre;						/* static pressure */
    double var_v_duct;						/* velocity in duct */
    double var_v_dmp;						/* velocity damper */
    double var_out_v[THACTST_OUT_CHANNELS];			/* output voltage */
    double* var_result_buff;					/* result buffer */
    FILE* var_fp;						/* file pointer */
#if defined (WIN32) || defined (_WIN32)
    DWORD var_thid;						/* thread id */
#else
    int var_thid;
#endif
};

/* local variables */
static struct _thactst var_thactst;				/* object */
static double _var_st_x[] = THORNIFIX_ST_X;
static double _var_st_y[] = THORNIFIX_ST_Y;
#if defined (WIN32) || defined (_WIN32)
static HANDLE _mutex;
static HANDLE _thread;
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
#define THACTST_DELETE_ALL			\
    thgsens_delete(&var_thactst.var_tmpsensor);	\
    thgsens_delete(&var_thactst.var_stsensor);	\
    thvelsen_delete(&var_thactst.var_velocity);	\
    _thactst_clear_tasks()

/* Constructor */
int thactst_initialise(FILE* fp, thactst* obj, void* sobj)
{
    /* create output task */
    if(ERR_CHECK(NICreateTask("", &var_thactst.var_outtask)))
	return 1;

    /* create input task */
    if(ERR_CHECK(NIClearTask("", &var_thactst.var_intask)))
	{
	    _thactst_clear_tasks();
	    return 1;
	}

    /* intialise variables */
    var_thactst.var_stsensor = NULL;
    var_thactst.var_tmpsensor = NULL;
    var_thactst.var_velocity = NULL;

    var_thactst.var_ductdia = 0.0;
    var_thactst.var_width = 0.0;
    var_thactst.var_height = 0.0;
    var_thactst.var_max_v = THACTST_MAX_VELOCITY;

    var_thactst.var_st_pre = 0.0;
    var_thactst.var_v_duct = 0.0;
    var_thactst.var_out_v[0] = 0.0;
    var_thactst.var_out_v[1] = 0.0;
    var_thactst.var_result_buff = NULL;
    var_thactst.var_fp = NULL;

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

    printf("%s\n","configuring velocity sensors..");
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
    printf("initialisation complete..\n");

    /* enable all sensors by default */
    thvelsen_enable_sensor(var_thactst.var_velocity, 0);
    thvelsen_enable_sensor(var_thactst.var_velocity, 1);
    return 0;
}

/* stop all running tasks */
void thactst_delete(void)
{
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
