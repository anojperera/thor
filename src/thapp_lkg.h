/* Class for handling the leak test */
/* Fri Jan 31 19:39:06 GMT 2014 */

#include <stdlib.h>
#include <math.h>

#include "thornifix.h"
#include "thapp.h"
#include "thsen.h"

#define THAPP_LKG_BUFF 500
#define THAPP_LKG_MAX_SM_SEN 4
#define THAPP_LKG_CLASS_BUFF 8

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

/* Enumeration of orifice plate diameters */
typedef enum {
    thapp_lkg_20,
    thapp_lkg_30,
    thapp_lkg_40,
    thapp_lkg_60,
    thapp_lkg_80,
    thapp_lkg_100,
    thapp_lkg_vent
} thapp_lkg_orifix_sz;

/* Typedef of leakage class array */
typedef char lkg_cls_arr[THAPP_LKG_CLASS_BUFF];

typedef struct _thapp_lkg thapp_lkg;

struct _thapp_lkg
{
    thapp _var_parent;
    int var_init_flg;

    int var_ahu_lkg_tst_time;					/* Total test time */
    int var_ahu_lkg_tst_incr;					/* Increment the readings should be taken */

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

    unsigned int var_lkg_start_flg;				/* Flag to indicate leakage test is started */
    unsigned int var_or_ix;					/* Orifice plate index */
    unsigned int var_calib_flg;

    unsigned int var_raw_flg;

    unsigned int var_positive_flg;				/*
								 * Flag to indicate test is running under
								 * positive pressure. This option is only
								 *applicable for the AHU test.
								 */
    /*
     * Variable to store the last insertion position
     * into the buffer.
     */    
    unsigned int var_disp_sp_pos;				
    char _var_t_sp_buff[THOR_BUFF_SZ];
    
    /*
     * AHU case leak figures are expressed in L1,L2...
     * these figures are stored in the configuration file.
     * following are the buffer and number of elements.
     */ 
    unsigned int var_num_lkg_arr;
    double* var_lkg_nl_arr;
    double* var_lkg_pl_arr;
    const double* var_d_lkg_arr;
    lkg_cls_arr* var_lkg_cls_arr;
    
    double var_tst_positive_pre;
    double var_tst_negative_pre;
    

    /*
     * This is pointer to the raw voltage value
     * of the active sensor.
     * This allows values to be obtained without passing
     * copies around. Pointer address shall be set by the
     * the smart sensor.
     */
    const double** var_raw_act_ptr;
    
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
     * Venturi meter sensor.
     */
    thsen* _var_vent_sen;

    /*
     * Array of pointers corresponding to the positions of the
     * data array which is recieved from the server.
     */
    double* _var_msg_addr[THAPP_LKG_MAX_SM_SEN];

    double var_max_static;
    double var_max_leakage;

    double _var_dp;
    double _var_ext_static;
    double _var_lkg;
    double _var_lkg_m2;
    double _var_lkg_f700;
    double _var_lkg_f770_base_pressure;					/* This shall depend on positive or negative pressure */
    
    double var_width;
    double var_height;
    double var_depth;

    double var_s_area;							/* Surface area */
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
