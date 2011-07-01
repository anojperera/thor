/* Implementation of generic sensor object.
 * Sat May 14 14:47:30 BST 2011 */

#include "thgsens.h"
#include <string.h>

#define THGSENS_MIN_VOLT 0.0			/* min voltage of sensor */
#define THGSENS_MAX_VOLT 10.0			/* max voltage of sensor */

/* private functions */

/* calculate gradient of equation */
static inline int thgsens_calc_grad(thgsens** obj)
{
    (*obj)->var_grad = ((*obj)->var_max - (*obj)->var_min) /
	(THGSENS_MAX_VOLT - THGSENS_MIN_VOLT);
    return 1;
}

/* calculate y interception */
static inline int thgsens_cacl_intercept(thgsens** obj)
{
    (*obj)->var_intc = (*obj)->var_min - (*obj)->var_grad *
	THGSENS_MIN_VOLT;
    return 1;
}

/* calculate value from raw data */
static inline int thgsens_calc_value(thgsens** obj)
{
    if((*obj)->var_raw <=0.0 || (*obj)->var_raw > 10.0)
	(*obj)->var_val = 0.0;
    else
	{
	    (*obj)->var_val = (*obj)->var_grad * (double) (*obj)->var_raw +
		(*obj)->var_intc;
	}

    return 1;
}

/* initialise sensor by creating channel specified in
 * constructor. Assumes default voltage range shall be
 * 0 - 10v */
static inline int thgsens_init(thgsens* obj)
{
  if(!obj)
    {
      printf("%s\n","initialisation failed");
      return 0;
    }

  if(!obj->var_ch_name)
    {
      printf("%s\n","initialisation failed");
      return 0;
    }

  int32 err_code;		/* error code */
  char err_msg[256];		/* error message */
    
  /* create analog input channel specified in
   * constructor */
  err_code =
    NICreateAIVoltageChan(*obj->var_task,	/* task handle */
			  obj->var_ch_name,	/* channel name */
			  NULL,			/* name of channel set to default */
			  DAQmx_Val_NRSE,	/* configuration set to referenced */
			  THGSENS_MIN_VOLT,	/* minimum voltage */
			  THGSENS_MAX_VOLT,	/* maximum voltage */
			  DAQmx_Val_Volts,	/* measurement in volts */
			  NULL);			/* custom scaling */

  /* check for errors */
  if(err_code)
    {
#if defined(WIN32) || defined(_WIN32)
      NIGetErrorString(err_code, err_msg, 256);
#else
      NIGetErrorString(err_msg, 256);
#endif
      fprintf(stderr, "%s\n", err_msg);
      return 0;
    }


  return 1;
}

/********************************************************************************/
/* constructor */
int thgsens_new(thgsens** obj, const char* ch_name,
		TaskHandle* task, gthsen_fptr fptr, void* data)
{
    /* check for args and return success or failure */
    if(!obj || !ch_name || !task)
	return 0;

    /* create struct */
    *obj = (thgsens*) malloc(sizeof(thgsens));

    if(!(*obj))
	return 0;
    
    /* set properties */
    (*obj)->var_ch_name =
	(char*) malloc(strlen(ch_name) + sizeof(char));
    strcpy((*obj)->var_ch_name, ch_name);

    (*obj)->var_task = task;
    (*obj)->var_update = fptr;

    (*obj)->var_min = 0;
    (*obj)->var_max = 0;

    (*obj)->var_val = 0;
    (*obj)->var_raw = 0;
    (*obj)->var_termflg = 0;
    (*obj)->var_grad = 0;
    (*obj)->var_intc = 0;
    (*obj)->var_okflg = 0;
    (*obj)->sobj_ptr = data;

    printf("%s\n","sensor parameters assigned");
    thgsens_init(*obj);

    /* sensor constructor compelete */
    printf("%s\n","sensor constructor complete");

    return 1;
}


/* destructor */
void thgsens_delete(thgsens** obj)
{
    if(!obj || !(*obj))
	return;

    (*obj)->sobj_ptr = NULL;
    if((*obj)->var_ch_name)
	free((*obj)->var_ch_name);
    (*obj)->var_ch_name = NULL;
    (*obj)->var_task = NULL;
    (*obj)->var_update = NULL;

    free(*obj);
    *obj = NULL;
}

/* Set range
 * when range is set call to calulate gradient and y interception */
inline int thgsens_set_range(thgsens* obj, double min, double max)
{
    if(!obj || min > max)
	return 0;
    obj->var_min = min;
    obj->var_max = max;
    

    thgsens_calc_grad(&obj);
    thgsens_cacl_intercept(&obj);

    /* set flag to indicate min and max ranges are set */
    obj->var_okflg = 1;

    return 1;
}

/* get current value */
inline double thgsens_get_value(thgsens* obj)
{
    if(!obj)
	return 0;
    else
	{
	    thgsens_calc_value(&obj);
	    
	    /* if function pointer to update external
	     * value was set call the function pointer */
	    if(obj->var_update)
		obj->var_update(obj->sobj_ptr, &obj->var_val);


	    
	    return obj->var_val;
	}
}

/* reset internal value */
inline int thgsens_reset_all(thgsens* obj)
{
    if(!obj)
	return 0;

    obj->var_min = 0;
    obj->var_max = 0;
    obj->var_val = 0;
    obj->var_grad = 0;
    obj->var_intc = 0;

    return 1;
}


/* reset value */
inline int thgsens_reset_value(thgsens* obj)
{
    if(!obj)
	return 0;

    obj->var_val = 0;
    return 1;
}



