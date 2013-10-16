#include "thcon.h"
#include "thornifix.h"

#include <fcntl.h>

/* thread methods for handling start and clean up process */
static void* _thcon_thread_function(void* obj);
static void _thcon_thread_cleanup(void* obj);

/* create socket and for server mode bind to it */
static int _thcon_create_connection(thcon* obj, int _con_mode);
static int _thcon_make_socket_nonblocking(int sock_id);

/* constructor */
int thcon_init(thcon* obj)
{
    if(obj == NULL)
	return -1;
    memset((void*) obj, 0, sizeof(thcon));
    memset((void*) obj->var_port_name, 0, THCON_PORT_NAME_SZ);
    memset((void*) obj->var_svr_name, 0, THCON_SERVER_NAME_SZ);

    obj->var_con_sock = 0;
    obj->var_acc_sock = 0;
    obj->var_flg = 1;

    obj->var_num_conns = 0;
    obj->_var_cons_fds = NULL;
    return 0;
}

/* Destructor */
void thcon_delete(thcon* obj)
{
    /* delete socket fd array */
    if(obj->var_num_conns)
	free(obj->_var_cons_fds);
    obj->_var_cons_fds = NULL;
}

/* Return external ip address of the host */
const char* thcon_get_my_addr(thcon* obj)
{
    /* check for object connection */
    if(!obj)
	return NULL;

    
}


/*======================================================================*/
/* Private methods */
/* Connection mode 1 - server mode */
/* Connection mode 0 - client mode */
static int _thcon_create_connection(thcon* obj, int _con_mode)
{
    const char* _err_msg;
    int _stat;
    struct addrinfo *_result, *_p;

    /* initialise the address infor struct */
    memset(obj->_var_info, 0, sizeof(struct addrinfo));

    /* set hints */
    obj->_var_info.ai_family = AF_UNSPEC;
    obj->_var_info.ai_socktype = SOCK_STREAM;

    if(_con_mode)
	obj->_var_info.ai_flags = AI_PASSIVE;

    /* create address infor struct */
    _stat = getaddrinfo((_con_mode? obj->var_svr_name : NULL), obj->var_port_name, obj->_var_info, &_result);
    if(_stat != 0)
	{
	    _err_msg = gai_strerror(_stat);
	    THOR_LOG_ERROR(_err_msg);
	    return -1;
	}

    /* create socket for client mode, connect to the server */
    for(_p = _result; _p != NULL; _p = _p->next)
	{
	    obj->var_con_sock = socket(obj->_var_info.ai_family, obj->_var_info.ai_socktype, obj->_var_info.ai_protocol);
	    if(obj->var_con_sock == -1)
		continue;

	    /* if successful creating a socket, check the mode if client and connect to the host */
	    if(_con_mode == 0)
		{
		    if(connect(obj->var_con_sock, _p->ai_addr, _p->ai_addrlen) == -1)
			{
			    close(obj->var_con_sock);
			    continue;
			}
		    else
			break;
		}
	    else
		{
		    /* for server mode, bind socket to the address */
		    if(bind(obj->var_con_sock, _p->ai_addr, _p->ai_addrlen) == -1)
			{
			    close(obj->var_con_sock);
			    continue;
			}
		    else
			break;
		}
	}

    /* If the address info pointer is NULL that means we have exhausted the linked list
     * with no connection. Therefore we will terminate the program at this point */
    freeaddrinfo(_result);
    if(_p == NULL)
	{
	    THOR_LOG_ERROR("Unable to find a valid address");
	    return -1;
	}

    return 0;
}

/* Make the socket non blocking */
static int _thcon_make_socket_nonblocking(int sock_id)
{
    int _stat, _f_mode;

    /* get socket mode */
    _f_mode = fcntl(sock_id, F_GETFL);

    if(_f_mode == -1)
	{
	    THOR_LOG_ERROR("Unable get file mode info");
	    return -1;
	}

    _f_mode |= O_NONBLOCK;
    _stat = fcntl(sock_id, F_SETFL, _f_mode);
    if(_stat == -1)
	{
	    THOR_LOG_ERROR("Unable set file mode");
	    return -1;
	}

    return 0;
}
