/* Header file for describing interfaces for pressure drop test.
 * Structure is hidden away in the source file. Interface methods
 * are used for retrieving values and setting. The test is meant to
 * run in an automated state. Once the test is started, fan shall be
 * throttled up, measurements from sensors are recorded until fan is maxed.
 * All relevant data is stored in formated text files. */

#ifndef __THPD__
#define __THPD__

#include <stdlib.h>
#include <stdio.h>
#include "thornifix.h"

/* Result buffer index */
#define THPD_RESULT_VDP1_IX 0
#define THPD_RESULT_VDP2_IX 1
#define THPD_RESULT_DP1_IX 2
#define THPD_RESULT_DP2_IX 3
#define THPD_RESULT_VEL_IX 4
#define THPD_RESULT_VOL_IX 5
#define THPD_RESULT_DP_IX 6
#define THPD_RESULT_TMP_IX 7


/* forward declaration of class */
typedef struct _thpd thpd;

/* constructor and destructor */
thpd* thpd_initialise(void* sobj);
void thpd_delete(void);

/* start stop methods */
int thpd_start(void);
int thpd_stop(void);

/* read and settling time */
int thpd_set_settle_time(double stime);
int thpd_set_read_time(double rtime);

/* set result buffer */
int thpd_set_buffer(double* buff);

/* set file pointer */
int thpd_set_file_pointer(FILE* fp);

/* set duct and damper size */
int thpd_set_damper_size(double width, double height);
int thpd_set_duct_dia(double dia);

#endif	/* __THPD__  */
