/* Implementation of pid controller */

#include "thpid.h"

#define THPID_MIN 0.0			/* Minimum and maximum */
#define THPID_MAX 10.0
#define THPID_DIFF_MAX 1.0

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
    obj->var_diff_max = THPID_DIFF_MAX;

    obj->var_lim_min = THPID_MIN;
    obj->var_lim_max = THPID_MAX;

    obj->var_eqtype = theq_linear;

    obj->var_raw_flg = 1;

    return 0;
}

/* delete struct */
void thpid_delete(struct thpid* obj)
{
    free(obj);
    obj = NULL;
    return;
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
	{
	    switch(obj->var_eqtype)
		{
		case theq_linear:
		    y_out = (res - obj->var_c) / obj->var_m;
		    break;
		case theq_log:
		    y_out = exp((log(res) - log(obj->var_m)) /
				obj->var_c);
		    break;
		}
	}

    obj->var_err = set - y_out;				/* error */
    printf("PID Error%f\n", obj->var_err);
    /* calculate output */
    switch(obj->var_eqtype)
	{
	case theq_linear:
	    obj->var_out = obj->var_err * obj->var_m +
		obj->var_c;
	    break;
	case theq_log:
	    obj->var_out = obj->var_err *
		pow(obj->var_m, obj->var_c);
	    break;
	}

    printf("Corrected Voltage %f\n",obj->var_out);
    /* limit the output within limits */
    if(obj->var_out < THPID_MIN)
	obj->var_out = THPID_MIN;
    else if(obj->var_out > THPID_MAX)
	obj->var_out = THPID_MAX;

    if(out)
	*out += obj->var_out;

    
    return 0;
}

/* PID Control 2 */
inline int thpid_pid_control2(struct thpid* obj,	/* object */
			      double set,		/* set point */
			      double res,		/* response */
			      double out_prev,		/* output previous */
			      double** out,		/* array */
			      unsigned int* sz)		/* array size */
{
    double tmp_val = 0.0;				/* temporary value */
    unsigned int i = 0;

    if(thpid_pid_control(obj, set, res, &tmp_val))
	{
	    printf("thpid_pid_control2: %s\n","Error calculating PID");
	    return 1;
	}

    /* printf("previous and current %f %f\n",out_prev, tmp_val); */
    *sz = 10 * (unsigned int) ceil(fabs(tmp_val));
    if(*sz <= 0)
	*sz = 1;
    
    /* check if out buffer pointer was allocated before
     * and delete */
    if(*out != NULL)
	free(*out);

    /* create out buffer */
    *out = (double*)
	calloc(*sz, sizeof(double));

    if(*out == NULL)
	{
	    printf("thpid_pid_control2: %s\n", "Unable to create array");
	    return 1;
	}
    printf("size %i\n", *sz);
    /* compare output from previous and now */
    if(*sz > obj->var_diff_max)
	{
	    for(i = 0;
		i < *sz;
		i++)
		{
		    (*out)[i] = out_prev + tmp_val *
			sin(M_PI * (double) i / (2 * (double) *sz));
		    /* printf("PID %f\n",(*out)[i]); */
		}
	}
    else
	(*out)[i] = out_prev;

    return 0;
}

/* Set equation type */
inline int thpid_set_equation_type(struct thpid* obj,
				   theq_type var)
{
    if(!obj)
	return 1;

    obj->var_eqtype = var;
}
