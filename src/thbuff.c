/* Implementation of buffer */
/* Tue Aug 30 15:20:50 GMTDT 2011 */

#include <stdio.h>
#include "thbuff.h"

/* Internal buffer variable */
static thbuff* var_thbuff = NULL;

int thbuff_new(size_t num, thbuff** obj)
{
    if(num <= 0)
	return 1;

    var_thbuff = (thbuff*) malloc(sizeof(thbuff));

    if(!var_thbuff)
	{
	    printf("thbuff_new : %s\n","Unable to create struct");
	    return 1;
	}

    /* create buffer array */
    var_thbuff->var_buff = (double*) calloc(num, sizeof(double));
    
    if(!var_thbuff->var_buff)
	{
	    printf("thbuff_new: %s\n", "unable to create array");
	    free(var_thbuff);
	    var_thbuff = NULL;
	    return 1;
	}

    /* initialise array and variables */
    int i;
    for(i=0; i<num; i++)
	var_thbuff->var_buff[i] = 0.0;

    var_thbuff->var_ix = 0;
    var_thbuff->var_count = num;
    var_thbuff->var_avg = 0.0;
    var_thbuff->var_stdev = 0.0;

    if(obj)
	*obj = var_thbuff;
    
    return 0;
}

/* Delete buffer */
int thbuff_delete(thbuff* obj)
{
    /* check for internal object */
    if(!var_thbuff)
	return 1;

    if(var_thbuff->var_buff)
	{
	    free(var_thbuff->var_buff);
	    var_thbuff->var_buff = NULL;
	}

    free(var_thbuff);
    var_thbuff = NULL;
    obj = NULL;

    return 0;
}

/* Calculate average in buffer */
inline double thbuff_add_new(thbuff* obj, double val)
{
    static int i = 0;
    static double cnt = 1.0;
    if(!var_thbuff)
	return 0.0;

    var_thbuff->var_buff[var_thbuff->var_ix++] = val;

    if(var_thbuff->var_ix >= var_thbuff->var_count)
	var_thbuff->var_ix = 0;

    var_thbuff->var_avg = 0.0;
    cnt = 1.0;
    for(i=0; i<var_thbuff->var_count; i++)
	{
	    if(var_thbuff->var_buff[i] > 0.0)
		{
		    var_thbuff->var_avg +=
			var_thbuff->var_buff[i];
		    cnt++;
		}
	}

    var_thbuff->var_avg = var_thbuff->var_avg / cnt;
    
    return var_thbuff->var_avg;
}
