/* Implementation of the air handling unit test program. */

#include "thapp_ahu.h"

/* Helper method for loading configuration settings */
static int _thapp_new_helper(thapp* obj);

/* Constructor */
thapp* thapp_ahu_new(void)
{
    int i;
    thapp_ahu* _obj;

    _obj = (thapp_ahu*) malloc(sizeof(thapp_ahu));

    if(_obj == NULL)
	return NULL;

    /* Call to initalise the parent object */
    thapp_init(&_obj->_var_parent);

    /* Set child pointer of parent objecgt */
    _obj->_var_parent.var_child = (void*) _obj;

    /* Initialise function pointer array */
    THAPP_INIT_FPTR(_obj);

    /* Load configurations and initialise sensors */
    if(_thapp_new_helper(_obj))
	{
	    /* Delete parent */
	    thapp_delete(&_obj->_var_parent);
	    free(_obj);
	    return NULL;
	}

    /* Set default running mode to manual */
    _obj->var_mode = 0;
    _obj->var_act_pct = 0;

    /* Initialise actuator buffer */
    for(; i<THAPP_AHU_DMP_BUFF; i++)
	_obj->var_dmp_buff[i] = 0.0;

    _obj->var_duct_dia = 0.0;
    _obj->var_duct_vel = 0.0;
    _obj->var_duct_vol = 0.0;
    _obj->var_duct_loss = 0.0;
    _obj->var_t_ext_st = 0.0;

    _obj->var_child = NULL;

    /* Return thapp object */
    return &_obj->_var_parent;
}

/* Desructor */
void thapp_delete(thapp* obj)
{
    /* Check object pointer */
    if(obj == NULL)
	retun;

    
    
}
