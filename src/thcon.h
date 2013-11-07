/* Class for handling connections */
#ifndef __THCON_H__
#define __THCON_H__

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define THCON_URL_BUFF_SZ 2048
#define THCON_PORT_NAME_SZ 16
#define THCON_SERVER_NAME_SZ 128
#define THCON_GEN_INFO_SZ 64

typedef struct _thcon thcon;
struct thcon_buff
{
    char* memory;
    size_t size;
};

struct thcon_host_info
{
    int _init_flg;					/* flag to indicate initialised */
    int _city_id;
    char _my_region[THCON_GEN_INFO_SZ];
    char _my_city[THCON_GEN_INFO_SZ];
    char _my_address[THCON_SERVER_NAME_SZ];
    char _long[THCON_GEN_INFO_SZ];
    char _lat[THCON_GEN_INFO_SZ];
    char _country[THCON_GEN_INFO_SZ];
};

/* connection status load */ 
typedef enum {
    thcon_disconnected,
    thcon_connected,
    thcon_idle
} thcon_stat;


struct _thcon
{
    int var_flg;					/* internal flag */
    int var_con_sock;					/* main connection socket */
    int var_acc_sock;					/* accept socket for server connection */
    int var_mode;					/* mode of operation (server / client) */
    thcon_stat _var_con_stat;				/* connection status */

    unsigned int var_num_conns;

    char var_port_name[THCON_PORT_NAME_SZ];
    char var_svr_name[THCON_SERVER_NAME_SZ];

    char var_ip_addr_url[THCON_URL_BUFF_SZ];		/* url for the ip address locator */
    char var_geo_addr_url[THCON_URL_BUFF_SZ];		/* url for the geo information */

    struct addrinfo _var_info;				/* address info struct */
    struct thcon_buff _var_url_buff;			/* buffer to hold external url */
    struct thcon_host_info var_my_info;			/* information about the location */

    int* _var_cons_fds;					/* connection sockets */
    pthread_t _var_run_thread;				/* internal running thread */
    semt_t _var_sem;					/* semaphore for controlling the delete method */
};

#ifdef __cplusplus
extern "C" {
#endif

    /* initialise class */
    int thcon_init(thcon* obj);				/* initialise connection object */
    void thcon_delete(thcon* obj);			/* free connection struct information */

    /* Methods for handling my information struct */
#define thcon_reset_my_info(obj)					\
    memset((obj)->var_my_info, 0, sizeof(struct thcon_host_info))
    const char* thcon_get_my_addr(thcon* obj);
    int thcon_get_my_geo(thcon* obj);

    /* contact admin and send local ip and geo location */
    int thcon_contact_admin(thcon* obj, const char* admin_url);

    /* server handling options */
    int thcon_start_local_server(thcon* obj);
    int thcon_stop_local_server(thcon* obj);

    int thcon_send_info(thcon* obj, void* data, sizt_t sz);

    /* set server name */
#define thcon_set_server_name(obj, name)				\
    memset((void*) (obj)->var_svr_name, 0, THCON_SERVER_NAME_SZ);	\
    strncpy((obj)->var_svr_name, name, THCON_SERVER_NAME_SZ-1);		\
    (obj)->var_svr_name[THCON_SERVER_NAME_SZ-1] = '\0'
    
    /* set port name */
#define thcon_set_port_name(obj, name) 					\
    memset((void*) (obj)->var_port_name, 0, THCON_PORT_NAME_SZ);	\
    strncpy((obj)->var_port_name, name, THCON_PORT_NAME_SZ-1);		\
    (obj)->var_port_name[THCON_PORT_NAME_SZ-1] = '\0'

    /* gets the connection status */
#define thcon_get_conn_stat(obj)		\
    (obj)->_var_con_stat
    
#ifdef __cplusplus
}
#endif

#endif /* __THCON_H__ */
