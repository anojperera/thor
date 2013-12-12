/*
 * Declaration for the smart sensor. The smart sensor reads pressure acurately
 * from an array of differential pressure sensors which are of different ranges.
 * The advantage is that each sensor in the array can measure pressure acurately
 * in a specific range. 
 */

#ifndef __THSMSEN_H__
#define __THSMSEN_H__

#include <stdlib.h>
#include <libconfig.h>
#include "thornifix.h"
#include "thsen.h"
#include "thgsensor.h"


/* Forward declaration of struct */
typedef struct _thsmsen thsmsen;

struct _thsmsen
{
    thsen var_parent;					/* Inherited parent class */

    unsigned int var_int_flg;				/* Flag to indicate initialisation successful. */

    /*
     * Flag to indicate error have occured and that no further
     * pressure data may be extracted from the sensor array.
     */
    unsigned int var_err_flg;

    unsigned int var_next_ix;				/* Next index to select */
    unsigned int var_raw_val_sz;			/* Size of array */
    
    const double* var_raw_vals;				/* Array of raw values */
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

    /* Constructor and destructor */
    thsen* thsmsen_new(thsmsen* obj, config_setting_t* settings);
    void thsmsen_delete(thsmsen* obj);


    /*
     * Array position of raw values are stored. Therefore every time the raw values are updated,
     * Call to update all sensors shall retrieve the values.
     */
    int thsmsen_set_value_array(thsmsen* obj, const double* vals, size_t sz);

#ifdef __cplusplus
}
#endif

#endif /* __THSMSEN_H__ */
