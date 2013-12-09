/* Generic class for handling the main loop */
#ifndef __THSYS_H__
#define __THSYS_H__

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <gqueue.h>
#include "thornifix.h"

#define THSYS_AI_CHANNELS "Dev1/ai0:12"
#define THSYS_A0_CHANNELS "Dev1/ao0:1"
#define THSYS_EMPTY_STR ""
#define THSYS_CLOCK_SOURCE "OnboardClock"
#define THSYS_NUM_AI_CHANNELS 13
#define THSYS_NUM_AO_CHANNELS 2
#define THSYS_MIN_VAL 0.0
#define THSYS_MAX_VAL 10.0
#define THSYS_DEFAULT_SAMPLE_RATE 8.0
#define THSYS_READ_WRITE_FACTOR 1.5

typedef struct _thsys thsys;

struct _thsys
{
    int var_flg;
    int var_client_count;
    int var_run_flg;					/* flag to indicate system is running */
    
    /* Analog tasks */
    TaskHandle var_a_outask;				/* analog output task */
    TaskHandle var_a_intask;				/* analog input task */

    /* Digital tasks */
    TaskHandle var_d_outask;				/* analog output task */    
    TaskHandle var_d_intask;				/* analog input task */

    float64 var_sample_rate;				/* sample rate */
    float64 var_inbuff[THSYS_NUM_AI_CHANNELS];
    float64 var_outbuff[THSYS_NUM_AO_CHANNELS];

    /*
     * A message queue is used to buffer the output writes.
     * All output values from clients are pushed in to the
     * queue. The internal scan loop shall pop from the
     * queue as requed.
     */
    gqueue _var_out_queue;
    
    pthread_t var_thread;				/* thread id */
    pthread_mutex_t _var_mutex;
    sem_t var_sem;
    void* var_ext_obj;					/* external object */

    /* function pointers for  */
    int (*var_callback_intrupt)(thsys*, void*);		/* interupt callback */
    int (*var_callback_update)(thsys*, void*, const float64*, const int);
};

#ifdef __cplusplus
extern "C" {
#endif

    /* initialise system struct */
    int thsys_init(thsys* obj, int(*callback) (thsys*, void*));
    void thsys_delete(thsys* obj);

    /* start method */
    int thsys_start(thsys* obj);
    int thsys_stop(thsys* obj);
    int thsys_e_stop(thsys* obj);

    /* set write buffer value */
    /* the buffer shall be THSYS_NUM_AO_CHANNELS */
    int thsys_set_write_buff(thsys* obj, float64* buff, size_t sz);

    /* set sampling rate */
#define thsys_set_sample_rate(obj, val)		\
    (obj)->var_sample_rate = (val>0.0? val : THSYS_DEFAULT_SAMPLE_RATE)
#define thsys_set_external_obj(obj, val)	\
    (obj)->var_ext_obj = (val)
#ifdef __cplusplus
}
#endif

#endif /* __THSYS_H__ */
