/* test program for testing system class */
#include <stdlib.h>
#include <unistd.h>
#include "thsys.h"

#define MAX_TIME 2

static int _update(thsys* obj, void* _ext_obj, const float64* buff, const int sz);

int main(int argc, char** argv)
{
    int count = 0;
    thsys sys;

    if(thsys_init(&sys, NULL))
	{
	    printf("%s\n","sys not initialised");
	    return -1;
	}

    openlog("thor_sys", LOG_PID, LOG_USER);
    sys.var_callback_update = _update;
    thsys_set_sample_rate(&sys, 4);
    thsys_start(&sys);
    printf("\n");
    while(1)
	{
	    fprintf(stderr, "count: %i\r", count);
	    sleep(1);
	    if(count++ > MAX_TIME)
		break;
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
    fprintf(stderr, "\n%f\t", buff[i]);
  fprintf(stderr, "\n=========================\n");
  return 0;
}
