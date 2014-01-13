/*
 * Application for running the AHU performance test.
 * Auto Controls the damper and records pressure and
 * temperature readings.
 */
#ifndef __THAPP_AHU_H__
#define __THAPP_AHU_H__

#include <stdlib.h>
#include "thornifix.h"
#include "thsen.h"
#include "thapp.h"

#define THAPP_AHU_DMP_BUFF 500
#define THAPP_AHU_NUM_MAX_PROBES 4


typedef struct _thapp_ahu thapp_ahu;

struct _thapp_ahu
{
    thapp _var_parent;
    int var_init_flg;
    
    /*
     * 0 - manual (default)
     * 1 - auto.
     */
    int var_mode;							/* Running mode. */

    /*
     * Extta time is added to when calibration in progress.
     * This is to allow settling time of the actuator.
     * The setting is brought to the configuration file so that
     * it can be edited outside the program.
     */
    int var_calib_wait_ext;
    
    unsigned int var_calib_flg;						/* Calibration flag is in progress */
    unsigned int var_calib_app_flg;					/* Apply calibration flag */
    unsigned int var_raw_flg;						/* Toggle flag to show raw voltage values */
    unsigned int var_dmp_cnt;
    double var_act_pct;							/* Actuator percentage */
    
    double var_dmp_buff[THAPP_AHU_DMP_BUFF];
    
    /*
     * Sensor array for the AHUs.
     * Velocity sensors.
     */
    thsen* _var_vsen;
    
    thsen* _var_tp_sen;							/* Temperature sensor */
    thsen* _var_sp_sen;							/* Speed sensor */
    thsen* _var_st_sen;							/* Static sensor */

    /*
     * Array of pointers to hold addr of message struct.
     * The array elements are set to the memory address of
     * message struct's channels values dealing with v-probes.
     */
    double* _var_msg_addr[THAPP_AHU_NUM_MAX_PROBES];

    double** _var_dp_val_ptr;						/* Array of pointers to dp values */

    /*
     * This sensor is used for measuring the loss in static pressure in
     * the test set up.
     * It uses a smart sensor which is an array of sensors used for
     * acurately measuring a diverse range of pressure.
     */
    thsen* _var_stm_sen;						/* Static smart sensor */

    double var_def_static;
    double var_duct_dia;
    double var_duct_vel;
    double var_duct_vol;
    double var_duct_loss;						/* Static pressure loss in duct */
    double var_t_ext_st;						/* Total external static pressure */
    double var_fm_ratio;						/* fan motor pulley ratio */

    /* Array of function pointers */
    struct _thapp_fptr_arr _var_fptr;    
    void* var_child;
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor and destructor */
    thapp* thapp_ahu_new(void);
    void thapp_ahu_delete(thapp_ahu* obj);

    /* Start and stop methods are implemented in the parent class. */

    
    
#ifdef __cplusplus
}
#endif    

#endif /* __THAPP_AHU_H__ */
