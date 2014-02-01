/* Class for handling the leak test */
/* Fri Jan 31 19:39:06 GMT 2014 */

#include <stdlib.h>
#include <math.h>

#include "thornifix.h"
#include "thapp.h"
#include "thsen.h"

#define THAPP_LKG_BUFF 500
#define THAPP_LKG_MAX_SM_SEN 4


/* Enummerated types of leak testing default shall be the manual method */
typedef enum {
    thapp_lkg_tst_man,
    thapp_lkg_tst_static,
    thapp_lkg_tst_leak
} thapp_lkg_test_type;

/* Product type enumeration */
typedef enum {
    thapp_lkg_dmp,
    thapp_lkg_ahu
} thapp_lkg_prod_type;

typedef struct _thapp_lkg thapp_lkg;

struct _thapp_lkg
{
    thapp _var_parent;
    int var_init_flg;

    /*
     * Leak test type which controls the fan.
     */
    thapp_lkg_test_type var_test_type;

    /*
     * Product type being tested.
     */
    thapp_lkg_prod_type var_prod_type;
    
    /*
     * Extra time is added to when calibration is in progress.
     */
    int var_calib_wait_ext;

    unsigned int var_or_ix;					/* Orifice plate index */
    unsigned int var_calib_flg;

    unsigned int var_raw_flg;
    double var_fan_pct;

    double var_fan_buff[THAPP_LKG_BUFF];

    /*
     * Sensor collections
     */

    /* Smart sensor */
    thsen* _var_sm_sen;
    thsen* _var_st_sen;
    thsen* _var_tmp_sen;

    /*
     * Array of pointers corresponding to the positions of the
     * data array which is recieved from the server.
     */
    double* _var_msg_addr[THAPP_LKG_MAX_SEN_BUFF];

    double var_max_static;
    double var_max_leakage;
    
    double var_width;
    double var_height;
    double var_depth;


    struct _thapp_fptr_arr _var_fptr;
    void* var_child;
};

#ifdef __cplusplus
extern "C" {
#endif

    thapp* thapp_lkg_new(void);
    void thapp_lkg_delete(thapp_lkg* obj);

    /* Start stop methods are implemented in the parent class */
    

#ifdef __cplusplus
}
#endif
