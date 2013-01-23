/* Main executable program for the thor test suit */
/* 22/01/2013 */
#if defined (WIN32) || defined (_WIN32)
#include <windows.h>
#include <conio.h>
#else
#include <time.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include "thahup.h"

#define THOR_QUIT_CODE1 81							/* Q */
#define THOR_QUIT_CODE2 113							/* q */
#define THOR_ACT_INCR_CODE 43							/* + */
#define THOR_ACT_DECR_CODE 45							/* - */

#define THOR_MAIN_MSG_FORMAT "%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\r"
#define THOR_MAIN_OPTMSG_FORMAT "%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\r%s\r"
#define THOR_ACTUATOR_PST "ACTUATOR: ======= %f ======="
#define THOR_MSG_BUFFER_SZ 2048							/* main buffer size */
#define THOR_OPT_MSG_BUFFER_SZ 64						/* optional buffer size */

#define THOR_WAIT_TIME 500							/* waiting time for the main loop */
#define THOR_WAIT_TIME_UNIX_CORRECTION 1000					/* correction for unix */
#define THOR_ACT_INCR 0.5
#define THOR_ACT_FLOOR 2.0							/* minimum actuator control percentage */
#define THOR_ACT_CEIL 9.0							/* maximum acturator percentage voltage */
#define THOR_MSG_DURATION 4							/* message duration */
static int _ctrl_ix;								/* control flag */
static int _quit_flg = 1;							/* quit flag */
static char _thor_msg_buff[THOR_MSG_BUFFER_SZ];					/* main message buffer */
static char _thor_optmsg_buff[THOR_OPT_MSG_BUFFER_SZ];				/* optional buffer size */
static double _thor_act_pos = THOR_ACT_FLOOR;					/* actuator percentage */

/* counters */
static unsigned int _thor_msg_cnt = 0;						/* message counter */

/* Control methods */
/* static int _thor_ahu_init(void); */
static int _thor_update_msg_buff(char* buff, char* opts);
static int _thor_increment_act(void);
static int _thor_decrement_act(void);
/* thread function for win32 */
#if defined (WIN32) || defined (_WIN32)
DWORD WINAPI _thor_key_handler(LPVOID obj);
HANDLE _thor_mutex;
#endif

/* main loop */
int main(int argc, char** argv)
{
    /* environment specific variables */
#if defined (WIN32) || defined (_WIN32)
    HANDLE _thhandle;
#else
    struct timespec t;
    t.tv_sec = 0;
    t.tv_nsec = THOR_WAIT_TIME * THOR_WAIT_TIME_UNIX_CORRECTION;
#endif
    /* initialise message buffer */
    memset((void*) _thor_msg_buff, 0, THOR_MSG_BUFFER_SZ);

    /* create thread for updating stdout */
#if defined (WIN32) || defined (_WIN32)
    /* intialise mutex */
    _thor_mutex = CreateMutex(NULL, FALSE, NULL);
    if(_thor_mutex == NULL)
	{
	    fprintf(stderr, "CreateMutex error\n");
	    return 1;
	}
    _thhandle = CreateThread(NULL, 0, _thor_key_handler, NULL, 0, NULL);
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
    printf("DP1\tDP2\tDP3\tDP4\tVel\tVol\tStatic\tTemp\n");
    while(_quit_flg)
	{
	    /* call to update the message buffer */
	    if(_thor_msg_cnt == 0)
		_thor_update_msg_buff(_thor_msg_buff, NULL);
	    fflush(stdout);
	    fprintf(stdout, "%s\r", _thor_msg_buff);
#if defined (WIN32) || defined (_WIN32)
	    Sleep(THOR_WAIT_TIME);
#else
	    nanosleep(&t, NULL);
#endif
	    if(_thor_msg_cnt>0)
		_thor_msg_cnt--;
	}
    /* wait for thread to closed */
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_thhandle, INFINITE);
    CloseHandle(_thhandle);
    CloseHandle(_thor_mutex);
#endif
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
	    sprintf(buff, THOR_MAIN_OPTMSG_FORMAT,
		    0.0,								/* DP 1 */
		    0.0,								/* DP 2 */
		    0.0,								/* DP 3 */
		    0.0,								/* DP 4 */
		    0.0,								/* static */
		    0.0,								/* velocity */
		    0.0,								/* volume flow */
		    0.0,								/* temp */
		    opts);
	}
    else
	{
	    sprintf(buff, THOR_MAIN_MSG_FORMAT,
		    0.0,								/* DP 1 */
		    0.0,								/* DP 2 */
		    0.0,								/* DP 3 */
		    0.0,								/* DP 4 */
		    0.0,								/* static */
		    0.0,								/* velocity */
		    0.0,								/* volume flow */
		    0.0);								/* temp */
	}
#if defined (WIN32) || defined (_WIN32)
    ReleaseMutex(_thor_mutex);
#endif

    return 0;
}


/* key press handler */
DWORD WINAPI _thor_key_handler(LPVOID obj)
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
	    if(_ctrl_ix == THOR_QUIT_CODE1 || _ctrl_ix == THOR_QUIT_CODE2)
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
 		case THOR_ACT_INCR_CODE:
		    _thor_increment_act();
		    break;
		case THOR_ACT_DECR_CODE:
		    _thor_decrement_act();
		    break;
		}
#if defined (WIN32) || defined (_WIN32)
	    ReleaseMutex(_thor_mutex);
#endif
	}
    return 0;
}

/* increment actuator position */
static int _thor_increment_act()
{
    if(_thor_act_pos<THOR_ACT_CEIL-THOR_ACT_INCR)
	_thor_act_pos += THOR_ACT_INCR;
    sprintf(_thor_optmsg_buff, THOR_ACTUATOR_PST, _thor_act_pos);
    _thor_msg_cnt = THOR_MSG_DURATION;    
    _thor_update_msg_buff(_thor_msg_buff, _thor_optmsg_buff);
    return 0;
}

/* decrement acturator position */
static int _thor_decrement_act()
{
    if(_thor_act_pos>THOR_ACT_FLOOR-THOR_ACT_INCR)
	_thor_act_pos -= THOR_ACT_INCR;
    sprintf(_thor_optmsg_buff, THOR_ACTUATOR_PST, _thor_act_pos);
    _thor_msg_cnt = THOR_MSG_DURATION;
    _thor_update_msg_buff(_thor_msg_buff, _thor_optmsg_buff);
    return 0;
}
