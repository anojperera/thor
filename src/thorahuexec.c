#if defined (WIN32) || defined (_WIN32)
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#endif
#include <getopt.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "thorexec.h"
#include "thahup.h"

#define THOR_AHU_DUCT_DIA 1120

#define THOR_AHU_STATIC_RNG_MIN 0.0							/* minimum static pressure */
#define THOR_AHU_STATIC_RNG_MAX 5000.0							/* maximum static pressure */
#define THOR_AHU_DEFAULT_MAX_STATIC 0.0
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
#define THOR_AHU_ACT_CEIL 95.0								/* maximum acturator percentage voltage */
#define THOR_AHU_MSG_DURATION 4								/* message duration */
#define THOR_AHU_LOG_FILE_NAME "Log.txt"
static unsigned int _init_flg = 0;							/* initialise flag */
static unsigned int _start_flg = 0;							/* start test flag */
static int _ctrl_ix;									/* control flag */
static int _quit_flg = 1;								/* quit flag */

static char _thor_msg_buff[THOR_AHU_MSG_BUFFER_SZ];					/* main message buffer */
static char _thor_optmsg_buff[THOR_AHU_OPT_MSG_BUFFER_SZ];				/* optional buffer size */

 static double _thor_act_pos = THOR_AHU_ACT_FLOOR;					/* actuator percentage */
static int _thor_ahu_duct_dia = THOR_AHU_DUCT_DIA;
static int _thor_def_static = THOR_AHU_DEFAULT_MAX_STATIC;
static double _thor_result_buffer[THOR_AHU_OPT_MSG_BUFFER_SZ];
static int _thor_num_sensors = 4;
static FILE* _thor_result_fp = NULL;							/* result file pointer */
static thahup* thahup_obj = NULL;							/* AHU object */

/* counters */
static unsigned int _thor_msg_cnt = 0;							/* message counter */

/* Control methods */
static int _thor_init_var(void);
static int _thor_ahu_init(void);
static int _thor_update_msg_buff(char* buff, char* opts);
static int _thor_adjust_act(double val);
static int _thor_parse_args(int argc, char** argv);
inline __attribute__ ((always_inline)) static int _thor_replace_newline(char* buff);
inline __attribute__ ((always_inline)) static int _thor_open_file(void);

/* thread function for win32 */
#if defined (WIN32) || defined (_WIN32)
static DWORD WINAPI _thor_msg_handler(LPVOID obj);
static HANDLE _thor_mutex;
#else
void* _thor_msg_handler(void* obj);
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
    /* call to create the file pointer */
    if(_thor_open_file())
	{
	    fprintf(stdout, "Unable to open file, data not logged\n");
	    _thor_result_fp = NULL;
	}
    /* call to parse arguments */
    _thor_parse_args(argc, argv);
    
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
	    if(_thor_result_fp == NULL)
		fclose(_thor_result_fp);
	    return 1;
	}
    printf("DP1\tDP2\tDP3\tDP4\tStatic\tVel\tVol\tTemp\n");
    _thhandle = CreateThread(NULL, 0, _thor_msg_handler, NULL, 0, NULL);
    /* exit and clean up if failes */
    if(_thhandle == NULL)
	{
	    CloseHandle(_thor_mutex);
	    if(_thor_result_fp == NULL)
		fclose(_thor_result_fp);	    
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
    fclose(_thor_result_fp);
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
    thvelsen_enable_sensor(thahup_obj->var_velocity, 0);
    thvelsen_enable_sensor(thahup_obj->var_velocity, 1);
    if(_thor_num_sensors > 3)
	{
	    thvelsen_enable_sensor(thahup_obj->var_velocity, 2);
	    thvelsen_enable_sensor(thahup_obj->var_velocity, 3);
	}
    thahup_set_stop_val(thahup_obj, (double) _thor_def_static);
    thahup_set_ductdia(thahup_obj, (double) _thor_ahu_duct_dia);
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

/* Parse argument */
static int _thor_parse_args(int argc, char** argv)
{
    int _next_opt;
    char _arg_buff[THOR_AHU_OPT_MSG_BUFFER_SZ];
    const char* const _short_opts = ":D:N:S:";
    const struct option _long_opts[] = {
	{"duct-dia", 1, NULL, 'D'},
	{"num-sensors", 1, NULL, 'N'},
	{"max-static", 1, NULL, 'S'},
	{NULL, 0, NULL, 0}
    };
    
    _thor_ahu_duct_dia = 0;
    _thor_num_sensors = 0;
    _thor_def_static = 0;
    
    /* parse arguments */
    do
	{
	    _next_opt = getopt_long(argc, argv, _short_opts, _long_opts, NULL);
	    switch(_next_opt)
		{
		case 'D':
		    if(optarg == NULL)
			break;
		    strncpy(_arg_buff, optarg, THOR_AHU_OPT_MSG_BUFFER_SZ-1);
		    _thor_ahu_duct_dia = atoi(_arg_buff);
		    break;
		case 'N':
		    if(optarg == NULL)
			break;
		    strncpy(_arg_buff, optarg, THOR_AHU_OPT_MSG_BUFFER_SZ-1);
		    _thor_num_sensors = atoi(_arg_buff);
		    break;		    
		case 'S':
		    if(optarg == NULL)
			break;
		    strncpy(_arg_buff, optarg, THOR_AHU_OPT_MSG_BUFFER_SZ-1);
		    _thor_def_static = atoi(_arg_buff);
		    break;		    		    
		case -1:
		default:
		    break;
		}

	}while(_next_opt != -1);

    /* check values */
    if(_thor_ahu_duct_dia <= 0)
	{
	    printf("\nEnter Duct Diameter (200/600/1120): ");
	    scanf("%i", &_thor_ahu_duct_dia);
	}

    if(_thor_num_sensors <= 0)
	{
	    printf("\nEnter number of sensors (4/2): ");
	    scanf("%i", &_thor_num_sensors);
	}

    if(_thor_def_static <= 0)
	{
	    printf("\nEnter max external static pressure range : ");
	    scanf("%i", &_thor_def_static);
	}

    /* assign defaults if the vaules are still invalid */
    if(_thor_ahu_duct_dia <= 0.0)
	_thor_ahu_duct_dia = THOR_AHU_DUCT_DIA;
    printf("Duct dia %i\n", _thor_ahu_duct_dia);
    if(_thor_num_sensors <= 0)
	_thor_num_sensors = 4;
    if(_thor_def_static < 0)
	_thor_def_static = _thor_def_static;
    return 0;
}

/* open file pointer with time based file name */
inline __attribute__ ((always_inline)) static int _thor_open_file(void)
{
    char _file_name[THOR_AHU_OPT_MSG_BUFFER_SZ];
    time_t _tm;
    struct tm* _tm_info;

    time(&_tm);
    _tm_info = localtime(&_tm);
    strftime(_file_name, THOR_AHU_OPT_MSG_BUFFER_SZ, "%Y-%m-%d-%I-%M-%S.txt", _tm_info);

    /* open file pointer */
    if((_thor_result_fp = fopen(_file_name, "w+")) == NULL)
	return -1;

    return 0;
}
