/*
 * Implementation of velocity probe class.
 * Derived from the parent class generic snesors.
 * Calculates, air velocity.
 */
#include <math.h>
#include "thvprb.h"

#define THVPRB_AIR_DENSITY 1.2
/* Callback methods hooked to the parent class */
static void _thvprb_delete(void* obj);
static double _thvprb_get_value(void* obj);

/* Constructor */
thgsensor* thvprb_new(thvprb* obj)
{
    if(obj == NULL)
	{
	    obj = (thvprb*) malloc(sizeof(thvprb));
	    obj->var_int_flg = 1;
	}
    else
	obj->var_int_flg = 0;

    /* Call parent constructor */
    if(thgsensor_new(&obj->var_parent, NULL))
	{
	    /*
	     * Errors have occured, checked to see if
	     * object was internally created and deleted.
	     */
	    if(obj->var_int_flg)
		free(obj);
	    return NULL;
	}

    /* Set self as child object of parent */
    obj->var_parent.var_child = (void*) obj;

    /* Set callback methods of parent class */
    obj->var_parent.var_del_fptr = _thvprb_delete;
    obj->var_parent.var_get_fptr = _thvprb_get_value;

    obj->var_init_flg = 0;
    obj->var_air_density = THVPRB_AIR_DENSITY;

    obj->var_val = 0.0;
    obj->var_child = NULL;

    /* Return pointer of parent */
    return &obj->var_parent;
}

/* Delete method */
void thvprb_delete(thvprb* obj)
{
    /* check object */
    if(obj == NULL)
	return 0;

    obj->var_child = NULL;
    
    /* check if object was created and delete it */
    if(obj->var_int_flg)
	free(obj);
    
    return;
}

/*===========================================================================*/
/***************************** Private Methods *******************************/

/* Delete object callback method */
static void _thvprb_delete(void* obj)
{
    thvprb* _obj;
    
    if(obj == NULL)
	return;

    _obj = (thvprb*) obj;
    thvprb_delete(_obj);

    return;
}


/* Get probe velocity */
static double _thvprb_get_value(void* obj)
{
    thvprb* _obj;

    /* Check object pointer */
    if(obj == NULL)
	return 0.0;
    
    _obj = (thvprb*) obj;


    /* Calculate velocity based on differential pressure */
    _obj->var_val = _obj->var_parent.var_val / (0.5 * THVPRB_AIR_DENSITY);
    _obj->var_val = sqrt(_obj->var_val);

    return _obj->var_val;
}
