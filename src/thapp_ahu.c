/* Implementation of the air handling unit test program. */

#include "thapp_ahu.h"

thapp* thapp_ahu_new(void)
{
    thapp_ahu* _app;

    _app = (thapp_ahu*) malloc(sizeof(thapp_ahu));

    if(_app == NULL)
	return NULL;

    /* Call to initalise the parent object */
    thapp_init(&_app->_var_parent);

    
}
