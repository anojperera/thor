/* Header file provides interfaces for handling buffer.
 * buffer structure is internally declared and adding
 * and removing is done via functions.
 *
 *
 * Buffer continually calculates the average of values
 * each time a new element is added to the list. The
 * size of the list is to be declared at constructor.
 * Tue Aug 30 12:29:55 GMTDT 2011 */

#ifndef __THBUFF_H__
#define __THBUFF_H__

#include <stdlib.h>
#include <math.h>

typedef struct _thbuff thbuff;

struct _thbuff
{
    double* var_buff;
    unsigned int var_ix;
    unsigned int var_count;
    double var_avg;
    double var_stdev;
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor */
    int thbuff_new(size_t num, thbuff** obj);

    int thbuff_delete(thbuff* obj);

    extern inline double thbuff_add_new(thbuff* obj, double val);

#ifdef __cplusplus
}
#endif

#endif /* __THBUFF_H__ */
