/* Damper slam shut test */
#if defined (WIN32) || defined (_WIN32)
#include <windows.h>
#include <conio.h>
#else
#include <time.h>
#include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#define THOR_ACTST_DUCT_DIA 510.0
#define THOR_ACTST_DMP_WIDTH 400.0
#define THOR_ACTST_DMP_HEIGHT 400.0

#define THOR_ACTST_INIT_PROG 105							/* i */
#define THOR_ACTST_QUIT_CODE1 81							/* Q */
#define THOR_ACTST_QUIT_CODE2 113							/* q */
#define THOR_ACTST_INCR_FAN_CODE 43							/* + */
#define THOR_ACTST_DECR_FAN_CODE 45							/* - */
#define THOR_ACTST_INCRF_FAN_CODE 42							/* * */
#define THOR_ACTST_DECRF_FAN_CODE 47							/* / */

#define THOR_ACTST_MAIN_MSG_FORMAT "%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\r"
#define THOR_ACTST_MAIN_OPTMSG_FORMAT "%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\r\r%s\r"
#define THOR_ACTST_MAIN_FAN_FORMAT "\rFAN: ===== %.2f =====\r"

#define THOR_ACTST_MSG_BUFF_SZ 2048
#define THOR_ACTST_RESULT_BUFF_SZ 6
#define THOR_ACTST_WAIT_TIME 500
#define THOR_ACTST_WAIT_TIME_UNIX_CORRECTION 1000

static unsigned int _init_flg = 0;
static unsigned int _start_flg = 0;
static unsigned int _quit_flg = 1;
static unsigned int _ctrl_ix = 0;

static unsigned int _thor_msg_cnt = 0;							/* message counter */
static double _thor_result_buff[THOR_ACTST_RESULT_BUFF_SZ];
static thactst _th_actst;

/* private methods */
static int _thor_init(void);
static int _thor_init_var(void);
static int _thor_update_msg_buff(char* buff, char* opts);

/* Thread function for Win32 */
#if defined (WIN32) || defined (_WIN32)
DWORD WINAPI _thor_msg_handler(LPVOID obj);
HANDLE _thor_mutex;
#else
void* _thor_msg_handler(void* obj);
#endif

/* main loop */
int thoractstexec_main(int argc, char** argv)
{
    /* environment spcific variables */
#if defined (WIN32) || defined (_WIN32)
    HANDLE _thhandle;
#else
    struct timespec t;
    t.tv_sec = 0;
    t.tv_nsec = THOR_ACTST_WAIT_TIME * THOR_ACTST_WAIT_TIME_UNIX_CORRECTION;
#endif


#if defined (WIN32) || defined (_WIN32)
    /* initialise matrix */
    _thor_mutex = CreateMutex(NULL, FALSE, NULL);
    if(_thor_mutex)
	{
	    fprintf(stderr, "CreateMutex error\n");
	    return 1;
	}
    printf("DP1\tDP2\tStatic\tVol\tVel(Dmp)\tTemp\n");
    _thhandle = CreateThread(NULL, 0, _thor_msg_handler, NULL, 0, NULL);
    /* exit and clean up if failes */
    if(_thhandle == NULL)
	{
	    CloseHandle(_thor_mutex);
	    return 0;
	}
#endif

    /**
     * Main loop for handling process control. Continuously scans for input and take action as
     * defined.
     **/
    while(_quit_flg)
	{
	    if(_thor_msg_cnt == 0)
		_thor_update_msg_buff(_thor_msg_buff, NULL);
	    fflush(stdout);
	    fprintf(stdout, "%s\r", _thor_msg_buff);
#if defined (WIN32) || defined (_WIN32)
	    Sleep(THOR_ACTST_WAIT_TIME);
#else
	    nanosleep(&t, NULL);
#endif
	    if(_thor_msg_cnt>0)
		_thor_msg_cnt--;
	}

    if(_start_flg)
	thactst_stop();
    /* delete program */
    thactst_delete();

#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_thhandle, INFINITE);
    CloseHandle(_thhandle);
    CloseHandle(_thor_mutex);
#endif
    return 0;
}

/* intialise variables */
static int _thor_init_var(void)
{
    int i;
    memset((void*) _thor_msg_buff, 0, THOR_ACTST_MSG_BUFF_SZ);
    for(i=0; i<THOR_ACTST_RESULT_BUFF_SZ; i++)
	_thor_result_buff[i] = 0.0;
    return 0;
}

/* update message buffer */
static int _thor_update_msg_buff(char* buff, char* opts)
{
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_thor_mutex, INFINITE);
#endif
    if(opts != NULL)
	{
	    sprintf(buff, THOR_ACTST_MAIN_OPTMSG_FORMAT,
		    _thor_result_buff[THACTST_DP1_IX],
		    _thor_result_buff[THACTST_DP2_IX],
		    _thor_result_buff[THACTST_ST_IX],
		    _thor_result_buff[THACTST_VOL_IX],
		    _thor_result_buff[THACTST_V_IX],
		    _thor_result_buff[THACTST_TMP_IX],
		    opts);
	}
    else
	{
	    sprintf(buff, THOR_ACTST_MAIN_MSG_FORMAT,
		    _thor_result_buff[THACTST_DP1_IX],
		    _thor_result_buff[THACTST_DP2_IX],
		    _thor_result_buff[THACTST_ST_IX],
		    _thor_result_buff[THACTST_VOL_IX],
		    _thor_result_buff[THACTST_V_IX],
		    _thor_result_buff[THACTST_TMP_IX]);
	}

    /* release mutex */
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(_thor_mutex);
#endif
    return 0;
}

/* key press handler */
#if defined (WIN32) || defined (_WIN32)
DWORD WINAPI _thor_msg_handler(LPVOID obj)
#else
void* _thor_msg_handler(void* obj)
#endif    
{
    while(1)
	{
#if defined (WIN32) || defined (_WIN32)
	    _ctrl_ix = _getch();
#else
	    _ctrl_ix = getchar();

	    /* flush input buffer */
	    fflush(stdin);
	    if(_ctrl_ix == THOR_ACTST_QUIT_CODE1 || _ctrl_ix == THOR_ACTST_QUIT_CODE2)
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
		
	}

#if defined (WIN32) || defined (_WIN32)
    return 0;
#else
    return NULL;
#endif    
}
