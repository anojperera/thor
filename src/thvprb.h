/*
 * Header file for defining the interfaces for velocity probe class.
 */

#ifndef __THVPRB_H__
#define __THVPRB_H__

#include "thsen.h"
#include "thgsensor.h"

/* Forward declaration of struct */
typedef struct _thvprb thvprb;

struct _thvprb
{
    thgsensor var_parent;
    unsigned int var_int_flg;
    unsigned int var_init_flg;

    double var_air_density;
    double var_val;

    struct thsen_vftpr var_fptr;
    void* var_child;
};

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * Constructor and destructor
     */
    thsen* thvprb_new(thvprb* obj);
    void thvprb_delete(thvprb* obj);

    /* Return parent */
    inline __attribute__ ((always_inline)) static thsen* thvprb_return_parent(thvprb* obj)
    {
	return thgsens_return_parent(&obj->var_parent);
    }

    /* All other methods are inherited from the parent class */
    
    

#ifdef __cplusplus
}
#endif

#endif /* __THVPRB_H__ */
