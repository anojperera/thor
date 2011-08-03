/* Implementation of pid controller */

#include "thpid.h"

#define THPID_MIN 0.0			/* Minimum and maximum */
#define THPID_MAX 10.0

/* initialise function */
int thpid_init(struct thpid* obj)
{
    if(!obj)
	{
	    printf("thpid_init : %s\n","missing object pointer");
	    return 1;
	}

    obj = (struct thpid*) malloc(sizeof(struct thpid));
    if(!obj)
	{
	    printf("thpid_init : %s\n","malloc failed");
	    return 1;
	}

    obj->var_set = 0.0;
    obj->var_res = 0.0;
    obj->var_out = 0.0;
    obj->var_kp = 1;
    obj->var_kd = 1;
    obj->var_ki = 1;
    obj->var_err = 0;
    obj->var_m = 0;
    obj->var_c = 0;

    obj->var_lim_min = THPID_MIN;
    obj->var_lim_max = THPID_MAX;

    obj->var_raw_flg = 1;

    return 1;
}

/* delete struct */
void thpid_delete(struct thpid* obj)
{
    free(obj);
    obj = NULL;
    return 0;
}

/* PID control */
inline int thpid_pid_control(struct thpid* obj,		/* object */
			     double set,		/* set point */
			     double res,		/* response */
			     double* out)		/* ouput */
{
    static double y_out = 0.0;				/* converted output */
    
    if(!obj)
	return 1;

    if(obj->var_raw_flg > 0)
	y_out = res;
    else
	y_out = (res - obj->var_c) / obj->var_m;

    obj->var_err = set - y_out;				/* error */

    /* calculate output */
    obj->var_out = (res + obj->var_err) * obj->var_m +
	obj->var_c;

    /* limit the output within limits */
    if(obj->var_out < THPID_MIN)
	obj->var_out = THPID_MIN;

    if(obj->var_out > THPID_MAX)
	obj->var_out = THPID_MAX;

    if(out)
	*out = obj->var_out;

    return 0;
}
