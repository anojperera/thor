/*
 * Main header file for describing interfaces for application parent class.
 * All applications are to inherit from the parent class.
 * This class provides hooks and interfaces for communicating with the server
 * component.
 */

#ifndef __THAPP_H__
#define __THAPP_H__

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <gqueue.h>
#include <libconfig.h>
#include "thornifix.h"
#include "thcon.h"


/* Application return codes */
#define THAPP_RT_NORMAL 0
#define THAPP_RT_CHILD 1
#define THAPP_RT_CONT 2

#define THAPP_DISP_BUFF_SZ 256
#define THAPP_SEC_DIV(obj_ptr)			\
    1000000 / (obj_ptr)->var_sleep_time
#define THAPP_POST_INC_MSGCOUNT(obj_ptr)	\
    (obj_ptr)->_msg_cnt++
#define THAPP_PRE_INC_MSGCOUNT(obj_ptr)		\
    ++(obj_ptr)->_msg_cnt

/* Helper macro for displaying messages */
#define THAPP_DISP_MESG(STR, BUFF)		\
    memset((BUFF), 0, THAPP_DISP_BUFF_SZ);	\
    printw((STR));				\
    refresh();					\
    getnstr((BUFF), THAPP_DISP_BUFF_SZ-1);	\
    clear()
/*
 * Macro for initialising the function pointer table.
 */
#define THAPP_INIT_FPTR(obj)			\
    (obj)->_var_fptr.var_init_ptr = NULL;	\
    (obj)->_var_fptr.var_del_ptr = NULL;	\
    (obj)->_var_fptr.var_start_ptr = NULL;	\
    (obj)->_var_fptr.var_stop_ptr = NULL;	\
    (obj)->_var_fptr.var_read_ptr = NULL;	\
    (obj)->_var_fptr.var_write_ptr = NULL;	\
    (obj)->_var_fptr.var_log_ptr = NULL;	\
    (obj)->_var_fptr.var_report_ptr = NULL;	\
    (obj)->_var_fptr.var_cmdhnd_ptr = NULL


typedef struct _thapp thapp;
typedef int (*thapp_gf_ptr)(thapp*, void*);

typedef enum {
    thapp_headless,
    thapp_master
} thapp_opmode;

/*
 * Table of function pointers.
 * All child classes inherits this class
 * may implement the vtable.
 */
struct _thapp_fptr_arr
{
    thapp_gf_ptr var_init_ptr;
    thapp_gf_ptr var_del_ptr;
    thapp_gf_ptr var_start_ptr;
    thapp_gf_ptr var_stop_ptr;
    thapp_gf_ptr var_read_ptr;
    thapp_gf_ptr var_write_ptr;
    thapp_gf_ptr var_log_ptr;
    thapp_gf_ptr var_report_ptr;
    int (*var_cmdhnd_ptr)(thapp*, void*, char);
};

struct _thapp
{
    unsigned int var_init_flg;
    unsigned int var_run_flg;							/* flag to indicate test is running */
    unsigned int var_sleep_time;						/* sleep time (miliseconds) */
    unsigned int var_max_opt_rows;						/* Maximum optional rows */
    unsigned int var_queue_limit;
    int _msg_cnt;

    thapp_opmode var_op_mode;							/* operation mode */
    config_t var_config;							/* configuration pointer */
    struct thor_msg _msg_buff;							/* message buffer */
    thcon _var_con;								/* connection object */

    void* var_child;								/* child object */

    /* Display buffers */
    char var_disp_header[THAPP_DISP_BUFF_SZ];
    char var_disp_vals[THAPP_DISP_BUFF_SZ];
    char var_cmd_vals[THAPP_DISP_BUFF_SZ];
    char var_disp_sp[THOR_BUFF_SZ];

    char var_disp_opts[THOR_BUFF_SZ]; 						/* Option buffer */

    /* Buffers to hold job number and tag number if applicable */
    char var_job_num[THAPP_DISP_BUFF_SZ];
    char var_tag_num[THAPP_DISP_BUFF_SZ];

    /* Log file pointer */
    FILE* var_def_log;
    /*
     * Queue for buffering the messages recieved from the
     * server.
     */
    gqueue _var_msg_queue;

    /*
     * Function pointer array.
     * These function pointers are called at various
     * stages of the operation.
     */
    struct _thapp_fptr_arr _var_fptr;

    sem_t _var_sem;
    pthread_t _var_thread;
    pthread_mutex_t _var_mutex;
};


#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor and destructor */
    int thapp_init(thapp* obj);
    void thapp_delete(thapp* obj);


    /*
     * Start and stop methods.
     * These method starts and stops the main loop.
     * If the respective function pointers are assigned
     * they shall be called.
     *
     * The start method shall only call function pointers when the
     * connection object has been successfull.
     */
    int thapp_start(thapp* obj);
    int thapp_stop(thapp* obj);

    /*
     * Message retrieves the value from the message queue
     * in a thread safe manor. The method is wrapped in
     * a mutex for thread safety.
     * Application are encouraged to use this for popping
     * messages off the queue rather than directly accessing
     * values using the methods provided by generic queue.
     */
#define  thapp_get_msg(obj, msg)		\
    pthread_mutex_lock(&(obj)->_var_mutex);	\
    gqueue_out(&(obj)->_var_msg_queue, (msg));	\
    pthread_mutex_unlock(&(obj)->_var_mutex)

    /* Helper macros for setting child methods */
#define thapp_set_init_ptr(obj_ptr, fptr)			\
    (obj_ptr)->_var_parent._var_fptr.var_init_ptr = fptr
#define thapp_set_del_ptr(obj_ptr, fptr)		\
    (obj_ptr)->_var_parent._var_fptr.var_del_ptr = fptr
#define thapp_set_start_ptr(obj_ptr, fptr)			\
    (obj_ptr)->_var_parent._var_fptr.var_start_ptr = fptr
#define thapp_set_stop_ptr(obj_ptr, fptr)			\
    (obj_ptr)->_var_parent._var_fptr.var_stop_ptr = fptr
#define thapp_set_read_ptr(obj_ptr, fptr)			\
    (obj_ptr)->_var_parent._var_fptr.var_read_ptr = fptr
#define thapp_set_write_ptr(obj_ptr, fptr)			\
    (obj_ptr)->_var_parent._var_fptr.var_write_ptr = fptr
#define thapp_set_log_ptr(obj_ptr, fptr)		\
    (obj_ptr)->_var_parent._var_fptr.var_log_ptr = fptr
#define thapp_set_report_ptr(obj_ptr, fptr)			\
    (obj_ptr)->_var_parent._var_fptr.var_report_ptr = fptr
#define thapp_set_cmdhnd_ptr(obj_ptr, fptr)			\
    (obj_ptr)->_var_parent._var_fptr.var_cmdhnd_ptr = fptr

    /*
     * Helper macro for derrived classes to get the connection
     * object.
     */
#define thapp_get_con_obj_ptr(obj_ptr)		\
    &(obj_ptr)->_var_con

    /* Get message count */
#define thapp_get_msg_cnt(obj_ptr)		\
    (obj_ptr)->_msg_cnt
    
    /* Reset message counter */
#define thapp_reset_msg_cnt(obj_ptr)		\
    (obj_ptr)->_msg_cnt = 0
    
    /* Macro for resetting the special message buffer */
#define thapp_reset_sp_buff(obj_ptr)				\
    memset((void*) (obj_ptr)->var_disp_sp, 0, THOR_BUFF_SZ)

#ifdef __cplusplus
}
#endif


#endif /* __THAPP_H__ */
