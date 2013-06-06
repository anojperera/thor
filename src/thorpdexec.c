/* User interface for pressure drop program.
 * Tue Jun  4 00:44:07 BST 2013 */
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
#include "thpd.h"

#define THOR_PD_DUCT_DIA 300.0
#define THOR_PD_D_WIDTH 500.0
#define THOR_PD_D_HEIGHT 500.0
#define THOR_PD_RESULT_BUFF_SZ 8
#define THOR_PD_MSG_BUFFER_SZ 2048
#define THOR_PD_FILE_NAME_SZ 64

#define THOR_PD_INIT_PROG 105								/* i */
#define THOR_PD_QUIT_CODE1 81								/* Q */
#define THOR_PD_QUIT_CODE2 113								/* q */
#define THOR_PD_PRG_START_CODE 83							/* S */
#define THOR_PD_PRG_STOP_CODE 115							/* s */

#define THOR_PD_WAIT_TIME 500								/* waiting time for the main loop */
#define THOR_PD_WAIT_TIME_UNIX_CORRECTION 1000						/* correction for unix */
#define THOR_PD_MAIN_MSG_FORMAT "%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\r"
static unsigned int _init_flg = 0;							/* initialise flag */
static unsigned int _start_flg = 0;							/* start test flag */
static unsigned int _quit_flg = 1;
static unsigned int _ctrl_ix = 0;

static char _thor_msg_buff[THOR_PD_MSG_BUFFER_SZ];					/* main message buffer */
static double _duct_dia = THOR_PD_DUCT_DIA;						/* duct diameter */
static double _d_width = THOR_PD_D_WIDTH;						/* damper width */
static double _d_height = THOR_PD_D_HEIGHT;						/* damper height */
static double _thor_result_buff[THOR_PD_RESULT_BUFF_SZ];				/* result buffer */
static FILE* _thor_result_fp = NULL;							/* result file pointer */
static thpd* _var_thpd = NULL;

/* counters */
static unsigned int _thor_msg_cnt = 0;							/* message counter */

/* Control methods */
static int _thor_init_var(void);
static int _thor_pd_init(void);
static int _thor_update_msg_buff(char* buff, char* opts);
static int _thor_parse_args(int argc, char** argv);
inline __attribute__ ((always_inline)) static int _thor_open_file(void);

/* thread function for win32 */
#if defined (WIN32) || defined (_WIN32)
static DWORD WINAPI _thor_msg_handler(LPVOID obj);
static HANDLE _thor_mutex;
#else
void* _thor_msg_handler(void* obj);
#endif


/* Main loop */
int thorpdexec_main(int argc, char** argv)
{
#if defined (WIN32) || defined (_WIN32)
    HANDLE _thhandle;
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
    _thor_pd_init();

    /* create thread for updating stdout */
#if defined (WIN32) || defined (_WIN32)
    /* initialise mutex */
    _thor_mutex = CreateMutex(NULL, FALSE, NULL);
    if(_thor_mutex == NULL)
	{
	    fprintf(stderr, "Create Mutex error\n");
	    if(_thor_result_fp == NULL)
		fclose(_thor_result_fp);
	    return 1;
	}

    printf ("V_DP1\tV_DP2\tDP1\tDP2\tVel\tVol\tDP\tTemp\n");
    _thhandle = CreateThread(NULL, 0, _thor_msg_handler, NULL, 0, NULL);
    /* exit and clean up if failed */
    if(_thhandle == NULL)
	{
	    CloseHandle(_thor_mutex);
	    if(_thor_result_fp)
		fclose(_thor_result_fp);
	    return 0;
	}
#endif

    /**
     * Main loop for recieving input, Continuously scans for input and takes
     * action as defined.
     **/
    while(_quit_flg)
	{
	    if(_thor_msg_cnt == 0)
		_thor_update_msg_buff(_thor_msg_buff, NULL);
	    fflush(stdout);
	    fprintf(stdout, "%s\r", _thor_msg_buff);

#if defined (WIN32) || defined (_WIN32)
	    Sleep(THOR_PD_WAIT_TIME);
#endif
	    if(_thor_msg_cnt>0)
		_thor_msg_cnt--;

	}

    if(_start_flg)
	thpd_stop();

    /* delete pressure drop program */
    thpd_delete();

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
    int i=0;
    for(i=0; i<THOR_PD_MSG_BUFFER_SZ; i++)
	_thor_msg_buff[i] = 0.0;
    return 0;
}

/* initialise pressure drop test */
static int _thor_pd_init(void)
{
    /* create test */
    _var_thpd = thpd_initialise(NULL);
    if(_var_thpd == NULL)
	return 1;

    thpd_set_buffer(_thor_result_buff);
    thpd_set_file_pointer(_thor_result_fp);

    thpd_set_damper_size(_d_width, _d_height);
    thpd_set_duct_dia(_duct_dia);
    thpd_set_mutex(_thor_mutex);
    _init_flg = 1;
    return 0;
}

/* Update message buffer */
static int _thor_update_msg_buff(char* buff, char* opts)
{
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_thor_mutex, INFINITE);
#endif
    sprintf(buff, THOR_PD_MAIN_MSG_FORMAT,
	    _thor_result_buff[THPD_RESULT_VDP1_IX],
	    _thor_result_buff[THPD_RESULT_VDP2_IX],
	    _thor_result_buff[THPD_RESULT_DP1_IX],
	    _thor_result_buff[THPD_RESULT_DP2_IX],
	    _thor_result_buff[THPD_RESULT_VEL_IX],
	    _thor_result_buff[THPD_RESULT_VOL_IX],
	    _thor_result_buff[THPD_RESULT_DP_IX],
	    _thor_result_buff[THPD_RESULT_TMP_IX]);
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(_thor_mutex);
#endif    
    return 0;
}

/* Parse command line arguments */
static int _thor_parse_args(int argc, char** argv)
{
    /* Flush standard input */
    fflush(stdin);
    printf("\nEnter Duct Diameter (200/300): ");
    scanf("%f", (float*) &_duct_dia);
    if(_duct_dia < 0.0)
	_duct_dia = THOR_PD_DUCT_DIA;

    fflush(stdin);
    printf("\nEnter Damper Width (mm): ");
    scanf("%f", (float*) &_d_width);
    if(_d_width < 0.0)
	_d_width = THOR_PD_D_WIDTH;

    fflush(stdin);
    printf("\nEnter Damper Height (mm): ");
    scanf("%f", (float*) &_d_height);
    if(_d_height < 0.0)
	_d_height = THOR_PD_D_HEIGHT;

    return 0;
}

#if defined (WIN32) || defined (_WIN32)
static DWORD WINAPI _thor_msg_handler(LPVOID obj)
#else
void* _thor_msg_handler(void* obj)
#endif
{
    while(1)
	{
#if defined (WIN32) || defined (_WIN32)
	    _ctrl_ix = getch();
#else
	    _ctrl_ix = getchar();
#endif

	    /* flush input buffers */
	    fflush(stdin);
	    if(_ctrl_ix == THOR_PD_QUIT_CODE1 || _ctrl_ix == THOR_PD_QUIT_CODE2)
		{
#if defined (WIN32) || defined (_WIN32)
		    WaitForSingleObject(_thor_mutex, INFINITE);
#endif
		    _quit_flg = 0;
#if defined (_WIN32) || defined (_WIN32)
		    ReleaseMutex(_thor_mutex);
#endif
		    break;
		}
	    switch(_ctrl_ix)
		{
		case THOR_PD_PRG_START_CODE:
		    if(_init_flg == 0)
			{
			    fprintf(stderr, "Program not initialised\n");
			    break;
			}
		    thpd_start();
		    _start_flg = 1;
		    break;
		case THOR_PD_PRG_STOP_CODE:
		    thpd_stop();
		    _start_flg = 0;
		    break;
		}
	    _ctrl_ix = -1;
	}
#if defined (WIN32) || defined(_WIN32)
    return 0;
#else
    return NULL;
#endif    
}

/* Open file name */
inline __attribute__ ((always_inline)) static int _thor_open_file(void)
{
    char _file_name[THOR_PD_FILE_NAME_SZ];
    time_t _tm;
    struct tm* _tm_info;

    time(&_tm);
    _tm_info = localtime(&_tm);
    strftime(_file_name, THOR_PD_FILE_NAME_SZ, "%Y-%m-%d-%I-%M-%S.txt", _tm_info);

    /* open file pointer */
    if((_thor_result_fp = fopen(_file_name, "w+")) == NULL)
	return -1;

    return 0;    
}
