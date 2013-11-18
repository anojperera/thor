/*
 * Class for controllign the connection and the sysclass.
 * The object can be configured to run as a server or a client.
 * Sat Nov 16 17:28:49 GMT 2013
 */

#ifndef _THSMART_H_
#define _THSMART_H_

#include <stdlib.h>
#include "thornifix.h"
#include "thsys.h"
#include "thcon.h"

typedef struct _thsmart thsmar;

struct _thsmart
{
    unsigned int var_init_flg;					/* flat to indicate initialised */
    thcon_mode _var_con_mode;					/* connection mode */
    unsigned int var_test_type;					/* type of test */

    config_t _var_config;
    
    thcon _var_conn_obj;
    thcon _var_sys_obj;
};

#ifdef __cplusplus
extern "C" {
#endif

    int thsmart_init(thsmart* obj, thcon_mode mode);
    int thsmart_delete(thsmart* obj);

    /*
     * Set the type of the test to run.
     * Has no effect on the server mode.
     */
    inline __attribute__ ((always_inline)) static int thsmart_set_test_type(thsmart* obj, unsigned int type);

#ifdef __cplusplus
}
#endif

#endif /* _THSMART_H_ */
