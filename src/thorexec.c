/* Main executable program for the thor test suit */
/* 22/01/2013 */
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "thorexec.h"
#define PROG_MSG "\nSelect Below Option: \nAHU Performance Test [1]\nDamper Close Test [2]\nOption [1/2]: "
int main(int argc, char** argv)
{
    int _next_opt;
    int _opt_st_flg = 0;
    int _prog_id = 0;
    /* short options */
    const char* const _short_opts = "a:d:";
    const struct option _long_opts[] = {
	{"ahu", 0, NULL, 'a'},
	{"damper", 0, NULL, 'd'},
	{NULL, 0, NULL, 0}
    };
    
    /* parse through arguments */
    do
	{
	    _next_opt = getopt_long(argc, argv, _short_opts, _long_opts, NULL);
	    switch(_next_opt)
		{
		case 'a':
		    _prog_id = 1;
		    _opt_st_flg = 1;
		    break;
		case 'd':
		    _prog_id = 2;
		    _opt_st_flg = 1;
		    break;
		case -1:
		default:
		    printf(PROG_MSG);
		    scanf("%i", &_prog_id);
		    break;
		}
	    if(_opt_st_flg)
		break;
	}while(_next_opt != -1);

    /* execute program */
    switch(_prog_id)
	{
	case 1:
	    thorahuexec_main(argc, argv);
	    break;
	case 2:
	    thoractstexec_main(argc, argv);
	    break;
	default:
	    fprintf(stderr, "\nInvalid Option\n");
	}
    return 0;
}
