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
#include <gqueue.h>
#include "thornifix.h"

#define THCON_URL_BUFF_SZ 2048
#define THCON_SUBNET_NAME_SZ 16
#define THCON_PORT_NAME_SZ 16
#define THCON_SERVER_NAME_SZ 128
#define THCON_GEN_INFO_SZ 64

#define THCON_MAC_ADDR_BUFF 6						/* base 16 mac address buffer */
#define THCON_MAC_ADDR_STR_BUFF 64					/* mac address buffer */

#define THCON_FORM_IP "ip_addr"
#define THCON_GEO_COUNTRY "my_country"
#define THCON_GEO_REGION "my_region"
#define THCON_GEO_TOWN "my_town"
#define THCON_GEO_LONG "my_long"
#define THCON_GEO_LAT "my_lat"


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

/*
 *
 * Connection mode for the server.
 * Can make the connection as a client or server.
 * If the connection was made as a client, hook, callback to
 * recieve data. On server mode, connections are pooled using
 * epol. Information is relayed between primary connection to
 * sys object
 *
 */
typedef enum {
    thcon_mode_client,
    thcon_mode_server
} thcon_mode;


struct _thcon
{
    int var_flg;							/* internal flag */
    int var_con_sock;						/* main connection socket */
    int var_acc_sock;						/* accept socket for server connection */
	int var_wol_sock;						/* wake on lan socket */

    /*
     * Active socket descriptor. This socket is set when
     * messages are recieved on the sockets managed by the
     * epoll instance. Descriptor can be obtained by the
     * macro THCON_GET_ACTIVE_SOCK
     */
    int _var_act_sock;

    thcon_stat _var_con_stat;				/* connection status */
    thcon_mode _var_con_mode;				/* connection mode to the server */

    unsigned int var_num_conns;
    unsigned int var_geo_flg;				/* indicate geo location is obtained */
    unsigned int var_ip_flg;				/* indicate ip flag was obtained */
    unsigned int _var_bf_sz;				/* dynamic buffer size holding */
    unsigned int _var_curl_timeout;			/* default time out mode */

	unsigned int var_inbuff_sz;				/* size of the inbound buffer */
	unsigned int var_outbuff_sz;			/* size of of the outbound buffer */

	unsigned char var_mac_addr[THCON_MAC_ADDR_BUFF];
	char var_subnet_addr[THCON_SUBNET_NAME_SZ];
	char var_mac_addr_str[THCON_MAC_ADDR_STR_BUFF];

    /*-----------------------------------------*/
    /*
     * Memory buffer to be used between read
     * and write to the main programm.
     */
    char* var_membuff_in;
    char* var_membuff_out;
    /*-----------------------------------------*/

    char var_port_name[THCON_PORT_NAME_SZ];
    char var_svr_name[THCON_SERVER_NAME_SZ];

    char var_ip_addr_url[THCON_URL_BUFF_SZ];		/* url for the ip address locator */
    char var_geo_addr_url[THCON_URL_BUFF_SZ];		/* url for the geo information */

    struct addrinfo _var_info;						/* address info struct */
    struct thcon_buff _var_url_buff;				/* buffer to hold external url */
    struct thcon_host_info var_my_info;				/* information about the location */

    int* _var_cons_fds;								/* connection sockets */
    int* _var_epol_inst;							/* epoll instance */
    void* _var_event_col;							/* event collection */

    pthread_t _var_run_thread;						/* internal running thread */
    pthread_t _var_svr_write_thread;				/* server writing thread */
    pthread_mutex_t _var_mutex;						/* mutex for controlling file descriptor array */
    pthread_mutex_t _var_mutex_q;					/* mutex for protecting the queue */
    sem_t _var_sem;									/* semaphore for controlling the delete method */
    void* _ext_obj;									/* external object pointer */
    gqueue _msg_queue;								/* message queue */

    /* set callback function to get a callback when data is recieve or write on the socket */
    /*
     * Recv callback is fired when data is recieved on listening sockets.
     * All messages between connections are made using message struct.
     */
    int (*_thcon_recv_callback)(void*, void*, size_t);
    int (*_thcon_write_callback)(void*, void*, size_t);

    /*
     * Callback pointer to indicate when a connection
     * is made on this server.
     * First argument is the external object pointer and the
     * second argument is the connection object itself.
     */
    int (*_thcon_conn_made)(void*, void*);

    /*
     * Callback method to indicate the connection was closed.
     * First argument is the external object pointer,
     * second argument is the connection object it self.
     * Third argument is the closed socket.
     */
    int (*_thcon_conn_closed)(void*, void*, int);
};

#ifdef __cplusplus
extern "C" {
#endif

    /* initialise class */
    int thcon_init(thcon* obj, thcon_mode mode);						/* initialise connection object */
    void thcon_delete(thcon* obj);									/* free connection struct information */

    /* Methods for handling my information struct */
#define thcon_reset_my_info(obj)					\
    memset((void*) &(obj)->var_my_info, 0, sizeof(struct thcon_host_info))

    const char* thcon_get_my_addr(thcon* obj);
    int thcon_get_my_geo(thcon* obj);

    /* contact admin and send local ip and geo location */
    int thcon_contact_admin(thcon* obj, const char* admin_url);

	/*
	 * Creates a raw socket for communication. If successful a socket
	 * descriptor shall be returned. If not -1 is returned.
	 * After a sucessful call to this use GET_ACTIVE_SCOK macro to
	 * return the active socket. This method should only be used on
	 * client mode.
	 */
	int thcon_create_raw_sock(thcon* obj);

    /* Start stop methods */
    int thcon_start(thcon* obj);
    int thcon_stop(thcon* obj);

    /* In the server mode, message is sent to all sockets */
    int thcon_send_info(thcon* obj, void* data, size_t sz);

    /*
     * Multicast message to all connected sockets.
     * Only works on the server mode.
     */
    int thcon_multicast(thcon* obj, void* data, size_t sz);

	/*
	 * Wake on lan device from mac address
	 */
	int thcon_wol_device(thcon* obj, const char* mac_addr);

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

    /* set geo location url */
#define thcon_set_geo_ip(obj, name)					\
    memset((void*) (obj)->var_geo_addr_url, 0, THCON_URL_BUFF_SZ);	\
    strncpy((obj)->var_geo_addr_url, name, THCON_URL_BUFF_SZ-1);	\
    (obj)->var_geo_addr_url[THCON_URL_BUFF_SZ-1] = '\0'


    /* gets the connection status */
#define thcon_get_conn_stat(obj)		\
    (obj)->_var_con_stat

#define thcon_set_ext_obj(obj, ext)		\
    (obj)->_ext_obj = (ext)
#define thcon_get_ext_obj(obj)			\
    (obj)->_ext_obj
    /* Set callback function pointers for recieving data */
#define thcon_set_recv_callback(obj, fptr)	\
    (obj)->_thcon_recv_callback = fptr

    /* Set callback to indicate the connection was closed */
#define thcon_set_closed_callback(obj, fptr)	\
    (obj)->_thcon_conn_closed = fptr

    /* Set callback to indicate connection was made */
#define thcon_set_conmade_callback(obj, fptr)	\
    (obj)->_thcon_conn_made = fptr

    /* Set timeout method */
#define thcon_set_timeout(obj, time)		\
    (obj)->_var_curl_timeout = time

	/* Set subnet */
#define thcon_set_subnet(obj, subnet)								\
	memset((obj)->var_subnet_addr, 0, THCON_PORT_NAME_SZ);			\
	strncpy((obj)->var_subnet_addr, subnet, THCON_PORT_NAME_SZ-1);	\
	(obj)->var_subnet_addr[THCON_PORT_NAME_SZ-1] = '\0'

    /* Get active socket */
#define THCON_GET_ACTIVE_SOCK(obj)		\
    (obj)->_var_act_sock

#ifdef __cplusplus
}
#endif

#endif /* __THCON_H__ */
