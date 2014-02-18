/*
 * Implementation of the speed sensor. This class
 * converts the frequency to RPM.
 */

#include "thspd.h"

#define THSPD_FREQ_CONV 60.0


/* Callback methods hooked to the parent class */
static void _thspd_delete(void* obj);
static double _thspd_get_value(void* obj);


/* Constructor */
thsen* thspd_new(thspd* obj)
{
    if(obj == NULL)
	{
	    obj = (thspd*) malloc(sizeof(thspd*));
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
    thsen_set_parent_del_fptr(obj, _thspd_delete);
    thsen_set_parent_get_fptr(obj, _thspd_get_value);

    obj->var_val = 0.0;
    obj->var_init_flg = 1;
    obj->var_child = NULL;

    /* Initialise self vtable */
    thsen_self_init_vtable(obj);

    return thgsens_return_parent(&obj->var_parent);
}

/* Delete method */
void thspd_delete(thspd* obj)
{
    if(obj == NULL)
	return;

    /* Remove parent object */
    thgsensor_delete(&obj->var_parent);

    obj->var_child = NULL;
    obj->var_init_flg = 0;

    if(obj->var_int_flg)
	free(obj);

    return;
}

/*===========================================================================*/
/***************************** Private Methods *******************************/

/* Delete object callback method */
static void _thspd_delete(void* obj)
{
    thspd* _obj;

    /* Check for object pointer */
    if(obj == NULL)
	return;

    _obj = (thspd*) obj;
    thspd_delete(_obj);

    return;
}

/* Get value method */
static double _thspd_get_value(void* obj)
{
    thspd* _obj;

    /* Check object pointer */
    if(obj == NULL)
	return 0.0;

    _obj = (thspd*) obj;

    if(_obj->var_init_flg != 1)
	return 0.0;

    /* Calculate RPM */
    _obj->var_val = _obj->var_parent.var_val * THSPD_FREQ_CONV;

    /* Call get function pointer if set */
    if(_obj->var_fptr.var_get_fptr)
	return _obj->var_fptr.var_get_fptr(_obj->var_child);
    else
	return _obj->var_val;
}
