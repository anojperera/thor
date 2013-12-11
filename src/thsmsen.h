/*
 * Declaration for the smart sensor. The smart sensor reads pressure acurately
 * from an array of differential pressure sensors which are of different ranges.
 * The advantage is that each sensor in the array can measure pressure acurately
 * in a specific range. 
 */

#ifndef __THSMSEN_H__
#define __THSMSEN_H__

#include "thornifix.h"
#include "thsen.h"
#include "thgsensor.h"


/* Forward declaration of struct */
typedef struct _thsmsen thsmsen;

struct _thsmsen
{
    thsen _var_parent;					/* Inherited parent class */

    unsigned int var_init_flg;				/* Flag to indicate initialisation successful. */

    /*
     * Flag to indicate error have occured and that no further
     * pressure data may be extracted from the sensor array.
     */
    unsigned int var_err_flg;

    unsigned int var_next_ix;				/* Next index to select */
    
    double var_val;					/* Pressure value */
    
    /* Array of sensors */
    unsigned int var_sen_cnt;				/* Sensor count */
    thsen** var_sen;					/* Pointers to sensor array */

    
    struct thsen_vftpr var_fptr;			/* Function pointer array */
    void* var_child;					/* Child pointer array */
    
};

#ifdef __cplusplus
extern "C" {
#endif

    thsen* thsmsen_new(

#ifdef __cplusplus
}
endif

#endif /* __THSMSEN_H__ */
