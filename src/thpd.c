/* Implementation of pressure drop program */
#include "thpd.h"
#include "thgsens.h"

/* class implementation */
struct _thpd
{
    unsigned int var_int_flg;					/* internal flag */
    unsigned int var_gcount;					/* generic counter */
    unsigned int var_scount;					/* sample counter */

    /* Diff flag is set to on, by default. This shall read a differential
     * reading by each differential pressure sensor. If the flag was set to off,
     * each sensor shall read pressure against atmosphere. */
    unsigned int var_diff_flg;					/* diff reading flag */

    TaskHandle var_outtask;
    TaskHandle var_inttask;

    FILE* var_fp;
    double var_static;
    double var_temp;
    double var_v1;
    double var_v2;
    double var_p1;
    double var_p2;
    double var_air_flow;

    /* duct diameter and damper dimensions */
    double var_ddia;
    double var_dwidth;
    double var_dheight;
    double var_darea;
    double var_farea;

    /* result buffer */
    double* var_result_buff;

    /* Settling and reading times are control timers.
     * Readin time is used reading number of samples
     * and use moving average to smooth it */
    double var_settle;
    double var_read_time;

    /*Sensor objects */
    thgsens* var_st_sen;
    thgsens* var_tmp_sen;
    thgsens* var_v1_sen;
    thgsens* var_v2_sen;
    thgsens* var_p1_sen;
    thgsens* var_p2_sen;

    void* var_sobj;			/* external objects */
};
