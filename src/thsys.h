/* Generic class for handling the main loop */
#ifndef __THSYS_H__
#define __THSYS_H__

#include "thornifix.h"

typedef struct _thsys thsys;


struct _thsys
{
    int var_flg;
    int var_client_count;

    /* Analog tasks */
    TaskHandle var_a_outask;		/* analog output task */
    TaskHandle var_a_intask;		/* analog input task */

    /* Digital tasks */
    TaskHandle var_d_outask;		/* analog output task */    
    TaskHandle var_d_intask;		/* analog input task */

    float64 var_sample_rate;		/* sample rate */

    
};


#endif /* __THSYS_H__ */
