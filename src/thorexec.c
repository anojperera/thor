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

#define THOR_QUIT_CODE1 81
#define THOR_QUIT_CODE2 113

#define THOR_MSG_BUFFER_SZ 2048

#define THOR_WAIT_TIME 250							/* waiting time for the main loop */
#define THOR_WAIT_TIME_UNIX_CORRECTION 1000					/* correction for unix */

static int _ctrl_ix;								/* control flag */
static int _quit_flg = 1;							/* quit flag */
static char _thor_msg_buff[THOR_MSG_BUFFER_SZ];					/* main message buffer */

/* Control methods */
/* static int _thor_ahu_init(void); */
static int _thor_update_msg_buff(char* buff);
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
	    _thor_update_msg_buff(_thor_msg_buff);
	    fflush(stdout);
	    fprintf(stdout, "%s\r", _thor_msg_buff);
#if defined (WIN32) || defined (_WIN32)
	    Sleep(THOR_WAIT_TIME);
#else
	    nanosleep(&t, NULL);
#endif
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
static int _thor_update_msg_buff(char* buff)
{
    /* lock mutex */
#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_thor_mutex, INFINITE);
#endif
    sprintf(buff, "%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\r",
	    0.0,								/* DP 1 */
	    0.0,								/* DP 2 */
	    0.0,								/* DP 3 */
	    0.0,								/* DP 4 */
	    0.0,								/* static */
	    0.0,								/* velocity */
	    0.0,								/* volume flow */
	    0.0);								/* temp */
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
	}
    return 0;
}
