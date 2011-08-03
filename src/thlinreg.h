/* Log linear regression to obtain gradient of equation
 * and y intercept. This is assuming all points have a
 * linear relationship. Typical use of the implementation
 * shall be on AHU test program. On initial calibration,
 * the damper is closed and static pressure is recorded for
 * each point. This data shall be used to work out the equation
 * of the line. The equation shall then be used with the PID
 * controller.
 *
 *
 * Wed Aug  3 10:25:56 GMTDT 2011 */

#ifndef __THLINREG_H__
#define __THLINREG_H__

#include <stdlib.h>
#include <math.h>
#include <alist.h>

/* Equation struct */
typedef struct _theq theq;

/* main xy struct */
struct thxy
{
    double x;
    double y;
    double x2;
    double xy;
};

struct _theq
{
    aNode* var_list;
    double var_m;
    double var_c;
    double var_r;
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor;
     * Allocates memory for object pointers */
    extern inline int thlinreg_new(theq** eq_obj, struct thxy** xy_obj);

    /* Destructor */
    void thlinreg_delete(theq** eq_obj);

    /* Calculates equation parameters.
     * Optionally pass gradient and intercept of equation to be returned with
     * values */
    int thlinreg_calc_equation(theq* eq_obj, double* m, double* c, double* r);
    
#ifdef __cplusplus
}
#endif

#endif /* __THLINREG_H__ */

