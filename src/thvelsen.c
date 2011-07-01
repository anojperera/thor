/* Implementation of velocity sensor.
 * Mon Jun 20 17:17:55 GMTDT 2011 */

#include <stdio.h>
#include <math.h>
#include "thvelsen.h"

#define THVELSEN_AIR_DENSITY 1.2
#define THVELSEN_VELOCITY(val)\
    (pow((2 * val) / THVELSEN_AIR_DENSITY, 0.5))

/* Constructor */
int thvelsen_new(thvelsen** obj,
		 TaskHandle* task,		/* task */
		 gthsen_fptr fptr,		/* function pointer */
		 void* data)			/* external data */
{
    if(!obj || !task)
	return 0;
    
    /* create struct */
    *obj = (thvelsen*) malloc(sizeof(thvelsen));
    if(!(*obj))
	{
	    /* unable to create velocity sensors */
	    printf("%s\n","unable to create velocity sensors");
	    return 0;
	}

    (*obj)->var_ave_vel = 0.0;
    (*obj)->var_task = task;
    (*obj)->var_fptr = (fptr? fptr : NULL);
    (*obj)->var_sen_cnt = 0;

    (*obj)->var_v1 = NULL;
    (*obj)->var_v2 = NULL;
    (*obj)->var_v3 = NULL;
    (*obj)->var_v4 = NULL;

    (*obj)->var_v1_flg = 0;
    (*obj)->var_v2_flg = 0;
    (*obj)->var_v3_flg = 0;
    (*obj)->var_v4_flg = 0;

    (*obj)->var_sobj = data;

    (*obj)->var_okflg = 0;

    return 1;
}

/* destructor */
void thvelsen_delete(thvelsen** obj)
{
    if(!obj || !(*obj))
	return;

    (*obj)->var_task = NULL;
    (*obj)->var_fptr = NULL;

    thgsens_delete(&(*obj)->var_v1);
    thgsens_delete(&(*obj)->var_v2);
    thgsens_delete(&(*obj)->var_v3);
    thgsens_delete(&(*obj)->var_v4);

    return;
}

/* configure sensor */
int thvelsen_config_sensor(thvelsen* obj,		/* object */
			   unsigned int ix,		/* index */
			   const char* ch_name,		/* channel name */
			   gthsen_fptr fptr,
			   double min,			/* minimum range */
			   double max)			/* maximum range */
{
  if(!obj || !ch_name)
    {
      printf("%s\n","unable to configure sensor");
      return 0;
    }
    
  switch(ix)
    {
    case 0:
      printf("%s\n","v0 sensor");
      if(thgsens_new(&obj->var_v1,
		     ch_name,
		     obj->var_task,
		     (fptr? fptr : obj->var_fptr),
		     obj->var_sobj))
	{
	  thgsens_set_range(obj->var_v1, min, max);
	  obj->var_v1_flg = 1;
	  printf("%s %i %s\n","velocity sensor", ix, "complete");
	}
      else
	printf("%s\n","unable to create velocity sensor 1");
	    
      break;

    case 1:
      printf("%s\n","v1 sensor");
      if(thgsens_new(&obj->var_v2,
		     ch_name,
		     obj->var_task,
		     (fptr? fptr : obj->var_fptr),
		     obj->var_sobj))
	{
	  thgsens_set_range(obj->var_v2, min, max);
	  obj->var_v2_flg = 1;
	  printf("%s %i %s\n","velocity sensor", ix, "complete");
	}
      else
	printf("%s\n","unable to create velocity sensor 2");
	    
      break;

    case 2:
      printf("%s\n","v2 sensor");
      if(thgsens_new(&obj->var_v3,
		     ch_name,
		     obj->var_task,
		     (fptr? fptr : obj->var_fptr),
		     obj->var_sobj))
	{
	  thgsens_set_range(obj->var_v3, min, max);
	  obj->var_v3_flg = 1;
	  printf("%s %i %s\n","velocity sensor", ix, "complete");		    
	}
      else
	printf("%s\n","unable to create velocity sensor 3");
	    
      break;

    default:
      printf("%s\n","v3 sensor");
      if(thgsens_new(&obj->var_v4,
		     ch_name,
		     obj->var_task,
		     (fptr? fptr : obj->var_fptr),
		     obj->var_sobj))
	{
	  thgsens_set_range(obj->var_v4, min, max);
	  obj->var_v4_flg = 1;
	  printf("%s %i %s\n","velocity sensor", ix, "complete");		    
	}
      else
	printf("%s\n","unable to create velocity sensor 4");
    }

  if(obj->var_sen_cnt < 3)
    obj->var_sen_cnt++;

  /* check for ok flags */
  if(obj->var_v1 && obj->var_v2 && obj->var_v3 && obj->var_v4)
    {
      if(obj->var_v1->var_okflg &&
	 obj->var_v2->var_okflg &&
	 obj->var_v3->var_okflg &&
	 obj->var_v4->var_okflg)
	obj->var_okflg = 1;
    }
    
  return 1;
}

int thvelsen_disable_sensor(thvelsen* obj,
				unsigned int ix)
{
    if(!obj || ix > 3)
	return 0;

    switch(ix)
	{
	case 0:
	    thgsens_delete(&obj->var_v1);
	    break;
	case 1:
	    thgsens_delete(&obj->var_v2);
	    break;
	case 2:
	    thgsens_delete(&obj->var_v3);
	    break;
	default:
	    thgsens_delete(&obj->var_v4);
	}

    /* decrement counter */
    if(obj->var_sen_cnt > 0)
	obj->var_sen_cnt--;
    
    return 1;
}


/* get velocity */
double thvelsen_get_velocity(thvelsen* obj)
{
    if(!obj)
	return 0;

    double v1=0.0, v2=0.0, v3=0.0, v4=0.0;

    if(obj->var_v1)
	{
	    v1 = THVELSEN_VELOCITY(thgsens_get_value(obj->var_v1));
	}

    if(obj->var_v2)
	{
	    v2 = THVELSEN_VELOCITY(thgsens_get_value(obj->var_v2));
	}

    if(obj->var_v3)
	{
	    v3 = THVELSEN_VELOCITY(thgsens_get_value(obj->var_v3));
	}

    if(obj->var_v4)
	{
	    v4 = THVELSEN_VELOCITY(thgsens_get_value(obj->var_v4));
	}

    obj->var_ave_vel =
	(v1 + v2 + v3 + v4) / (double) obj->var_sen_cnt;

    /* if the callback function pointer was assigned
     * call to update external widget */
    if(obj->var_fptr)
	obj->var_fptr(obj->var_sobj, &obj->var_ave_vel);
    
    return obj->var_ave_vel;
}
