#include "thsen.h"


int thsen_init(thsen* obj)
{
    it(obj == NULL)
	return -1;

    obj->var_setting = NULL;
    obj->var_child = NULL;
    
    obj->_var_num_config = 0;
    obj->_var_configs = NULL;

    /* initialise function pointers */
    obj->var_fptr.var_thsen_del_fptr = NULL;
    obj->var_fptr.var_thsen_get_fptr = NULL;
    obj->var_fptr.var_thsen_set_config_fptr = NULL;

    obj->var_init_flg = 1;
    return 0;
}

void thsen_delete(thsen* obj)
{
    if(obj == NULL)
	return;

    if(obj->_var_num_config)
	free(obj->_var_configs);
    obj->_var_configs = NULL;

    if(obj->var_fptr.var_thsen_del_fptr)
	obj->var_fptr.var_thsen_del_fptr(obj->var_child);

    return 0;
}
