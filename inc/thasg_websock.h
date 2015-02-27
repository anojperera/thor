/*
 * This is a wrapper for the libwebsocket class. Provides a simple
 * interface to be used by the asgard server
 */
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <libconfig.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "libwebsockets.h"

#include "thornifix.h"

/* STL queue for storing the messages */
#include <queue>

struct _thasg_msg_wrap
{
    int _fd;						/* File descriptor */
    char _msg[THORNIFIX_MSG_BUFF_SZ];			/* message buffer */
    size_t _msg_sz;
};

class _thasg_websock
{
 protected:
    unsigned int _num_cons;				/* Number of connections */
    unsigned int _err_flg;				/* Flag to indicates error have occured */
    unsigned int _cont_flg;

    std::queue<struct _thasg_msg_wrap> _msg_queue;	/* Message queue */


    struct libwebsocket_context* _websock_context;	/* Websocket context */
    struct lws_context_creation_info _websock_info;	/* Infor struct */

    void* _self_ptr;					/* Self pointer */

    pthread_mutex_t var_mutex;				/* Lock for the queue */

 public:
    _thasg_websock(int port);
    virtual ~_thasg_websock(void);

    /* Perform servive operations */
    int service_server(void);
    int service_server(const char* msg, size_t sz);

    /* Increment and decrement operators for the clinet */
    int incr_cons(void);
    int decr_cons(void);

    struct _thasg_msg_wrap* get_queue_front(void);
    int pop_queue(void);
};
