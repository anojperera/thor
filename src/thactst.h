/* Header file for defining interfaces for Damper slam shut test */
#ifndef __THACTST_H__
#define __THACTST_H__

#include <stdlib.h>
#include <stdio.h>
#include "thornifix.h"
#include "thgsens.h"		/* generic sensor */
#include "thvelsen.h"		/* velocity sensor */

/* Forward declaration of struct */
typedef struct _thactst thactst;

/* C functions */
#ifdef __cplusplus
extern "C" {
#endif

/* constructor and destructor */
int thactst_initialise(FILE* fp, thactst* obj, void* sobj);
void thactst_delete(void);

/* Set sizes and duct diameter */
int thactst_set_diameter(double dia);
int thactst_set_damper_sz(double width, double height);

/* Set velocity ceiling */
int thactst_set_max_velocity(double val);

/* Fan speed */
int thactst_set_fan_speed(double percen);

/* Start and stop the test */
int thactst_start(void);
int thactst_stop(void);

/* Close actuator */
int thactst_close_act(void);


#ifdef __cplusplus
}
#endif
#endif /* __THACTST_H__ */
