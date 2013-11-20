/*
 * Header file for describing interfaces for the server component.
 * 18/11/2013.
 */
#ifndef __THSVR_H__
#define __THSVR_H__

#include "thornifix.h"
#include "thsys.h"
#include "thcon.h"

typedef struct _thsvr thsvr;

struct _thsvr
{
    unsigned int var_init_flg;

    char var_admin1_url[THCON_URL_BUFF_SZ];
    char var_admin2_url[THCON_URL_BUFF_SZ];
    const config_t* _var_config;
    thcon _var_con;
    thsys _var_sys;
};


#ifdef __cplusplus
extern "C" {
#endif

    /* Constructor and destructor */
    int thsvr_init(thsvr* obj, const config_t* config);
    void thsvr_delete(thsvr* obj);

    /* Start and stop methods */
    int thsvr_start(thsvr* obj);
    int thsvr_stop(thsvr* obj);
#ifdef __cplusplus
}
#endif

#endif /* __THSVR_H__ */
