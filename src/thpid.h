/* Generic PID controller */

#ifndef __THPID_H__
#define __THPID_H__

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

struct thpid
{
    double var_set;		/* set point */
    double var_res;		/* response */
    double var_out;		/* ouput */
    double var_kp;		/* Kp - proportional */
    double var_kd;		/* Kd - derivative */
    double var_ki;		/* Ki - integral */
    double var_err;		/* error */
    double var_m;		/* gradient */
    double var_c;		/* y intercept */

    double var_lim_min;		/* minimum */
    double var_lim_max;		/* maximum */

    int var_raw_flg;		/* flag to indicate feedback
				 * is raw static value instead of
				 * voltage */
};

#ifdef __cplusplus
extern "C" {
#endif

    /* initialise function */
    int thpid_init(struct thpid* obj);

    /* delete pid */
    void thpid_delete(struct thpid* obj);

    /* Produce output based on m and c */
    extern inline int thpid_pid_control(struct thpid* obj,		/* object */
					double set,			/* set point */
					double res,			/* response */
					double* out);			/* ouput */

#ifdef __cplusplus
}
#endif

#endif /* __THPID_H__ */
