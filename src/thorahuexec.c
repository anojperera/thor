#if defined (WIN32) || defined (_WIN32)
#include <windows.h>
#include <conio.h>
#else
#include <time.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include "thorexec.h"
#include "thahup.h"

#define THOR_AHU_DUCT_DIA 1120.0

#define THOR_AHU_STATIC_RNG_MIN 0.0							/* minimum static pressure */
#define THOR_AHU_STATIC_RNG_MAX 5000.0							/* maximum static pressure */
#define THOR_AHU_TEMP_RNG_MIN -10.0							/* minimum temperature */
#define THOR_AHU_TEMP_RNG_MAX 40.0							/* maximum temperature */

#define THOR_AHU_INIT_PROG 105								/* i */
#define THOR_AHU_QUIT_CODE1 81								/* Q */
#define THOR_AHU_QUIT_CODE2 113								/* q */
#define THOR_AHU_ACT_INCR_CODE 43							/* + */
#define THOR_AHU_ACT_DECR_CODE 45							/* - */
#define THOR_AHU_PRG_START_CODE 83							/* S */
#define THOR_AHU_PRG_STOP_CODE 115							/* s */
#define THOR_AHU_ACT_INCRF_CODE 42							/* * */
#define THOR_AHU_ACT_DECRF_CODE 47							/* / */

#define THOR_AHU_MAIN_MSG_FORMAT "%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\r\r"
#define THOR_AHU_MAIN_OPTMSG_FORMAT "%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\r\r%s\r"
#define THOR_AHU_ACTUATOR_PST "\rACTUATOR: ======= %.2f =======\r"
#define THOR_AHU_MSG_BUFFER_SZ 2048							/* main buffer size */
#define THOR_AHU_OPT_MSG_BUFFER_SZ 64							/* optional buffer size */
#define THOR_AHU_RESULT_BUFF_SZ 8							/* result buffer size */

#define THOR_AHU_WAIT_TIME 500								/* waiting time for the main loop */
#define THOR_AHU_WAIT_TIME_UNIX_CORRECTION 1000						/* correction for unix */
#define THOR_AHU_ACT_INCR 5.0
#define THOR_AHU_ACT_DECR -5.0
#define THOR_AHU_ACT_ADJT_FINE 1.0							/* actuator control adjustment */
#define THOR_AHU_ACT_FLOOR 5.0								/* minimum actuator control percentage */
#define THOR_AHU_ACT_CEIL 60.0								/* maximum acturator percentage voltage */
#define THOR_AHU_MSG_DURATION 4								/* message duration */
static unsigned int _init_flg = 0;							/* initialise flag */
static unsigned int _start_flg = 0;							/* start test flag */
static int _ctrl_ix;									/* control flag */
static int _quit_flg = 1;								/* quit flag */

static char _thor_msg_buff[THOR_AHU_MSG_BUFFER_SZ];					/* main message buffer */
static char _thor_optmsg_buff[THOR_AHU_OPT_MSG_BUFFER_SZ];				/* optional buffer size */

static double _thor_act_pos = THOR_AHU_ACT_FLOOR;					/* actuator percentage */
static double _thor_result_buffer[THOR_AHU_OPT_MSG_BUFFER_SZ];

static FILE* _thor_result_fp = NULL;							/* result file pointer */
static thahup* thahup_obj = NULL;							/* AHU object */

/* counters */
static unsigned int _thor_msg_cnt = 0;							/* message counter */

/* Control methods */
static int _thor_init_var(void);
static int _thor_ahu_init(void);
static int _thor_update_msg_buff(char* buff, char* opts);
static int _thor_adjust_act(double val);
inline __attribute__ ((always_inline)) static int _thor_replace_newline(char* buff);


/* thread function for win32 */
#if defined (WIN32) || defined (_WIN32)
DWORD WINAPI _thor_msg_handler(LPVOID obj);
HANDLE _thor_mutex;
#endif

/* main loop */
int thorahuexec_main(int argc, char** argv)
{
    /* environment specific variables */
#if defined (WIN32) || defined (_WIN32)
    HANDLE _thhandle;
#else
    struct timespec t;
    t.tv_sec = 0;
    t.tv_nsec = THOR_AHU_WAIT_TIME * THOR_AHU_WAIT_TIME_UNIX_CORRECTION;
#endif

    /* initialise variables */
    _thor_init_var();
    _thor_ahu_init();

    /* create thread for updating stdout */
#if defined (WIN32) || defined (_WIN32)
    /* intialise mutex */
    _thor_mutex = CreateMutex(NULL, FALSE, NULL);
    if(_thor_mutex == NULL)
	{
	    fprintf(stderr, "CreateMutex error\n");
	    return 1;
	}
    printf("DP1\tDP2\tDP3\tDP4\tStatic\tVel\tVol\tTemp\n");
    _thhandle = CreateThread(NULL, 0, _thor_msg_handler, NULL, 0, NULL);
    /* exit and clean up if failes */
    if(_thhandle == NULL)
	{
	    CloseHandle(_thor_mutex);
	    return 0;
	}
#endif

    /**
     * Main loop for receving input. Continuously scans for input and takes
     * action as defined.
     **/
    while(_quit_flg)
	{
	    if(_thor_msg_cnt == 0)
		_thor_update_msg_buff(_thor_msg_buff, NULL);
	    fflush(stdout);
	    fprintf(stdout, "%s\r", _thor_msg_buff);
#if defined (WIN32) || defined (_WIN32)
	    Sleep(THOR_AHU_WAIT_TIME);
#else
	    nanosleep(&t, NULL);
#endif
	    if(_thor_msg_cnt>0)
		_thor_msg_cnt--;
	}

    if(_start_flg)
	thahup_stop(NULL);
    /* delete ahu program */
    thahup_delete();
    thahup_obj = NULL;

    /* wait for thread to closed */
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_thhandle, INFINITE);
    CloseHandle(_thhandle);
    CloseHandle(_thor_mutex);
#endif
    return 0;
}

/* initialise variables */
static int _thor_init_var(void)
{
    int i;
    /* initialise message buffer */
    memset((void*) _thor_msg_buff, 0, THOR_AHU_MSG_BUFFER_SZ);
    for(i=0; i<THOR_AHU_RESULT_BUFF_SZ; i++)
	_thor_result_buffer[i] = 0.0;
    return 0;
}

/* update message buffer */
static int _thor_update_msg_buff(char* buff, char* opts)
{
    /* lock mutex */
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_thor_mutex, INFINITE);
#endif
    if(opts != NULL)
	{
	    sprintf(buff, THOR_AHU_MAIN_OPTMSG_FORMAT,
		    _thor_result_buffer[THAHUP_RESULT_BUFF_DP1_IX],			/* DP 1 */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_DP2_IX],			/* DP 2 */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_DP3_IX],			/* DP 3 */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_DP4_IX],			/* DP 4 */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_ST_IX],			/* static */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_VEL_IX],			/* velocity */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_VOL_IX],			/* volume flow */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_TMP_IX],			/* temp */		    
		    opts);
	}
    else
	{
	    sprintf(buff, THOR_AHU_MAIN_MSG_FORMAT,
		    _thor_result_buffer[THAHUP_RESULT_BUFF_DP1_IX],			/* DP 1 */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_DP2_IX],			/* DP 2 */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_DP3_IX],			/* DP 3 */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_DP4_IX],			/* DP 4 */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_ST_IX],			/* static */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_VEL_IX],			/* velocity */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_VOL_IX],			/* volume flow */
		    _thor_result_buffer[THAHUP_RESULT_BUFF_TMP_IX]);			/* temp */
	}
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(_thor_mutex);
#endif

    return 0;
}


/* key press handler */
DWORD WINAPI _thor_msg_handler(LPVOID obj)
{
    while(1)
	{
#if defined (WIN32) || defined (_WIN32)
	    _ctrl_ix = _getch();
#else
	    _ctrl_ix = getchar();
#endif
	    /* flush input buffer */
	    fflush(stdin);
	    if(_ctrl_ix == THOR_AHU_QUIT_CODE1 || _ctrl_ix == THOR_AHU_QUIT_CODE2)
		{
	    /* lock mutex */
#if defined (WIN32) || defined (_WIN32)
	    	WaitForSingleObject(_thor_mutex, INFINITE);
#endif
	    	_quit_flg = 0;
#if defined (WIN32) || defined (_WIN32)
	    	ReleaseMutex(_thor_mutex);
#endif
	    	break;
		}

#if defined (WIN32) || defined (_WIN32)
	    WaitForSingleObject(_thor_mutex, INFINITE);
#endif
	    switch(_ctrl_ix)
		{
 		case THOR_AHU_ACT_INCR_CODE:
		    _thor_adjust_act(THOR_AHU_ACT_INCR);
		    break;
		case THOR_AHU_ACT_DECR_CODE:
    		    _thor_adjust_act(THOR_AHU_ACT_DECR);
		    break;
		case THOR_AHU_PRG_START_CODE:
		    thahup_start(NULL);
		    _start_flg = 1;
		    break;
		case THOR_AHU_PRG_STOP_CODE:
		    thahup_stop(NULL);
		    _start_flg = 0;
		    break;
		case THOR_AHU_ACT_INCRF_CODE:
		    _thor_adjust_act(THOR_AHU_ACT_ADJT_FINE);
		    break;
		case THOR_AHU_ACT_DECRF_CODE:
		    _thor_adjust_act(-1*THOR_AHU_ACT_ADJT_FINE);
		    break;		    
		}
#if defined (WIN32) || defined (_WIN32)
	    ReleaseMutex(_thor_mutex);
#endif	    /* call to update the message buffer */
	}
    return 0;
}

/* adjust actuator position */
static int _thor_adjust_act(double val)
{
    if(val > 0 && _thor_act_pos<THOR_AHU_ACT_CEIL-THOR_AHU_ACT_INCR)
	_thor_act_pos += val;
    else if(val< 0 && _thor_act_pos>THOR_AHU_ACT_FLOOR+THOR_AHU_ACT_DECR)
	_thor_act_pos += val;

    sprintf(_thor_optmsg_buff, THOR_AHU_ACTUATOR_PST, _thor_act_pos);
    _thor_msg_cnt = THOR_AHU_MSG_DURATION;
    _thor_update_msg_buff(_thor_msg_buff, _thor_optmsg_buff);
    thahup_set_actctrl_volt(_thor_act_pos);
    return 0;
}

/* replaces the inline character with line return */
inline __attribute__ ((always_inline)) static int _thor_replace_newline(char* buff)
{
    char* _ch;
    _ch = strchr(buff, '\n');
    if(_ch)
	*_ch = '\r';
    return 0;
}

/* initialise ahu program */
static int _thor_ahu_init(void)
{
    if(!_init_flg)
	{
	    if(thahup_initialise(thahup_man,					/* control option */
				 _thor_result_fp,				/* file pointer */
				 NULL,
				 NULL,
				 NULL,
				 NULL,
				 NULL,
				 NULL,
				 NULL,
				 NULL,
				 &thahup_obj,
				 NULL))
		{
		    /* unable to connect to asgard */
		    printf("%s\n","unable to connect to asgard");
		    return 1;
		}
	    else
		{
		    _init_flg = 1;
   		    thahup_reset_sensors(NULL);
		}
	}
    else if(thahup_obj)
	{
	    /* reconfigure thor */
	    thahup_reset_sensors(NULL);
	    thahup_obj->var_stctrl = thahup_man;
	}
    thahup_set_ductdia(thahup_obj, THOR_AHU_DUCT_DIA);
    thahup_set_result_buffer(thahup_obj, _thor_result_buffer);
     /* set sensor range */
    printf("%s\n","setting sensor range..");
    thgsens_set_range(thahup_obj->var_stsensor,
		      THOR_AHU_STATIC_RNG_MIN,
		      THOR_AHU_STATIC_RNG_MAX);

    thgsens_set_range(thahup_obj->var_tmpsensor,
		      THOR_AHU_TEMP_RNG_MIN,
		      THOR_AHU_TEMP_RNG_MAX);

    printf("prestart complete\n");
    return 0;
}
