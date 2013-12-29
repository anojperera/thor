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
thsen* thvprb_new(thvprb* obj)
{
    if(obj == NULL)
	{
	    obj = (thvprb*) malloc(sizeof(thvprb));
	    obj->var_int_flg = 1;
	}
    else
	obj->var_int_flg = 0;

    /* Call parent constructor */
    if(!thgsensor_new(&obj->var_parent, NULL))
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

    /* Set function pointers of the parent object */
    thsen_set_parent_del_fptr(obj, _thvprb_delete);
    thsen_set_parent_get_fptr(obj, _thvprb_get_value);

    obj->var_init_flg = 1;
    obj->var_air_density = THVPRB_AIR_DENSITY;

    obj->var_val = 0.0;
    obj->var_child = NULL;

    /* initialise self vtable */
    thsen_self_init_vtable(obj);    

    /* Return pointer of parent */
    return thgsens_return_parent(&obj->var_parent);
}

/* Delete method */
void thvprb_delete(thvprb* obj)
{
    /* check object */
    if(obj == NULL)
	return;

    /* Remove parent object */
    thgsensor_delete(&obj->var_parent);
    
    obj->var_child = NULL;
    obj->var_init_flg = 0;
    
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
    if(_obj->var_init_flg != 1)
	return 0.0;

    /* Calculate velocity based on differential pressure */
    _obj->var_val = _obj->var_parent.var_val / (0.5 * THVPRB_AIR_DENSITY);
    _obj->var_val = _obj->var_val<0.0? 0.0 : sqrt(_obj->var_val);

    /* Call get function pointer if set */
    if(_obj->var_fptr.var_get_fptr)
	return _obj->var_fptr.var_get_fptr(_obj->var_child);
    else
	return _obj->var_val;
}
