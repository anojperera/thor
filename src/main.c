/* test program for testing system class */
#include <stdlib.h>
#include <unistd.h>
#include "thsys.h"
#define MAX_TIME 10
int main(int argc, char** argv)
{
    int count = 0;
    thsys sys;

    if(thsys_init(&sys, NULL))
	{
	    printf("%s\n","sys not initialised");
	    return -1;
	}

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
    printf("\nEnd\n");
    thsys_stop(&sys);
    thsys_delete(&sys);
    return 0;
}
