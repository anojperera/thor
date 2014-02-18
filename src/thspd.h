/*
 * Speed sensor class is an abstract of generic sensor class.
 */
#ifndef __THSPD_H__
#define __THSPD_H__

#include "thsen.h"
#include "thgsensor.h"

typedef struct _thspd thspd;

struct _thspd
{
    thgsensor var_parent;
    unsigned int var_int_flg;
    unsigned int var_init_flg;

    double var_val;

    struct thsen_vftpr var_fptr;
    void* var_child;
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor and destructor */
    thsen* thspd_new(thspd* obj);
    void thspd_delete(thspd* obj);


#endif __cplusplus
}
#endif    

#endif /* __THSPD_H__ */

