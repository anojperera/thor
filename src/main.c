/* test program for testing system class */
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "thsys.h"

#define MAX_TIME 30



static int _update(thsys* obj, void* _ext_obj, const float64* buff, const int sz);
static void _sig_handler(int signo);

static thsys sys;
int main(int argc, char** argv)
{
    int count = 0;


    if(thsys_init(&sys, NULL))
	{
	    printf("%s\n","sys not initialised");
	    return -1;
	}

    openlog("thor_sys", LOG_PID, LOG_USER);
    sys.var_callback_update = _update;
    signal(SIGINT, _sig_handler);
    thsys_set_sample_rate(&sys, 4);
    thsys_start(&sys);
    printf("\n");
    while(1)
	{
	    sleep(10);
	}


    thsys_stop(&sys);
    thsys_delete(&sys);
    closelog();

    printf("\nEnd\n");    
    return 0;
}


static int _update(thsys* obj, void* _ext_obj, const float64* buff, const int sz)
{
  int i = 0;
  for(i=0; i<sz; i++)
    fprintf(stderr, "%f\t", buff[i]);
  fprintf(stderr, "\r");
  fflush(stderr);
  return 0;
}

static void _sig_handler(int signo)
{
    if(signo == SIGINT)
	{
	    thsys_stop(&sys);
	    thsys_delete(&sys);
	    closelog();
	    printf("\nEnd\n");    	    
	}
    return;
}
