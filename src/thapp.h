/*
 * Main header file for describing interfaces for application parent class.
 * All applications are to inherit from the parent class.
 * This class provides hooks and interfaces for communicating with the server
 * component.
 */

#ifndef __THAPP_H__
#define __THAPP_H__

#include <stdlib.h>
#include "thornifix.h"
#include "thcon.h"

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
    (obj)->_var_fptr.var_report_ptr = NULL


typedef struct _thapp thapp;
typedef int (*thapp_gf_ptr)(thapp*, void*);

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
    thapp_gf_prt var_stop_ptr;
    thapp_gf_ptr var_read_ptr;
    thapp_gr_ptr var_write_ptr;
    thapp_gf_ptr var_log_ptr;
    thapp_gf_ptr var_report_ptr;
};

struct _thapp
{
    unsigned int var_init_flg;

    struct thor_msg _msg_buff;							/* message buffer */    
    thcon _var_con;								/* connection object */

    void* var_child;								/* child object */

    /*
     * Function pointer array.
     * These function pointers are called at various
     * stages of the operation.
     */
    struct _thapp_fptr_arr _var_fptr;
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
     */
    int thapp_start(thapp* obj);
    int thapp_stop(thapp* obj);


#endif __cplusplus
}
#endif


#endif /* __THAPP_H__ */
