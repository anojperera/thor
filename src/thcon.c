#include "thcon.h"
#include "thornifix.h"

#include <fcntl.h>
#include <curl/curl.h>

/* url memory struct */
struct _curl_mem
{
    char* memory;
    size_t size;
};

/* callback for handling content from curl lib */
static _thcon_copy_to_mem(void* contents, size_t size, size_t memb, void* usr_obj);


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

/* Get external ip address of self */
const char* thcon_get_my_addr(thcon* obj)
{
    CURL* _url_handle;
    CURLcode _res;
    struct _curl_mem _ip_buff;
    
    /* check for object */
    if(obj == NULL)
	return NULL;

    /* check for ip address url */
    if(obj->var_ip_addr_url[0] == '\0' || obj->var_ip_addr_url[0] == 0)
	return NULL;

    /* initialise the ip buffer */
    _ip_buff.memory = (char*) malloc(1);
    _ip_buff.size = 0;

    /* initialise global variables of curl */
    curl_global_init(CURL_GLOBAL_ALL);

    _url_handle = curl_easy_init();

    /* specify url to get */
    curl_easy_setopt(_url_handle, CURLOPT_URL, obj->var_ip_addr_url);

    /* set all data to the function */
    curl_easy_setopt(_url_handle, CURLOPT_WRITEFUNCTION, _thcon_copy_to_mem);

    /* set write data */
    curl_easy_setopt(_url_handle, CURLOPT_WRITEDATA, (void*) &_ip_buff);

    /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
    curl_easy_setopt(_url_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    _res = curl_easy_perform(_url_handle);
    memset(obj->_my_address, 0, THCON_SERVER_NAME_SZ);
    if(_res == CURLE_OK)
	{
	    strncpy(obj->_my_address, _ip_buff.memory, THCON_SERVER_NAME_SZ-1);
	    obj->_my_address[THCON_SERVER_NAME_SZ-1] = '\0';
	}

    
  
    
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

/* Curl message copy buffer */
static _thcon_copy_to_mem(void* contents, size_t size, size_t memb, void* usr_obj)
{
    size_t rel = size*memb;
    struct _curl_mem _mem = (struct _curl_mem*) usr_obj;

    /* allocate memory */
    _mem->memory = realloc(_mem->memory, _mem->size+rel+1);
    if(_mem->memory == NULL)
	return -1;

    /* copy to buffer */
    memcpy(&_mem->memory[_mem->size], contents, rel);
    _mem->size += rel;
    _mem->memory[_mem->size] = '\0';
    return rel;
}

