/* Implementation of pressure drop program */
#include <windows.h>
#include <math.h>
#include "thpd.h"
#include "thgsens.h"

/* sensor channels */
#define THPD_TEMP "Dev1/ai0"			/* temperature sensor */
#define THPD_ST "Dev1/ai1"			/* static pressure in duct */
#define THPD_DP1 "Dev1/ai2"			/* diff pressure high */
#define THPD_DP2 "Dev1/ai3"			/* diff pressure low */
#define THPD_VSEN1 "Dev1/ai4"			/* velocity differential reading */
#define THPD_VSEN2 "Dev1/ai5"			/* velocity differential reading */
 
#define THPD_MAX_VRANGE 1600.0			/* max range for velocity sensors */
#define THPD_MAX_SRANGE 5000.0			/* max ststic lressure range */
#define THPD_MAX_TRANGE 40.0			/* maximum temperature range */
#define THPD_MIN_TRANGE -10.0			/* minimum temperature range */
 
/* fan control channel */
#define THPD_FAN "Dev1/ao0"			/* fan control singal */
 
#define THPD_BUFF_SZ 2048
 
#define THPD_SETTLE_TIME 5.0			/* settle time before reading */
#define THPD_READ_TIME 5.0			/* read time after settling */

/* macro for deleting the sensors */
#define THPD_SENSOR_DELETE			\
    thgsens_delete(&var_thpd.var_st_sen);	\
    thgsens_delete(&var_thpd.var_tmp_sen);	\
    thgsens_delete(&var_thpd.var_v1_sen);	\
    thgsens_delete(&var_thpd.var_v2_sen);	\
    thgsens_delete(&var_thpd.var_p1_sen);	\
    thgsens_delete(&var_thpd.var_p2_sen)
 
 
/* class implementation */
struct	_thpd
{
    unsigned int var_int_flag;							/* internal flag */
    unsigned int var_gcount;							/* generic counter */
    unsigned int var_scount;							/* sample counter */
 	
    /* Diff flag is set to on, by default. This shall read
     * a differential reading by each differential pressure
     * sensor. If the flag was set to off, each sensor shall
     * read pressure against atmosphere. */
    unsigned int var_diff_flg;							/* diff reading flag */
 	
    TaskHandle var_outtask;
    TaskHandle var_intask;
 	
    FILE* var_fp;								/* file pointer */
    double var_static;
    double var_temp;
    double var_v1;
    double var_v2;
    double var_p1;
    double var_p2;
    double var_air_flow;							/* air flow */
 	
    /* duct and damper dimensions */
    double var_ddia;								/* duct diameter */
    double var_dwidth;								/* damper width */
    double var_dheight;								/* damper height */
    double var_darea;								/* duct area */
    double var_farea;								/* damper face area */
 	
    /* settling amd reading time are control timers. 
       Reading time is used reading number of samples
       and use moving average to smooth it. */
    double var_settle;								/* settle down period */
    double var_read_time;							/* reading time */
 	
    /* function pointer for updating external */
    gthor_fptr var_update_dp;							/* update differential pressure */
    ghtor_fptr var_update_velocity;						/* update velocity */
    gthor_fptr var_update_static;						/* update static pressure */
    ghtor_fptr var_update_temp;							/* update temperature */
 	
    /* sensor objects */
    thgsens* var_st_sen;							/* static sensor */
    thgsens* var_tmp_sen;							/* temperature sensor */
    thgsens* var_v1_sen;							/* velocity sensor */
    thgsens* var_v2_sen;							/* velocity sensor */
    thgsens* var_p1_sen;							/* pressure sensor 1 */
    thgsens* var_p2_sen;							/* pressure sensor 2 */
 	
    void* var_sobj;								/* external object to be passed to function pointer */
};
 
static thpd var_thpd;								/* internal variable */
static char thpd_err_mag[THPD_ERR_MSG];						/* error message buffer */
 
/* private callback functions for timing */
static int32 CVICALLBACK EveryNCallback(TaskHandle taskHandle,
					int32 everyNSampleEvent,
					uInt32 nSamples,
					void* callbackdata);
static int32 CVICALLBACK DoneCallback(TaskHandle taskHandle,
				      int32 status,
				      void* callbackdata);
 										
static int _thpd_clear_tasks(void);						/* clear tasks */
DWORD WINAPI _thpd_async_start(LPVOID obj);					/* asynchronous start */
 
/* constructor */
thpd* thpd_initialise(void)
{
    int32 _err_code;
    /* initialise variables */
    var_thpd.var_int_flag = 0;
    var_thpd.var_gcount = 0;
    var_thpd.var_scount = 0;
    var_thpd.var_diff_flag = 1;
 	
    /* create in and out tasks */
    _err_code = NICreateTask("", &var_thpd.var_outtask);
    if (_err_code)
 	{
	    NIGetErrorString(_err_code, thpd_err_msg, THPD_BUFF_SZ);
	    fprintf(stderr, "%s\n", thpd_err_msg);
	    return NULL;
 	}
 	
    _err_code = NICreateTask("", &var_thpd.var_intask);
    if (_err_code)
 	{
	    NIGetErrorString(_err_code, thpd_err_msg, THPD_BUFF_SZ);
	    fprintf(stderr, "%s\n", thpd_err_msg);
	    ERR_CHECK(NIClearTasks(&var_thpd.var_outtask));
	    return NULL;
 	}
 	
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
 	
    /* initialise function pointers */
    var_thpd.var_update_dp = NULL;
    var_thpd.var_update_velocity = NULL;
    var_thpd.var_update_static = NULL;
    var_thpd.var_update_temp = NULL;

    /* initialise the sensors to NULL first for deletion */
    var_thpd.var_st_sen = NULL;
    var_thpd.var_tmp_sen = NULL;
    var_thpd.var_v1_sen = NULL;
    var_thpd.var_v2_sen = NULL;
    var_thpd.var_p1_sen = NULL;
    var_thpd.var_p2_sen = NULL;
 	
    /* create sensors */
    /* create temperature sensor */
    if(!thgsens_new(&var_thpd.var_tmp_sen,
		    THPD_TEMP,
		    &var_thpd.var_intask,
		    NULL,
		    NULL))
 	{
	    fprintf(stderr, "%s\n", "unable to create temperature sensor");
	    _thpd_clear_tasks(void);
	    return NULL;
 	}
 	
    /* create static sensor */
    if(!thgsens_new(&var_thpd.var_st_sen,
		    THPD_ST,
		    &var_thpd.var_intask,
		    NULL,
		    NULL))
 	{
	    fprintf(stderr, "%s\n", "unable to create static sensor");
	    _thpd_clear_tasks(void);
	    THPD_SENSOR_DELETE;
	    return NULL;
 	}
 	
    /* create dp1 sensor */
    if(!thgsens_new(&var_thpd.var_p1_sen,
		    THPD_DP1,
		    &var_thpd.var_intask,
		    NULL,
		    NULL))
 	{
	    fprintf(stderr, "%s\n", "unable to create dp1 sensor");
	    _thpd_clear_tasks(void);
	    THPD_SENSOR_DELETE;
	    return NULL;
 	}

    /* create dp2 sensor */
    if(!thgsens_new(&var_thpd.var_p2_sen,
		    THPD_DP2,
		    &var_thpd.var_intask,
		    NULL,
		    NULL))
 	{
	    fprintf(stderr, "%s\n", "unable to create dp2 sensor");
	    _thpd_clear_tasks(void);
	    THPD_SENSOR_DELETE;
	    return NULL;
 	}

    /* create velocity sensor */
    if(!thgsens_new(&var_thpd.var_v1_sen,
		    THPD_VSEN1,
		    &var_thpd.var_intask,
		    NULL,
		    NULL))
 	{
	    fprintf(stderr, "%s\n", "unable to create velocity 1 sensor");
	    _thpd_clear_tasks(void);
	    THPD_SENSOR_DELETE;
	    return NULL;
 	}

    if(!thgsens_new(&var_thpd.var_v2_sen,
		    THPD_VSEN2,
		    &var_thpd.var_intask,
		    NULL,
		    NULL))
 	{
	    fprintf(stderr, "%s\n", "unable to create velocity 2 sensor");
	    _thpd_clear_tasks(void);
	    THPD_SENSOR_DELETE;
	    return NULL;
 	}

    /* set range of sensors */
    thgsens_set_range(var_thpd.var_st_sen, 0.0, THPD_MAX_SRANGE);
    thgsens_set_range(var_thpd.var_tmp_sen, THPD_MIN_TRANGE, THPD_MAX_TRANGE);
    thgsens_set_range(var_thpd.var_v1_sen, 0.0, THPD_MAX_VRANGE);
    thgsens_set_range(var_thpd.var_v2_sen, 0.0, THPD_MAX_VRANGE);
    thgsens_set_range(var_thpd.var_p1_sen, 0.0, THPD_MAX_VRANGE);
    thgsens_set_range(var_thpd.var_p2_sen, 0.0, THPD_MAX_VRANGE);

    return &var_thpd;
}

/* destructor */
void thpd_delete(void)
{
    /* clear tasks */
    _thpd_clear_tasks(void);

    /* delete sensors */
    thgsens_delete(&var_thpd.var_st_sen);
    thgsens_delete(&var_thpd.var_tmp_sen);
    thgsens_delete(&var_thpd.var_v1_sen);
    thgsens_delete(&var_thpd.var_v2_sen);
    thgsens_delete(&var_thpd.var_p1_sen);
    thgsens_delete(&var_thpd.var_p2_sen);

    /* set function pointers to NULL */
    var_thpd.var_update_dp = NULL;
    var_thpd.var_update_velocity = NULL;
    var_thpd.var_update_static = NULL;
    var_thpd.var_update_temp = NULL;
    return;
}

/* Set settling and reading times */
int thpd_set_settle_time(double stime)
{
    var_thpd.var_settle = stime;
    return 0;
}

/* Set read time */
int thpd_set_read_time(double rtime)
{
    var_thpd.var_read_time = rtime;
    return 0;
}

/* set external pointers */
int thpd_set_ext_pointers(gthor_fptr update_dp,
			  gthor_fptr update_velocity,
			  gthor_fptr update_static,
			  gthor_fltr update_temp,
			  void* sobj)
{
    var_thpd.var_update_dp = update_dp;
    var_thpd.var_update_velocity = update_velocity;
    var_thpd.var_update_static = var_static;
    var_thpd.var_update_temp = update_temp;
    var_thpd.var_sobj = sobj;
    return 0;
}

/* set file pointer */
int thpd_set_file_pointer(FILE* fp)
{
    var_thpd.var_fp = fp;
    return 0;
}

int thpd_set_damper_size(double width, double height)
{
    var_thpd.var_dwidth = width;
    var_thpd.var_dheight = height;
    var_thpd.var_farea = width * height * (1/1000000);
    return 0;
}

int thpd_set_duct_dia(double dia)
{
    var_thpd.var_ddia = dia;
    var_thpd.var_darea = pow((dia/2), 2)*M_PI;
    return 0;
}


/* asynchronous start */
int thpd_start(void)
{

}


/* stop thread */
int thpd_stop(void)
{

}

/****************************************************************************************************/
/* Private methods */
DWORD WINAPI _thpd_async_start(LPVOID obj);
{

}
