/* Header file for SOV interface test
 * Thu Jul 14 13:39:12 GMTDT 2011 */

#ifndef __THSOV_H__
#define __THSOV_H__

#include <stdio.h>
#if defined (WIN32) || defined (_WIN32)
#include <NIDAQmx.h>
#else
#include <NIDAQmxBase.h>
#endif
#include "thgsens.h"		/* generic sensor */

/* struct for SOV */
typedef struct _thsov thsov;

struct _thsov
{
    TaskHandle var_outask;		/* analog output task */
    TaskHandle var_intask;		/* analog input task */

    FILE* var_fp;			/* file pointer */
    double* var_sov_ctrl;		/* solenoid control voltage */
    double* var_sov_or;			/* orientation control voltage */

    int var_thrid;			/* thread id */
    unsigned int var_stflg;		/* start flag */
    unsigned int var_idlflg;		/* idle flag */

    void* var_sobj;			/* object to pass to callback */

    thgsens* var_open_sw;		/* open limit switch */
    thgsens* var_close_sw;		/* close limit switch */

    double var_start_sov_volt;		/* starting voltage */
    double var_start_or_angl;		/* starting orientation angle */
    
    double var_sov_ctrl_grad;		/* solenoid control gradient */
    double var_sov_sup_grad;		/* supply voltage gradint */
    
    unsigned int var_op_flg;		/* Operation flag */
    unsigned int var_count;		/* Test counter */

    float64* var_write;			/* write array */
    float64* var_read;			/* read array */
    
};

#endif /* __THSOV_H__ */
