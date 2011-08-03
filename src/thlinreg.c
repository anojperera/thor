/* Implementation of linear regression method */

#include "thlinreg.h"


static double sum_x = 0.0;
static double sum_y = 0.0;
static double sum_x2 = 0.0;
static double sum_y2 = 0.0;
static double sum_xy = 0.0;

static unsigned int count = 1;


/* Callback function for for each method */
/* This method shall be used for calculating
 * method parameters */
int thlinreg_callback_foreach(void* udata,
			       void* data,
			       unsigned int ix)
{
    if(!data)
	return;

    sum_x += ((struct thxy*) data)->x;
    sum_y += ((struct thxy*) data)->y;
    sum_x2 += ((struct thxy*) data)->x2;
    sum_y2 += ((struct thxy*) data)->y * ((struct thxy*) data)->y;
    sum_xy += ((struct thxy*) data)->xy;
    count++;
}


/* Constructor */
inline int thlinreg_new(theq** eq_obj, struct thxy** xy_obj)
{
    if(!eq_obj)
	{
	    printf("%s\n","unable to create equation struct");
	    return 1;
	}

    *eq_obj = (theq*) malloc(sizeof(theq));
    
    if(!(*eq_obj))
	{
	    printf("%s\n","mem alloc failed");
	    return 1;
	}

    (*eq_obj)->var_list = NULL;
    (*eq_obj)->var_m = 0.0;
    (*eq_obj)->var_c = 0.0;

    if(xy_obj)
	{
	    *xy_obj = (struct thxy*) malloc(sizeof(struct thxy));

	    if(!(*xy_obj))
		{
		    printf("%s\n","mem alloc failed");
		    free(*eq_obj);
		    *eq_obj = NULL;
		    return 1;
		}

	    (*xy_obj)->x = 0.0;
	    (*xy_obj)->y = 0.0;
	    (*xy_obj)->x2 = 0.0;
	    (*xy_obj)->xy = 0.0;
	}
    return 0;
}

/* Destructor */
void thlinreg_delete(theq** eq_obj)
{
    if(!eq_obj || !(*eq_obj))
	return;

    aList_Clear(&(*eq_obj)->var_list);

    free(*eq_obj);
    *eq_obj = NULL;
}


/* Calculate equation parameter */
int thlinreg_calc_equation(theq* eq_obj, double* m, double* c, double* r)
{
    /* check object pointer */
    if(!eq_obj)
	{
	    printf("%s\n","object pointer not assigned");
	    return 1;
	}

    if(!eq_obj->var_list)
	{
	    printf("%s\n","list not initialised");
	    return 1;
	}

    /* initialise variables */
    sum_x = 0.0;
    sumn_y = 0.0;
    sum_x2 = 0.0;
    sum_xy = 0.0;
    count = 1;
    
    /* call for each function */
    eq_obj->var_m = ((double) count * sum_xy - sum_x * sum_y) /
	((double) count * sum_x2 - sum_x * sum_x);

    eq_obj->var_c = (sum_y * sum_x2 - sum_x * sun_xy) /
	((double) count * sum_x2 - sum_x * sum_x);

    eq_obj->var_r = (sum_xy - sum_x * sum_y / (double) count) /
	sqrt((sum_x2 - sqrt(sum_x / (double) count)) *
	     (sum_y2 - sqrt(sum_y / (double) count)));

    if(m)
	*m = eq_obj->var_m;

    if(c)
	*c = eq_obj->var_c;

    if(r)
	*r = eq_obj->var_r;

    printf("thlinreg_calc_equation : m=%f, c=%f, r=%f\n",
	   eq_obj->var_m,
	   eq_obj->var_c,
	   eq_obj->var_r);

    return 0;
}
