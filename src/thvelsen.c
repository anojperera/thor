/* Implementation of velocity sensor.
 * Mon Jun 20 17:17:55 GMTDT 2011 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "thvelsen.h"

#define THVELSEN_GMIN 0.0
#define THVELSEN_GMAX 1600.0
#define THVELSEN_AIR_DENSITY 1.2
#define THVELSEN_VELOCITY(val)						\
    (val<0.0? 0.0 : pow(((2 * val) / THVELSEN_AIR_DENSITY), 0.5))

/* channel name macro */
#define THVELSET_CPYSTR(ch_ptr, channel_name) \
    (*ch_ptr) = (char*) malloc(sizeof(char) * (strlen(channel_name) + 1)); \
     strcpy(*ch_ptr, channel_name);

static const double _v1_cal_x[] = THORNIFIX_S1_X;
static const double _v1_cal_y[] = THORNIFIX_S1_Y;

static const double _v2_cal_x[] = THORNIFIX_S2_X;
static const double _v2_cal_y[] = THORNIFIX_S2_Y;

static const double _v3_cal_x[] = THORNIFIX_S3_X;
static const double _v3_cal_y[] = THORNIFIX_S3_Y;

static const double _v4_cal_x[] = THORNIFIX_S4_X;
static const double _v4_cal_y[] = THORNIFIX_S4_Y;

static const double _v1_pcal_x[] = THORNIFIX_P1_X;
static const double _v1_pcal_y[] = THORNIFIX_P1_Y;

static const double _v2_pcal_x[] = THORNIFIX_P2_X;
static const double _v2_pcal_y[] = THORNIFIX_P2_Y;

static const double _v3_pcal_x[] = THORNIFIX_P3_X;
static const double _v3_pcal_y[] = THORNIFIX_P3_Y;

static const double _v4_pcal_x[] = THORNIFIX_P4_X;
static const double _v4_pcal_y[] = THORNIFIX_P4_Y;

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

    (*obj)->var_gmin = THVELSEN_GMIN;
    (*obj)->var_gmax = THVELSEN_GMAX;

    (*obj)->var_ch1 = NULL;
    (*obj)->var_ch2 = NULL;
    (*obj)->var_ch3 = NULL;
    (*obj)->var_ch4 = NULL;

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

    /* delete channel names */
    /************************/
    if((*obj)->var_ch1)
	{
	    free((*obj)->var_ch1);
	    (*obj)->var_ch1 = NULL;
	}

    if((*obj)->var_ch2)
	{
	    free((*obj)->var_ch2);
	    (*obj)->var_ch2 = NULL;
	}

    if((*obj)->var_ch3)
	{
	    free((*obj)->var_ch3);
	    (*obj)->var_ch3 = NULL;
	}

    if((*obj)->var_ch4)
	{
	    free((*obj)->var_ch4);
	    (*obj)->var_ch4 = NULL;
	}

    /************************/
    thgsens_delete(&(*obj)->var_v1);
    thgsens_delete(&(*obj)->var_v2);
    thgsens_delete(&(*obj)->var_v3);
    thgsens_delete(&(*obj)->var_v4);

    (*obj)->var_fptr_s1 = NULL;
    (*obj)->var_fptr_s2 = NULL;
    (*obj)->var_fptr_s3 = NULL;
    (*obj)->var_fptr_s4 = NULL;

    free(*obj);
    *obj = NULL;

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
    if(!obj)
	{
	    printf("%s\n","unable to configure sensor");
	    return 1;
	}

    int ptr_flg=0, ch_flg=0, rng_flg=0;

    /* evaluale channel name argument */
    if(obj->var_okflg && ch_name==NULL)
	ch_flg = 1;
    else if(obj->var_okflg==0 && ch_name==NULL)
	{
	    printf("%s %i %s\n", "channel name not specified for", ix, "sensor");
	    return 1;
	}

    /* evaluate function pointer argument */
    if(obj->var_okflg && fptr==NULL)
	ptr_flg = 1;

    /* evaluate range argument */
    if(obj->var_okflg && min==0.0 && max==0.0)
	rng_flg = 1;

    switch(ix)
	{
	case 0:
	    printf("%s\n","v0 sensor");
	    if(thgsens_new(&obj->var_v1,
			   (ch_flg? obj->var_ch1 : ch_name),
			   obj->var_task,
			   (ptr_flg? obj->var_fptr_s1 : fptr),
			   obj->var_sobj))
		{
		    thgsens_set_range(obj->var_v1,
		    		      (rng_flg>0? obj->var_gmin : min),
		    		      (rng_flg>0? obj->var_gmax : max));
		    obj->var_v1_flg = 1;
		    printf("%s %i %s\n","velocity sensor", ix, "complete");

		    /* copy string and generic min and max val */
		    THVELSET_CPYSTR(&obj->var_ch1, ch_name)
			obj->var_gmin = min;
		    obj->var_gmax = max;
		    thgsens_set_calibration_buffers(obj->var_v1, _v1_cal_x, _v1_cal_y, THORNIFIX_S_CAL_SZ);
		}
	    else
		{
		    printf("%s\n","unable to create velocity sensor 1");
		    obj->var_okflg = 0;
		}

	    /* set calibration */

	    break;

	case 1:
	    printf("%s\n","v1 sensor");
	    if(thgsens_new(&obj->var_v2,
			   (ch_flg? obj->var_ch2 : ch_name),
			   obj->var_task,
			   (ptr_flg? obj->var_fptr_s2 : fptr),
			   obj->var_sobj))
		{
		    thgsens_set_range(obj->var_v2,
				      (rng_flg? obj->var_gmin : min),
				      (rng_flg? obj->var_gmax : max));
		    obj->var_v2_flg = 1;
		    printf("%s %i %s\n","velocity sensor", ix, "complete");

		    /* copy string and generic min and max val */
		    THVELSET_CPYSTR(&obj->var_ch1, ch_name);
		    obj->var_gmin = min;
		    obj->var_gmax = max;
		    thgsens_set_calibration_buffers(obj->var_v2, _v2_cal_x, _v2_cal_y, THORNIFIX_S_CAL_SZ);
		}
	    else
		{
		    printf("%s\n","unable to create velocity sensor 2");
		    obj->var_okflg = 0;
		}

	    break;

	case 2:
	    printf("%s\n","v2 sensor");
	    if(thgsens_new(&obj->var_v3,
			   (ch_flg? obj->var_ch3 : ch_name),
			   obj->var_task,
			   (ptr_flg? obj->var_fptr_s3 : fptr),
			   obj->var_sobj))
		{
		    thgsens_set_range(obj->var_v3,
				      (rng_flg? obj->var_gmin : min),
				      (rng_flg? obj->var_gmax : max));
		    obj->var_v3_flg = 1;
		    printf("%s %i %s\n","velocity sensor", ix, "complete");

		    /* copy string and generic min and max val */
		    THVELSET_CPYSTR(&obj->var_ch1, ch_name);
		    obj->var_gmin = min;
		    obj->var_gmax = max;
		    thgsens_set_calibration_buffers(obj->var_v3, _v3_cal_x, _v3_cal_y, THORNIFIX_S_CAL_SZ);
		}
	    else
		{
		    printf("%s\n","unable to create velocity sensor 3");
		    obj->var_okflg = 0;
		}

	    break;

	default:
	    printf("%s\n","v3 sensor");
	    if(thgsens_new(&obj->var_v4,
			   (ch_flg? obj->var_ch4 : ch_name),
			   obj->var_task,
			   (ptr_flg? obj->var_fptr_s4 : fptr),
			   obj->var_sobj))
		{
		    thgsens_set_range(obj->var_v4,
				      (rng_flg? obj->var_gmin : min),
				      (rng_flg? obj->var_gmax : max));
		    obj->var_v4_flg = 1;
		    printf("%s %i %s\n","velocity sensor", ix, "complete");

		    /* copy string and generic min and max val */
		    THVELSET_CPYSTR(&obj->var_ch1, ch_name);
		    obj->var_gmin = min;
		    obj->var_gmax = max;
		    thgsens_set_calibration_buffers(obj->var_v4, _v4_cal_x, _v4_cal_y, THORNIFIX_S_CAL_SZ);
		}
	    else
		{
		    printf("%s\n","unable to create velocity sensor 4");
		    obj->var_okflg = 0;
		}
	}

    /* check for ok flags */
    if(obj->var_v1 && obj->var_v2)
	{
	    if(obj->var_v1->var_okflg &&
	       obj->var_v2->var_okflg)
		obj->var_okflg = 1;
	}

    return 0;
}

int thvelsen_disable_sensor(thvelsen* obj,
			    unsigned int ix)
{
    if(!obj || ix > 3)
	return 1;

    switch(ix)
	{
	case 0:
	    obj->var_v1_flg = 0;
	    obj->var_v1->var_flg = 0;
	    break;
	case 1:
	    obj->var_v2_flg = 0;
	    obj->var_v2->var_flg = 0;
	    break;
	case 2:
	    obj->var_v3_flg = 0;
	    obj->var_v3->var_flg = 0;
	    break;
	default:
	    obj->var_v4_flg = 0;
	    obj->var_v4->var_flg = 0;
	}

    /* decrement counter */
    if(obj->var_sen_cnt > 0)
	obj->var_sen_cnt--;

    return 0;
}

/* get velocity */
double thvelsen_get_velocity(thvelsen* obj)
{
    if(!obj)
	return 0;

    double v = 0.0;
    double _dp[1] = {0.0};
    double _gx[1] = {0.0};

    if(obj->var_v1 && obj->var_v1_flg)
	{
	    _dp[0] = thgsens_get_value2(obj->var_v1);
	    /* if(thor_interpol(_v1_pcal_x, _v1_pcal_y, THORNIFIX_P_CAL_SZ, _dp, _gx, 1)) */
	    /* 	_gx[0] = 0.0; */
	    v += THVELSEN_VELOCITY(_dp[0]) + _gx[0];
	}

    if(obj->var_v2 && obj->var_v2_flg)
	{
	    _dp[0] = thgsens_get_value2(obj->var_v2);
	    /* if(thor_interpol(_v2_pcal_x, _v2_pcal_y, THORNIFIX_P_CAL_SZ, _dp, _gx, 1)) */
	    /* 	_gx[0] = 0.0; */
	    v += THVELSEN_VELOCITY(_dp[0]) + _gx[0];
	}

    if(obj->var_v3 && obj->var_v3_flg)
    	{
    	    _dp[0] = thgsens_get_value2(obj->var_v3);
    	    /* if(thor_interpol(_v3_pcal_x, _v3_pcal_y, THORNIFIX_P_CAL_SZ, _dp, _gx, 1)) */
    	    /* 	_gx[0] = 0.0; */
    	    v += THVELSEN_VELOCITY(_dp[0]) + _gx[0];
    	}

    if(obj->var_v4 && obj->var_v4_flg)
    	{
    	    _dp[0] = thgsens_get_value2(obj->var_v4);
    	    /* if(thor_interpol(_v4_pcal_x, _v4_pcal_y, THORNIFIX_P_CAL_SZ, _dp, _gx, 1)) */
    	    /* 	_gx[0] = 0.0; */
    	    v += THVELSEN_VELOCITY(_dp[0]) + _gx[0];
    	}

    obj->var_ave_vel = (obj->var_sen_cnt>0?
			(v / (double) (obj->var_sen_cnt)) :
			v);

    /* if the callback function pointer was assigned
     * call to update external widget */
    if(obj->var_fptr)
	obj->var_fptr(obj->var_sobj, &obj->var_ave_vel);

    return obj->var_ave_vel;
}

/* enable sensor */
int thvelsen_enable_sensor(thvelsen* obj,
			   unsigned int ix)
{
    if(!obj || ix > 3)
	return 1;
 
    switch(ix)
	{
	case 0:
	    obj->var_v1_flg = 1;
	    obj->var_v1->var_flg = 1;
	    break;
	case 1:
	    obj->var_v2_flg = 1;
	    obj->var_v2->var_flg = 1;
	    break;
	case 2:
	    obj->var_v3_flg = 1;
	    obj->var_v3->var_flg = 1;
	    break;
	default:
	    obj->var_v4_flg = 1;
	    obj->var_v4->var_flg = 1;
	}

    /* increment counter */
    if(obj->var_sen_cnt < 4)
	obj->var_sen_cnt++;
    printf("Sensor count: %i\n", obj->var_sen_cnt);
    return 0;
}
