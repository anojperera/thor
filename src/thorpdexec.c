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
#define THOR_PD_MSG_BUFFER_SZ 8

#define THOR_PD_INIT_PROG 105								/* i */
#define THOR_PD_QUIT_CODE1 81								/* Q */
#define THOR_PD_QUIT_CODE2 113								/* q */
#define THOR_PD_PRG_START_CODE 83							/* S */
#define THOR_PD_PRG_STOP_CODE 115							/* s */

#define THOR_PD_WAIT_TIME 500								/* waiting time for the main loop */
#define THOR_PD_WAIT_TIME_UNIX_CORRECTION 1000						/* correction for unix */
static unsigned int _init_flg = 0;							/* initialise flag */
static unsigned int _start_flg = 0;							/* start test flag */
static char _thor_msg_buff[THOR_PD_MSG_BUFFER_SZ];					/* main message buffer */
static double _duct_dia = THOR_PD_DUCT_DIA;						/* duct diameter */
static doulbe _d_width = THOR_PD_D_WIDTH;						/* damper width */
static double _d_height = THOR_PD_D_HEIGHT;						/* damper height */
static FILE* _thor_result_fp = NULL;							/* result file pointer */

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

    printf ("V_DP1\tV_DP2\tDP1\t\DP2\t\Vel\tVol\tDP\tTemp\n");
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
	thpd_stop(NULL);

    /* delete pressure drop program */
    thpd_delete(NULL);

#if defined (WIN32) || defined (_WIN32)
    WaitForSingleObject(_thhandle, INFINITE);
    CloseHandle(_thhandle);
    CloseHandle(_thor_mutex);
#endif
    return 0;
}

/* initialise variables */
static int _thor_init(void)
{
    int i=0;
    for(i=0; i<THOR_PD_MSG_BUFFER_SZ; i++)
	_thor_msg_buff[i] = 0.0;
    return 0;
}

/* initialise pressure drop test */
static int _thor_pd_init(void)
{

}
