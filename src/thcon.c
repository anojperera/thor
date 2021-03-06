#include "thcon.h"
#include "thornifix.h"

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>

#define THCON_CLIENT_RECV_SLEEP_TIME 100000			/* wait time for receiving */
#define THCON_MAX_CLIENTS 10 						/* maximum connections */
#define THCON_MAX_EVENTS 64							/* maximum events */
#define HTML_STACK_SZ 16
#define THCON_DEF_TIMEOUT 5							/* Default time out for geolocation */

#define THCON_DEFAULT_WOL_PORT 9

#define THCON_MAGIC_REPEAT 17
#define THCON_MAGIC_PACK_BUFF THCON_MAGIC_REPEAT*THCON_MAC_ADDR_BUFF
#define THCON_MAC_ADDR_STR_BUFF 64
#define THCON_MAC_ADDR_BASE 16						/* base 16 */

#define THCON_MAC_ADDR_DEL ":"
#define THCON_DEFAULT_SUBNET "255.255.255.255"

/* #define HTML_STACK_DEBUG_MODE */

/*---------------------------------------------------------------------------*/
/*
 *
 * Struct for handling parsing awkward free geolocation service from http://www.iplocation.net/
 * html from the site obtined by curl. Use libxml to parse the html. To narrow down to the GSP
 * coords, a software stack was used.
 * Stack to be allocated by parsing the configutaion file which should define how to navigate
 * the html to get to the elementes containing geolocation */

/* enumation to include what to do */
enum _html_nav_dir {
    _html_nav_into,
    _html_nav_side
};

/* element containing direction and number of items to navigate */
struct _html_nav_elm {
    unsigned int num;
    unsigned int act_cnt;
    enum _html_nav_dir dir;
};

/* main struct containing all instructions */
struct _html_parser_stack {
    unsigned int stack_ix;
    unsigned int stack_count;
    struct _html_nav_elm stack_elms[HTML_STACK_SZ];
};


/* macros for handling parser stack */
#define HTML_PARSER_TRY_INCREMENT(obj)				\
    ++(obj)->stack_ix < (obj)->stack_count? 0 : 1
#define HTML_PARSER_GET_DIR(obj)				\
    (obj)->stack_elms[(obj)->stack_ix].dir
#define HTML_PARSER_ELEM_INC_TRY(obj)									\
    (obj)->stack_elms[(obj)->stack_ix].act_cnt++ < (obj)->stack_elms[(obj)->stack_ix].num? 1 : 0

/* initialise elements in parser stack */
#define HTML_PARSER_ELEM_INIT(obj)					\
    for(i=0; i<(obj)->stack_count; i++)				\
	{												\
	    (obj)->stack_elms[i].num = 0;				\
	    (obj)->stack_elms[i].act_cnt = 0;			\
	    (obj)->stack_elms[i].dir = _html_nav_into;	\
	}

/* set element direction */
#define HTML_PARSER_ELEM_SET_DIR(obj, ix, nav_dir, nav_num)	\
    if(ix < (obj)->stack_count)								\
	{														\
	    (obj)->stack_elms[ix].dir = nav_dir;				\
	    (obj)->stack_elms[ix].num = nav_num;				\
	}

/*---------------------------------------------------------------------------*/

/* url memory struct */
struct _curl_mem
{
    char* memory;
    size_t size;
};

/* helper methods for sending magic packet to wake on lan device */
static int _thcon_conv_mac_addr_to_base16(thcon* obj);
static int _thcon_create_udp_socket(thcon* obj);
static int _thcon_send_magic_packet(thcon* obj);

/* callback for handling content from curl lib */
static int _thcon_copy_to_mem(void* contents, size_t size, size_t memb, void* usr_obj);


/* thread methods for handling start and clean up process */
static void* _thcon_thread_function_client(void* obj);
static void* _thcon_thread_function_server(void* obj);
static void* _thcon_thread_function_write_server(void* obj);

static void _thcon_thread_cleanup_server(void* obj);


/* create socket and for server mode bind to it */
static int _thcon_create_connection(thcon* obj, int _con_mode);
static int _thcon_make_socket_nonblocking(int sock_id);

/* send message (msg) of size (sz) to the socket pointed by fd */
static int _thcon_send_info(int fd, void* msg, size_t sz);

static int _thcon_get_url_content(const char* ip_addr, struct _curl_mem* mem);

static int _parse_html_geo(const struct _curl_mem* _mem, struct thcon_host_info* info);
static xmlNodePtr _get_html_tag_names(xmlNodePtr ptr, struct _html_parser_stack* stack);

/*
 * Accept connection on listening socket and add to the epoll instance.
 * connection socket will be created non blocking.
 */
static int _thcon_accept_conn(thcon* obj, int list_sock, int epoll_inst, struct epoll_event* event);

/*---------------------------------------------------------------------------*/
/*
 * Functions for reading and writing to the common buffer between test program and
 * clients.
 */
static int _thcon_write_to_int_buff(thcon* obj, int socket_fd);
static int _thcon_write_to_ext_buff(thcon* obj, int socket_fd);
/* static int _thcon_read_from_int_buff(thcon* obj, int socket_fd); */




/*---------------------------------------------------------------------------*/

/*
 * Internal file descriptor handling methods.
 */
inline __attribute__ ((always_inline)) static int _thcon_alloc_fds(thcon* obj);
inline __attribute__ ((always_inline)) static int _thcon_adjust_fds(thcon* obj, int fd);

/*---------------------------------------------------------------------------*/

/*
 * Callback method for queue destroy.
 */
static void _thcon_queue_del_helper(void* data);


/*===========================================================================*/

/* constructor */
int thcon_init(thcon* obj, thcon_mode mode)
{
    if(obj == NULL)
		return -1;
    memset((void*) obj, 0, sizeof(thcon));
    memset((void*) obj->var_port_name, 0, THCON_PORT_NAME_SZ);
    memset((void*) obj->var_svr_name, 0, THCON_SERVER_NAME_SZ);
	memset(obj->var_mac_addr, 0, THCON_MAC_ADDR_BUFF);
	memset(obj->var_subnet_addr, 0, THCON_PORT_NAME_SZ);
	memset(obj->var_mac_addr_str, 0, THCON_MAC_ADDR_STR_BUFF);

	strcpy(obj->var_subnet_addr, THCON_DEFAULT_SUBNET);

    obj->var_con_sock = 0;
    obj->var_acc_sock = 0;
    obj->_var_act_sock = 0;
	obj->var_wol_sock = 0;

    obj->_var_curl_timeout = 0;
    obj->var_flg = 1;

    obj->_var_con_stat = thcon_disconnected;
    obj->_var_con_mode = mode;

    obj->var_num_conns = 0;
    obj->var_geo_flg = 0;
    obj->var_ip_flg = 0;

	obj->var_inbuff_sz = 0;
	obj->var_outbuff_sz = 0;

	obj->var_membuff_in = NULL;
	obj->var_membuff_out = NULL;
    obj->_var_cons_fds = NULL;
    obj->_var_epol_inst = NULL;
    obj->_var_event_col = NULL;

    obj->var_my_info._init_flg = 0;
    obj->_ext_obj = NULL;
    obj->_thcon_recv_callback = NULL;
    obj->_thcon_write_callback = NULL;
    obj->_thcon_conn_made = NULL;
    obj->_thcon_conn_closed = NULL;

    /* initialise queue and locks */
    gqueue_new(&obj->_msg_queue, _thcon_queue_del_helper);
    sem_init(&obj->_var_sem, 0, 0);
    pthread_mutex_init(&obj->_var_mutex, NULL);
    pthread_mutex_init(&obj->_var_mutex_q, NULL);

    return 0;
}

/* Destructor */
void thcon_delete(thcon* obj)
{
    obj->_ext_obj = NULL;
    obj->_thcon_recv_callback = NULL;
    obj->_thcon_write_callback = NULL;
    obj->_thcon_conn_made = NULL;
    obj->_thcon_conn_closed = NULL;

	if(obj->var_membuff_in)
		free(obj->var_membuff_in);

	if(obj->var_membuff_out)
		free(obj->var_membuff_out);

	obj->var_membuff_in = NULL;
	obj->var_membuff_out = NULL;

    /* check scope */
    sem_destroy(&obj->_var_sem);
    pthread_mutex_destroy(&obj->_var_mutex);
    pthread_mutex_destroy(&obj->_var_mutex_q);

	/* if wake on lan socket was created close it */
	if(obj->var_wol_sock > 0)
		close(obj->var_wol_sock);

    /* delete socket fd array */
    if(obj->var_num_conns && obj->_var_cons_fds)
		free(obj->_var_cons_fds);
    obj->_var_cons_fds = NULL;
    obj->_var_epol_inst = NULL;
    obj->_var_event_col = NULL;

    /* delete queue */
    gqueue_delete(&obj->_msg_queue);

    return;
}

/* Get external ip address of self
   Uses libcurl to contact ip address set from
   configuration data to obtain the external
   ip address */
const char* thcon_get_my_addr(thcon* obj)
{
    int _rt_val = 0;
    struct _curl_mem _ip_buff;

    /* check for object */
    if(obj == NULL)
		return NULL;

    /* check for ip address url */
    if(obj->var_ip_flg > 0)
		return obj->var_my_info._my_address;

    if(obj->var_ip_addr_url[0] == '\0' || obj->var_ip_addr_url[0] == 0)
		return NULL;

    _rt_val = _thcon_get_url_content(obj->var_ip_addr_url, &_ip_buff);

    memset(obj->var_my_info._my_address, 0, THCON_SERVER_NAME_SZ);
    if(_rt_val == 0)
	{
	    strncpy(obj->var_my_info._my_address, _ip_buff.memory, THCON_SERVER_NAME_SZ-1);
	    obj->var_my_info._my_address[THCON_SERVER_NAME_SZ-1] = '\0';
	    obj->var_my_info._init_flg = 1;
	}

    /* clean up memory */
    if(_ip_buff.memory)
		free(_ip_buff.memory);

    if(_rt_val == 0)
		return obj->var_my_info._my_address;
    else
		return NULL;

    return NULL;
}

/* get geo location */
int thcon_get_my_geo(thcon* obj)
{
    int _rt_val = 0;
    struct _curl_mem _ip_buff;

    /* check for object */
    if(obj == NULL)
		return -1;

    if(obj->var_geo_addr_url[0] == '\0' || obj->var_geo_addr_url[0] == 0)
		return -1;

    /* Initialise the memory buffer */
    memset((void*) &_ip_buff, 0, sizeof(struct _curl_mem));
    _ip_buff.memory = NULL;
    _rt_val = _thcon_get_url_content(obj->var_geo_addr_url, &_ip_buff);
    if(_rt_val)
		return -1;
    _parse_html_geo(&_ip_buff, &obj->var_my_info);

    obj->var_geo_flg = 1;
    obj->var_ip_flg = 1;

    return 0;
}

/* Contact admin, uses the admin url pointed and uses libcurl to
 * submit self data as a form */
int thcon_contact_admin(thcon* obj, const char* admin_url)
{
    CURL* _curl;
    CURLcode _res;

    struct curl_httppost* _form_post = NULL;
    struct curl_httppost* _last_ptr = NULL;
    struct curl_slist* _header_list = NULL;
    static const char _buff[] = "Expect:";

    if(obj == NULL || admin_url == NULL)
		return -1;

    if(!obj->var_geo_flg && !obj->var_ip_flg)
		thcon_get_my_geo(obj);

    /* initialise curl */
    curl_global_init(CURL_GLOBAL_ALL);

    /* fill form data */
    curl_formadd(&_form_post,
				 &_last_ptr,
				 CURLFORM_COPYNAME, THCON_FORM_IP,
				 CURLFORM_COPYCONTENTS, obj->var_my_info._my_address,
				 CURLFORM_END);

    curl_formadd(&_form_post,
				 &_last_ptr,
				 CURLFORM_COPYNAME, THCON_GEO_COUNTRY,
				 CURLFORM_COPYCONTENTS, obj->var_my_info._country,
				 CURLFORM_END);

    curl_formadd(&_form_post,
				 &_last_ptr,
				 CURLFORM_COPYNAME, THCON_GEO_REGION,
				 CURLFORM_COPYCONTENTS, obj->var_my_info._my_region,
				 CURLFORM_END);

    curl_formadd(&_form_post,
				 &_last_ptr,
				 CURLFORM_COPYNAME, THCON_GEO_TOWN,
				 CURLFORM_COPYCONTENTS, obj->var_my_info._my_city,
				 CURLFORM_END);

    curl_formadd(&_form_post,
				 &_last_ptr,
				 CURLFORM_COPYNAME, THCON_GEO_LONG,
				 CURLFORM_COPYCONTENTS, obj->var_my_info._long,
				 CURLFORM_END);

    curl_formadd(&_form_post,
				 &_last_ptr,
				 CURLFORM_COPYNAME, THCON_GEO_LAT,
				 CURLFORM_COPYCONTENTS, obj->var_my_info._lat,
				 CURLFORM_END);

    /* initialise easy interface */
    _curl = curl_easy_init();

    _header_list = curl_slist_append(_header_list, _buff);
    if(!_curl)
		goto contact_admin_cleanup;

    /* set admin url */
    curl_easy_setopt(_curl, CURLOPT_URL, admin_url);
    curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _header_list);
    curl_easy_setopt(_curl, CURLOPT_HTTPPOST, _form_post);

    /* set time out */
    curl_easy_setopt(_curl, CURLOPT_TIMEOUT, obj->_var_curl_timeout);

    _res = curl_easy_perform(_curl);
    if(_res != CURLE_OK)
		THOR_LOG_ERROR("Unable to send information to admin");

    /* check for errors */
    curl_easy_cleanup(_curl);

contact_admin_cleanup:
    curl_formfree(_form_post);
    curl_slist_free_all(_header_list);

    return 0;
}

/* Start program */
int thcon_start(thcon* obj)
{
    if(obj == NULL)
		return -1;

    /*
     * If it runs in the server mode
     * 1. Create listening socket,
     * 2. make it non blocking,
     * 3. start epoll instance and listen for connections.
     */

    /* start server */
    pthread_create(&obj->_var_run_thread,
				   NULL,
				   (obj->_var_con_mode == thcon_mode_server? _thcon_thread_function_server : _thcon_thread_function_client),
				   (void*) obj);

    /*
     * If we are running in the server mode, call the write methods.
     */
    if(obj->_var_con_mode == thcon_mode_server)
	{
	    pthread_create(&obj->_var_svr_write_thread,
					   NULL,
					   _thcon_thread_function_write_server,
					   (void*) obj);
	}
	return 0;
}

/*
 * Stop connection and close all socekts.
 * On server mode write method is closed first and thread is joined.
 * Subsequently, reader method is joined to the main thread.
 * This is to prevent possible race conditions on socket descriptor
 * array and number of connection variables.
 */
int thcon_stop(thcon* obj)
{
    if(obj == NULL)
		return -1;

    /* If there is not server running, exit method */
    if(obj->_var_con_stat == thcon_disconnected)
		return -1;

    /* indicate server is stopped */
    obj->_var_con_stat = thcon_disconnected;


    /* post sem to proceed with quit */
    sem_post(&obj->_var_sem);

    /*
     * In the server mode, first stop and join the write operations.
     */
    if(obj->_var_con_mode != thcon_mode_client)
	{
	    pthread_cancel(obj->_var_svr_write_thread);
	    pthread_join(obj->_var_svr_write_thread, NULL);
	}

    /*
     * Stop connection handling mode and join to the main thread.
     */
    pthread_cancel(obj->_var_run_thread);
    pthread_join(obj->_var_run_thread, NULL);

    THOR_LOG_ERROR("thcon sucessfully freed");
    return 0;

}

/* send information to the socket */
int thcon_send_info(thcon* obj, void* data, size_t sz)
{
    unsigned int i = 0;
    if(obj == NULL || data == NULL)
		return -1;

    /* check if the connection was made*/
    if(obj->_var_con_stat == thcon_disconnected)
		return -1;

    /* call private method for sending the information */
    if(obj->_var_con_mode == thcon_mode_client)
		_thcon_send_info(obj->var_acc_sock, data, sz);
    else
	{
	    for(i = 0; i < obj->var_num_conns; i++)
			_thcon_send_info(obj->_var_cons_fds[i], data, sz);
	}

    return 0;
}

/*
 * Function duplicates data between all sockets.
 * May be use vmsplice, tee and splice for performance
 */
int thcon_multicast(thcon* obj, void* data, size_t sz)
{
    struct _curl_mem* _msg;

    /* check for argument pointers */
    if(!obj || !data || !sz)
		return -1;

    /* check it its running in the server mode */
    if(obj->_var_con_stat == thcon_disconnected)
		return -1;

    _msg = (struct _curl_mem*) malloc(sizeof(struct _curl_mem));
    _msg->memory = (char*) malloc(sz);
    memcpy((void*) _msg->memory, data, sz);
    _msg->size = sz;

    /*--------------------------------------------------*/
    /************* Mutex Lock This Section **************/
    pthread_mutex_lock(&obj->_var_mutex_q);
    gqueue_in(&obj->_msg_queue, (void*) _msg);
    pthread_mutex_unlock(&obj->_var_mutex_q);
    sem_post(&obj->_var_sem);
    /*--------------------------------------------------*/
    return 0;
}

/* send magic packet to device with mac address specified by mac_addr */
int thcon_wol_device(thcon* obj, const char* mac_addr)
{
	/* check for NULL pointer */
	if(obj == NULL || mac_addr == NULL)
		return -1;

	strcpy(obj->var_mac_addr_str, mac_addr);

	/* convert mac addr to base16 */
	if(_thcon_conv_mac_addr_to_base16(obj))
	{
		THOR_LOG_ERROR("unable to translate mac address to base16");
		return -1;
	}

	/* create UDP socket */
	if(_thcon_create_udp_socket(obj))
	{
		THOR_LOG_ERROR("unable to create a udp socket");
		return -1;
	}

	/* send magic packet */
	return _thcon_send_magic_packet(obj);
}

int thcon_create_raw_sock(thcon* obj)
{
	/* check for NULL pointer */
	if(obj == NULL)
		return -1;

	if(_thcon_create_connection(obj, obj->_var_con_mode))
		return -1;

	/* if all was successful, return the active socket */
	return obj->var_acc_sock;
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
    memset(&obj->_var_info, 0, sizeof(struct addrinfo));

    /* set hints */
    obj->_var_info.ai_family = AF_UNSPEC;
    obj->_var_info.ai_socktype = SOCK_STREAM;
    obj->_var_info.ai_protocol = 0;

    if(_con_mode)
		obj->_var_info.ai_flags = AI_PASSIVE;

    /* create address infor struct */
    _stat = getaddrinfo((_con_mode? NULL : obj->var_svr_name), obj->var_port_name, &obj->_var_info, &_result);
    if(_stat != 0)
	{
	    _err_msg = gai_strerror(_stat);
	    THOR_LOG_ERROR(_err_msg);
	    return -1;
	}

    /* create socket for client mode, connect to the server */
    for(_p = _result; _p != NULL; _p = _p->ai_next)
	{
		obj->var_con_sock = socket(_p->ai_family, _p->ai_socktype, _p->ai_protocol);
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

    /* client mode was selected set the accept socket same as connect socket */
    if(obj->_var_con_mode == thcon_mode_client)
		obj->var_acc_sock = obj->var_con_sock;

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
static int _thcon_copy_to_mem(void* contents, size_t size, size_t memb, void* usr_obj)
{
    size_t rel = size*memb;
    struct _curl_mem* _mem = (struct _curl_mem*) usr_obj;

    /* allocate memory */
    _mem->memory = (char*) realloc(_mem->memory, _mem->size+rel+1);
    if(_mem->memory == NULL)
		return 0;

    /* copy to buffer */
    memcpy(&_mem->memory[_mem->size], contents, rel);
    _mem->size += rel;
    _mem->memory[_mem->size] = '\0';
    return rel;
}

/* Get ip address content pointed to by ip_addr. The results are stored in
   struct struct _curl_mem pointed to by mem */
static int _thcon_get_url_content(const char* ip_addr, struct _curl_mem* mem)
{
    CURL* _url_handle;
    CURLcode _res;

    /* check for ip address url */
    if(ip_addr == NULL || mem == NULL)
		return -1;

    /* initialise the ip buffer */
    mem->memory = (char*) malloc(1);
    mem->size = 0;

    /* initialise global variables of curl */
    if(curl_global_init(CURL_GLOBAL_ALL))
		return -1;

    _url_handle = curl_easy_init();
    if(_url_handle == NULL)
		return -1;

    /* specify url to get */
    curl_easy_setopt(_url_handle, CURLOPT_URL, ip_addr);

    /* set all data to the function */
    curl_easy_setopt(_url_handle, CURLOPT_WRITEFUNCTION, _thcon_copy_to_mem);

    /* set write data */
    curl_easy_setopt(_url_handle, CURLOPT_WRITEDATA, (void*) mem);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(_url_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* Set timeout so that it wont stall here for long time */
    curl_easy_setopt(_url_handle, CURLOPT_TIMEOUT, THCON_DEF_TIMEOUT);

    _res = curl_easy_perform(_url_handle);
    if(_res != CURLE_OK)
		THOR_LOG_ERROR(curl_easy_strerror(_res));
    /* clean up memory */
    curl_easy_cleanup(_url_handle);
    curl_global_cleanup();
    if(_res == CURLE_OK)
		return 0;
    else
		return -1;
}

/* parse html geo location data */
static int _parse_html_geo(const struct _curl_mem* _mem, struct thcon_host_info* info)
{
    unsigned int i;
    const char* _search;
    xmlNodePtr _node, _node_ptr, _child_ptr;
    xmlNodePtr _t_node;
    htmlDocPtr _html_doc;
    char* _elm_val;

    struct _html_parser_stack _p_stack;
    struct _html_parser_stack _p_stack_x;

    _html_doc = htmlReadMemory(_mem->memory, _mem->size, "iplocation.net", NULL, HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET);
    _node = xmlDocGetRootElement(_html_doc);
    memset(&_p_stack, 0, sizeof(struct _html_parser_stack));

    _node_ptr = NULL;
    if(_node != NULL)
		_node_ptr = xmlFirstElementChild(_node);


    /*----------------------------------------------------------------------*/
    /*
     * This section to be handled by the configuration file
     *
     */

    _p_stack.stack_ix = 0;
    _p_stack.stack_count = 11;

    _p_stack_x.stack_ix = 0;
    _p_stack_x.stack_count = 1;

    /* initialise elements */
    HTML_PARSER_ELEM_INIT(&_p_stack);
    HTML_PARSER_ELEM_INIT(&_p_stack_x);

    /* set element values */
    HTML_PARSER_ELEM_SET_DIR(&_p_stack, 0, _html_nav_side, 1);
    HTML_PARSER_ELEM_SET_DIR(&_p_stack, 1, _html_nav_into, 1);
    HTML_PARSER_ELEM_SET_DIR(&_p_stack, 2, _html_nav_side, 6);

    HTML_PARSER_ELEM_SET_DIR(&_p_stack, 3, _html_nav_into, 3);
    HTML_PARSER_ELEM_SET_DIR(&_p_stack, 4, _html_nav_side, 1);
    HTML_PARSER_ELEM_SET_DIR(&_p_stack, 5, _html_nav_into, 4);
    HTML_PARSER_ELEM_SET_DIR(&_p_stack, 6, _html_nav_side, 1);
    HTML_PARSER_ELEM_SET_DIR(&_p_stack, 7, _html_nav_into, 6);
    HTML_PARSER_ELEM_SET_DIR(&_p_stack, 8, _html_nav_side, 20);
    HTML_PARSER_ELEM_SET_DIR(&_p_stack, 9, _html_nav_into, 1);
    HTML_PARSER_ELEM_SET_DIR(&_p_stack, 10, _html_nav_side, 3);

    HTML_PARSER_ELEM_SET_DIR(&_p_stack_x, 0, _html_nav_side, 7);

    /*----------------------------------------------------------------------*/

    /* iterate through the nodes */
    _search = NULL;
    while(_node_ptr)
	{
	    /* check if the node is the body element */
	    if(_node_ptr->name)
	    	_search = strstr((char*) _node_ptr->name, "body");

	    /* if its the body tag dive into the element and look for the first div */
	    if(_search != NULL)
		{
		    /* Get first row of location */
		    _t_node = _get_html_tag_names(_node_ptr, &_p_stack);
		    if(_t_node)
			{
			    i = 0;
			    _child_ptr = xmlFirstElementChild(_t_node);

			    while(_child_ptr)
				{
				    _elm_val = (char*) xmlNodeGetContent(_child_ptr);
				    switch(i)
					{
					case 0:
					    strncpy(info->_my_address, _elm_val, THCON_SERVER_NAME_SZ-1);
					    info->_my_address[THCON_SERVER_NAME_SZ-1] = '\0';
					    break;
					case 1:
					    strncpy(info->_country, _elm_val, THCON_GEN_INFO_SZ-1);
					    info->_country[THCON_GEN_INFO_SZ-1] = '\0';
					    break;
					case 2:
					    strncpy(info->_my_region, _elm_val, THCON_GEN_INFO_SZ-1);
					    info->_my_region[THCON_GEN_INFO_SZ-1] = '\0';
					    break;
					case 3:
					    strncpy(info->_my_city, _elm_val, THCON_GEN_INFO_SZ-1);
					    info->_my_city[THCON_GEN_INFO_SZ-1] = '\0';
					default:
					    break;
					}
				    _child_ptr =  xmlNextElementSibling(_child_ptr);
				    i++;
				}
			}

		    /* Get second table information */
			if(_t_node)
				_t_node = _get_html_tag_names(_t_node->parent, &_p_stack_x);

		    if(_t_node)
			{
			    i = 0;
			    _child_ptr = xmlFirstElementChild(_t_node);

			    while(_child_ptr)
				{
				    _elm_val = (char*) xmlNodeGetContent(_child_ptr);
				    switch(i)
					{
					case 1:
					    strncpy(info->_long, _elm_val, THCON_GEN_INFO_SZ-1);
					    info->_long[THCON_GEN_INFO_SZ-1] = '\0';
					    break;
					case 2:
					    strncpy(info->_lat, _elm_val, THCON_GEN_INFO_SZ-1);
					    info->_lat[THCON_GEN_INFO_SZ-1] = '\0';
					    break;
					default:
					    break;
					}

				    _child_ptr =  xmlNextElementSibling(_child_ptr);
				    i++;
				}
			}
		    break;
		}
		_node_ptr = xmlNextElementSibling(_node_ptr);
	    _search = NULL;
	}

    xmlFreeDoc(_html_doc);
    return 0;
}

/* get element based on stack */
static xmlNodePtr _get_html_tag_names(xmlNodePtr ptr, struct _html_parser_stack* stack)
{
    static xmlNodePtr _ptr = NULL;
    if(ptr == NULL || stack == NULL)
		return NULL;

    _ptr = xmlFirstElementChild(ptr);
    while(_ptr)
	{
#if defined HTML_STACK_DEBUG_MODE
	    printf ("Inside Child - %s\n", _ptr->name);
	stack_check_dir:
#endif
	    if(HTML_PARSER_GET_DIR(stack) == _html_nav_into)
		{
		    if(HTML_PARSER_ELEM_INC_TRY(stack))
				_get_html_tag_names(_ptr, stack);
		    goto stack_pop;
		}

	    if(HTML_PARSER_ELEM_INC_TRY(stack))
		{
		    _ptr = xmlNextElementSibling(_ptr);
		    continue;
		}

	stack_pop:
	    /* Increment stack counter */
	    if(HTML_PARSER_TRY_INCREMENT(stack))
			break;
#if defined HTML_STACK_DEBUG_MODE
	    else
			goto stack_check_dir;
#endif
	}

    return _ptr;
}

/*
 * Thread function for client.
 * This operation performed on non blocking sockets.
 */
static void* _thcon_thread_function_client(void* obj)
{
    thcon* _obj;
    int _stat = 0;
    int _cancel_state = 0;

    /* check object pointer */
    if(obj == NULL)
		return NULL;

    /* cast object pointer to connection type */
    _obj = (thcon*) obj;

    /* create socket and bind */
    if(_thcon_create_connection(_obj, thcon_mode_client))
		return NULL;

    /* make socket non blocking */
    if(_thcon_make_socket_nonblocking(_obj->var_acc_sock))
		return NULL;

    pthread_testcancel();

    /* indicate server is idling */
    _obj->_var_con_stat = thcon_connected;

    /* loop while connection is active and recieving messages */
    do
	{
	    pthread_testcancel();
	    _stat = _thcon_write_to_ext_buff(_obj, _obj->var_acc_sock);

	    /*
	     * Server has closed the connection therefore we exit the
	     * loop.
	     */
	    if(_stat == 0)
			break;

	    /* disable thread cancel state */
	    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &_cancel_state);

	    /*
	     * If a callback pointer was set, it shall be called immediately.
	     * Later on, the message should be queued and have the callback pop following
	     * execution.
	     */
	    if(_obj->_thcon_recv_callback && _stat > 0)
			_obj->_thcon_recv_callback(_obj->_ext_obj, _obj->var_membuff_out, _obj->var_outbuff_sz);

	    /* enable thread cancel state */
	    pthread_setcancelstate(_cancel_state, NULL);

	    pthread_testcancel();
	    /* sleep for 100ms to save processor cycle time */
	    usleep(THCON_CLIENT_RECV_SLEEP_TIME);
	}while(_stat);

    return NULL;
}

/* Send information to the socket pointed by data msg of size sz */
static int _thcon_send_info(int fd, void* msg, size_t sz)
{
    size_t _buff_sent = 0;

    /*
     * Send message in non blocking mode. Iterate until the message was sent.
     */
    do
	{
	    _buff_sent += send(fd, ((char*) msg)+_buff_sent, sz, MSG_DONTWAIT | MSG_NOSIGNAL);
	    if(_buff_sent == EPIPE)
			return -1;
	}while(_buff_sent < sz);

    return _buff_sent;
}

/*
 * Thread function for handling the server side of the object.
 * All connections are handled in a single thread using epoll.
 */
static void* _thcon_thread_function_server(void* obj)
{
    /* counters */
    int _i = 0, _n = 0, _old_state;
    int _e_sock = 0;
    int _stat = 0, _complete = 0;
    thcon* _obj;
    struct epoll_event _event, *_events = NULL;
    char _err_msg[THOR_BUFF_SZ];

    /* check for object pointer */
    if(obj == NULL)
		return NULL;

    /* Initialise error buffer */
    memset(_err_msg, 0, THOR_BUFF_SZ);

    /* Thread clean up handler. */
    pthread_cleanup_push(_thcon_thread_cleanup_server, obj);

    /* Disable thread cancelling temporarily */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &_old_state);

    /* cast object pointer to the correct type */
    _obj = (thcon*) obj;

    /* create socket and bind */
    if(_thcon_create_connection(_obj, thcon_mode_server))
		return NULL;

    /* make socket non blocking */
    if(_thcon_make_socket_nonblocking(_obj->var_con_sock))
		goto listening_sock_exit;

    /* listen on socket */
    if(listen(_obj->var_con_sock, THCON_MAX_CLIENTS))
		goto listening_sock_exit;

    /* create and epoll instance */
    _e_sock = epoll_create1(0);

    if(_e_sock == -1)
		goto epoll_exit_lbl;
    _obj->_var_epol_inst = &_e_sock;
    _event.data.fd = _obj->var_con_sock;
    _event.events = EPOLLIN | EPOLLET;

    if(epoll_ctl(_e_sock, EPOLL_CTL_ADD, _obj->var_con_sock, &_event))
		goto epoll_exit_lbl;

    /* Restore thread cancelling */
    pthread_setcancelstate(_old_state, NULL);
    pthread_testcancel();
    _events = (struct epoll_event*) calloc(THCON_MAX_EVENTS, sizeof(struct epoll_event));
    _obj->_var_event_col = (void*) _events;

    /* allocate memory for the incomming connections */
    _obj->_var_cons_fds = (int*) calloc(THCON_MAX_CLIENTS, sizeof(int));

    /* indicate server is idling */
    _obj->_var_con_stat = thcon_connected;

    /* main event loop */
    while(1)
	{
	    /* check for cancel here */
	    pthread_testcancel();

	    _n = epoll_wait(_e_sock, _events, THCON_MAX_EVENTS, -1);
	    for(_i = 0; _i < _n; _i++)
		{
		    _complete = 0;

		    /* check for errors */
		    if(((_events[_i].events & EPOLLERR) ||
				(_events[_i].events & EPOLLHUP)) &&
		       (!(_events[_i].events & EPOLLIN)))
			{
				/*
				 * If the socket is not the listening socket,
				 * we close the file descriptor and remove the
				 * socket from the socket array.
				 */
			    if(_events[_i].data.fd != _obj->var_con_sock)
				{
					/*
					 * Number of connections are decremented by the
					 * adjust file descriptor methods.
					 * The all operations involved in object variables
					 * are wrapped in mutex for thread safety.
					 */
				    _thcon_adjust_fds(_obj, _events[_i].data.fd);
				    close(_events[_i].data.fd);

				    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &_old_state);
				    pthread_setcancelstate(_old_state, NULL);

				    /*
				     * Indicate, the connection object was closed.
				     */
				    if(_obj->_thcon_conn_closed)
						_obj->_thcon_conn_closed(_obj->_ext_obj, obj, _events[_i].data.fd);
				}
			    else
				{
					/* errors have occured */
					THOR_LOG_ERROR("epoll error");
				}

			    pthread_testcancel();
			    continue;
			}
		    else if((_events[_i].events & EPOLLIN) ||
					(_events[_i].events & EPOLLRDHUP))
			{
			    if(_obj->var_con_sock == _events[_i].data.fd)
				{
				    /*
				     * Information on listening socket, means we have a connection.
				     * Call internal method to haddle the incomming connection and add
				     * to the epoll instance.
				     */
				    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &_old_state);
				    _thcon_accept_conn(_obj, _obj->var_con_sock, _e_sock, &_event);


				    /*
				     * Fire callback to indicate connection was established and that
				     * the sys object should be started.
				     */
				    if(_obj->_thcon_conn_made)
						_obj->_thcon_conn_made(_obj->_ext_obj, obj);

				    pthread_setcancelstate(_old_state, NULL);
				    pthread_testcancel();
				    continue;
				}
			    else
				{

				    /*
				     * Data on socket waiting to be read. All data shall be read
				     * in a single pass. Since we are running on edge triggered mode
				     * in epoll, we wont get a notification again.
				     */
				    while(1)
					{
					    /*
					     * Active socket is set. When recv callback is called,
					     * a user will be able to query the socket which the message
					     * came through.
					     */
					    _obj->_var_act_sock = _events[_i].data.fd;

					    _stat = _thcon_write_to_int_buff(_obj, _events[_i].data.fd);
					    if(_obj->_thcon_recv_callback && _stat > 0)
							_obj->_thcon_recv_callback(_obj->_ext_obj, _obj->var_membuff_in, _obj->var_inbuff_sz);
					    if(_stat == -1)
						{
						    /*
						     * if error is EAGAIN, we have read all data, go back to
						     * the main loop.
						     */
						    if(errno != EAGAIN)
								_complete = 1;

						    break;
						}
					    else if(_stat == 0)
						{
						    _complete = 1;
						    break;
						}
					}

				}
			}
		    if(_complete)
			{
			    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &_old_state);

			    /*
			     * Number of connections are decremented by the
			     * adjust file descriptor methods.
			     * The all operations involved in object variables
			     * are wrapped in mutex for thread safety.
			     */
			    _thcon_adjust_fds(_obj, _events[_i].data.fd);


			    /*
			     * Indicate, the connection object was closed.
			     */
			    if(_obj->_thcon_conn_closed)
					_obj->_thcon_conn_closed(_obj->_ext_obj, obj, _events[_i].data.fd);

			    /* close connection so that epoll shall remove the watching descriptor */
			    close(_events[_i].data.fd);
				pthread_setcancelstate(_old_state, NULL);
			    pthread_testcancel();
			}
		}
	}

    /*
     * Pthread clean up pop handler not executed when this point is reached
     * natuarally.
     */
    pthread_cleanup_pop(0);
    free(_events);

epoll_exit_lbl:
    if(_e_sock)
		close(_e_sock);

listening_sock_exit:
    if(_obj->var_con_sock)
		close(_e_sock);

    pthread_exit(NULL);
    return NULL;
}

/* Accept connection */
static int _thcon_accept_conn(thcon* obj, int list_sock, int epoll_inst, struct epoll_event* event)
{
    struct sockaddr _in_addr;
    socklen_t _in_len=0;
    int _fd=0, _stat=0;
    char _err_msg[THOR_BUFF_SZ];

    char _hbuf[NI_MAXHOST], _sbuf[NI_MAXSERV];
    memset((void*) &_in_addr, 0, sizeof(struct sockaddr));
    memset(_err_msg, 0, THOR_BUFF_SZ);
    while(1)
	{
	    _fd = accept(list_sock, &_in_addr, &_in_len);
	    if(_fd == -1)
		{
		    if(errno == EAGAIN ||
		       errno == EWOULDBLOCK)
			{
			    /*
			     * Finished processing all incoming connections
			     * break loop and exit.
			     */
			    break;
			}
		    else
			{
				THOR_LOG_ERROR("Accept handling error\n");
			    break;
			}
		}


	    /* get information about the connection and log */
	    _stat = getnameinfo(&_in_addr, _in_len,
							_hbuf, NI_MAXHOST,
							_sbuf, NI_MAXSERV,
							NI_NUMERICHOST | NI_NUMERICSERV);

	    if(_stat == 0)
		{
		    sprintf(_err_msg, "Accepted connection from %s on port %s\n", _hbuf, _sbuf);
		    THOR_LOG_ERROR(_err_msg);
		}
	    /* make the connection non blocking  and add to the epoll instance */
	    _thcon_make_socket_nonblocking(_fd);

	    event->data.fd = _fd;
	    event->events = EPOLLIN | EPOLLET;
	    epoll_ctl(epoll_inst, EPOLL_CTL_ADD, _fd, event);

	    /*
	     * Check if buffer needs expanding or not and set fd value.
	     */
	    _thcon_alloc_fds(obj);

	    /* counter incremented in a mutex */
	    pthread_mutex_lock(&obj->_var_mutex);
	    obj->_var_cons_fds[obj->var_num_conns++] = _fd;

	    /*
	     * Set the active socket so that a user may be able to
	     * query the socket which the new connection was made.
	     */
	    obj->_var_act_sock = _fd;

	    /* Display message in debug mode */
	    sprintf(_err_msg, "Connection made on socket: %i\n", obj->_var_cons_fds[obj->var_num_conns-1]);
	    THOR_LOG_ERROR(_err_msg);

	    pthread_mutex_unlock(&obj->_var_mutex);
	}

    return 0;
}

/* Write data on socket to the buffer */
static int _thcon_write_to_int_buff(thcon* obj, int socket_fd)
{
    int _sz = 0;
	obj->var_inbuff_sz = 0;

	/* free the buffer if it exists */
	if(obj->var_membuff_in)
		free(obj->var_membuff_in);
	obj->var_membuff_in = NULL;

	/* allocate and memset the initial buffer */
	obj->var_membuff_in = (char*) malloc(sizeof(char) * THORNIFIX_MSG_BUFF_SZ);
    memset((void*) obj->var_membuff_in, 0, THORNIFIX_MSG_BUFF_SZ);
	do {
		_sz = read(socket_fd,
				   obj->var_membuff_in + obj->var_inbuff_sz,
				   THORNIFIX_MSG_BUFF_SZ);

		/* break out of the loop if its an error */
		if(_sz == -1)
			break;

		obj->var_inbuff_sz += _sz;

		/* reallocate buffer if read size is more than half of the initial buffer */
		if(((float) _sz) > (float) (THORNIFIX_MSG_BUFF_SZ / 2)) {
			obj->var_membuff_in = (char*)
				realloc(obj->var_membuff_in, (obj->var_inbuff_sz + THORNIFIX_MSG_BUFF_SZ));
		}

	} while(_sz > 0);

	obj->var_membuff_in[obj->var_inbuff_sz] = '\0';
	if(_sz == -1 && obj->var_inbuff_sz > 0)
		return obj->var_inbuff_sz;
	else
		return _sz;
}

/* Write data on socket to the external buffer */
static int _thcon_write_to_ext_buff(thcon* obj, int socket_fd)
{
	int _sz = 0;
	obj->var_outbuff_sz = 0;

	/* Free the buffer if its allocated */
	if(obj->var_membuff_out)
		free(obj->var_membuff_out);
	obj->var_membuff_out = NULL;

	/* allocate and memset the buffer */
	obj->var_membuff_out = (char*) malloc(sizeof(char) * THORNIFIX_MSG_BUFF_SZ);
	memset((void*) obj->var_membuff_out, 0, THORNIFIX_MSG_BUFF_SZ);
	do {
		_sz = recv(socket_fd,
				   obj->var_membuff_out + obj->var_outbuff_sz,
				   THORNIFIX_MSG_BUFF_SZ,
				   MSG_DONTWAIT);

		/* break out of the loop if its an error */
		if(_sz == -1)
			break;

		obj->var_outbuff_sz += _sz;

		/* reallocate buffer if the read size is more than half of the initial buffer */
		if((float) _sz > (float) (THORNIFIX_MSG_BUFF_SZ / 2)) {
			obj->var_membuff_out = (char*)
				realloc(obj->var_membuff_out, (obj->var_outbuff_sz + THORNIFIX_MSG_BUFF_SZ));
		}

	} while(_sz > 0);

	obj->var_membuff_out[obj->var_outbuff_sz] = '\0';
	if(_sz == -1 && obj->var_outbuff_sz > 0)
		return obj->var_outbuff_sz;
	else
		return _sz;
}


/* Alocate memory */
inline __attribute__ ((always_inline)) static int _thcon_alloc_fds(thcon* obj)
{
    int _t_exs_sz;
    int* _t_buff;

    /*
     * Mutex is locked for the entire session. This is due to the
     * num connection variable being accessed in different places.
     */
    pthread_mutex_lock(&obj->_var_mutex);

    /*
     * If the counter is 0, then its the first connection,
     * set the initial size of the buffer and create it
     */
    if(obj->var_num_conns == 0)
	{
	    obj->_var_bf_sz = THCON_MAX_CLIENTS;
	    obj->_var_cons_fds = (int*) calloc(obj->_var_bf_sz, sizeof(int));
	}

    /*
     * Check if the number of connections has reached 80% of the buffer.
     * If it has, create a new buffer of double the size of existing buffer
     * and copy the contents of existing buffer to the new one. Subsequently
     * detroy the existing buffer.
     */
    if(obj->var_num_conns > ((int) 0.8*obj->_var_bf_sz))
	{
	    /* Record existing size */
	    _t_exs_sz = obj->_var_bf_sz;

	    /* New buffer size */
	    obj->_var_bf_sz += obj->_var_bf_sz;

	    /*
	     * Create a new temporary buffer and copy the contents of existing
	     * buffer to the new one.
	     */
	    _t_buff = (int*) calloc(obj->_var_bf_sz, sizeof(int));
	    memcpy(_t_buff, obj->_var_cons_fds, sizeof(int) * _t_exs_sz);

	    /*--------------------------------------------------*/
	    /************* Mutex Lock This Section **************/
	    /*
	     * Free the existing buffer and set the new pointer to object
	     * buffer pointer
	     */
	    free(obj->_var_cons_fds);
	    obj->_var_cons_fds = _t_buff;
	    /*--------------------------------------------------*/
	}
    pthread_mutex_unlock(&obj->_var_mutex);
    return 0;
}

/*
 * Adjust file descriptor internal buffer when connections are closed.
 * When connections are closed, epoll instance will auto remove the descriptor
 * from the queue. This method will remove the descriptor from the internal
 * list and adjust the memory buffer if required.
 */
inline __attribute__ ((always_inline)) static int _thcon_adjust_fds(thcon* obj, int fd)
{
    int* _t_buff;
    unsigned int i, a;

    /* check if connection count is 0, exit method */
    pthread_mutex_lock(&obj->_var_mutex);
    if(obj->var_num_conns < 1)
		goto _thcon_adjust_fds_exit;

    /* allocate memory for the new buffer */
    _t_buff = (int*) calloc((obj->var_num_conns-1), sizeof(int));

    /* copy existing descriptors to the new buffer */
    for(i=0,a=0; i<obj->var_num_conns; i++)
	{
	    /*
	     * If the closing file descriptor was found,
	     * do not copy it.
	     */
	    if(obj->_var_cons_fds[i] == fd)
			continue;
	    _t_buff[a++] = obj->_var_cons_fds[i];
	}

    /* free internal buffer and assign new buffer */
    /*--------------------------------------------------*/
    /************* Mutex Lock This Section **************/
    free(obj->_var_cons_fds);
    obj->_var_cons_fds = _t_buff;
    /*--------------------------------------------------*/

    /* Decrement number of connections */
    obj->var_num_conns--;
_thcon_adjust_fds_exit:

    pthread_mutex_unlock(&obj->_var_mutex);
    return 0;
}


/*
 * The thread function peeks at the queue. If the messages exist in
 * the queue, its written to the socket.
 */
static void* _thcon_thread_function_write_server(void* obj)
{
    unsigned int i;
    int _old_state;
    thcon* _obj;
    struct _curl_mem* _msg;

    if(obj == NULL)
		return NULL;

    /* cast argument to correct object pointer */
    _obj = (thcon*) obj;

    while(1)
	{
	    pthread_testcancel();

	    /* wait on semaphore */
	    sem_wait(&_obj->_var_sem);

	    pthread_testcancel();

	    /*
	     * If the message queue is empty we continue the loop
	     * and wait on semaphore until, posted from enqueu
	     * method.
	     */
	    if(gqueue_count(&_obj->_msg_queue) == 0)
			continue;

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &_old_state);

	    /* Pop message from queue */
	    pthread_mutex_lock(&_obj->_var_mutex_q);
	    gqueue_out(&_obj->_msg_queue, (void**) &_msg);
	    pthread_mutex_unlock(&_obj->_var_mutex_q);

	    /*
	     * Write to all sockets. Cancellation state is disable between the write.
	     */
	    /*--------------------------------------------------*/
	    /************* Mutex Lock This Section **************/
	    pthread_mutex_lock(&_obj->_var_mutex);
	    for(i = 0; i < _obj->var_num_conns; i++)
	    	_thcon_send_info(_obj->_var_cons_fds[i], _msg->memory, _msg->size);
	    pthread_mutex_unlock(&_obj->_var_mutex);
	    /*--------------------------------------------------*/

	    /* free memory */
	    free(_msg->memory);
	    free(_msg);
	    _msg->memory = NULL;
	    _msg = NULL;

   	    pthread_setcancelstate(_old_state, NULL);
	}

    return NULL;
}

/*
 * Thread cleanup handler for the server method.
 */
static void _thcon_thread_cleanup_server(void* obj)
{
    unsigned int i;
    thcon* _obj;

    if(obj == NULL)
		return;

    /* Cast to correct object */
    _obj = (thcon*) obj;

    if(_obj->_var_event_col != NULL)
		free(_obj->_var_event_col);
    _obj->_var_event_col = NULL;

    /* close open connections */
    for(i=0; i<_obj->var_num_conns; i++)
		close(_obj->_var_cons_fds[i]);
    /*
     * All open file descriptors are closed.
     * This thread join should proceed, closing the write method.
     */
    free(_obj->_var_cons_fds);
    _obj->var_num_conns = 0;


    if(_obj->_var_epol_inst)
		close(*_obj->_var_epol_inst);
    _obj->_var_epol_inst = NULL;

    /* create connection socekt */
    if(_obj->var_con_sock)
		close(_obj->var_con_sock);

    return;

}

/*
 * Delete helper method for clearing queue.
 */
static void _thcon_queue_del_helper(void* data)
{
    struct _curl_mem* _mem;
    if(!data)
		return;

    /* cast data object to */
    _mem = (struct _curl_mem*) data;

    /* check and free buffers */
    if(_mem->memory && _mem->size)
		free(_mem->memory);
    _mem->memory = NULL;

    /* free object itself */
    free(_mem);

    return;
}

/*
 * convert to base16 from string
 */
static int _thcon_conv_mac_addr_to_base16(thcon* obj)
{
	unsigned int _cnt = 0;
	char* _tok = NULL;
	char _buff[THCON_MAC_ADDR_STR_BUFF];

	/* initialise the buffer */
	memset(obj->var_mac_addr, 0, THCON_MAC_ADDR_BUFF);
	memset(_buff, 0, THCON_MAC_ADDR_BUFF);

	strncpy(_buff, obj->var_mac_addr_str, THCON_MAC_ADDR_STR_BUFF-1);
	_buff[THCON_MAC_ADDR_STR_BUFF-1] = '\0';

	/* split the string by delimiter  */
	_tok = strtok(_buff, THCON_MAC_ADDR_DEL);
	while(_tok)
	{
		/* check buffer for overflow */
		if(_cnt >= THCON_MAC_ADDR_BUFF)
			break;
		obj->var_mac_addr[_cnt] = strtol(_tok, NULL, THCON_MAC_ADDR_BASE);
		_tok = strtok(NULL, THCON_MAC_ADDR_DEL);
		_cnt++;
	}

	return 0;
}

/* create UDP socket */
static int _thcon_create_udp_socket(thcon* obj)
{
	int _opt_val = 1;

	THOR_LOG_ERROR("creating UDP socket for sending magic packet");
	/* check if socket is connected */
	if(obj->var_wol_sock > 0)
		close(obj->var_wol_sock);
	if((obj->var_wol_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		return -1;

	/* set socket options */
	if(setsockopt(obj->var_wol_sock, SOL_SOCKET, SO_BROADCAST, &_opt_val, sizeof(int)) < 0)
		return -1;
	return 0;
}

/* send magic packet to the mac address specified */
static int _thcon_send_magic_packet(thcon* obj)
{
	struct sockaddr_in _addr;
	unsigned char _magic_packet[THCON_MAGIC_PACK_BUFF] = {};
	unsigned char* _packet_ptr = NULL;
	int _i = 0;

	memset(&_addr, 0, sizeof(struct sockaddr_in));

	_addr.sin_family = AF_INET;
	_addr.sin_port = htons(THCON_DEFAULT_WOL_PORT);

	if(inet_aton(obj->var_subnet_addr, &_addr.sin_addr) == 0)
		return -1;


	_packet_ptr = _magic_packet;
	memset(_magic_packet, 0xFF, THCON_MAC_ADDR_BUFF);
	_packet_ptr += THCON_MAC_ADDR_BUFF;

	for(_i = 0; _i < THCON_MAGIC_REPEAT-1; _i++) {
		memcpy(_packet_ptr, obj->var_mac_addr, THCON_MAC_ADDR_BUFF);
		_packet_ptr += THCON_MAC_ADDR_BUFF;
	}

	if(sendto(obj->var_wol_sock, _magic_packet, THCON_MAGIC_PACK_BUFF, 0, (struct sockaddr *) &_addr, sizeof(struct sockaddr)) < 0)
		return -1;
	THOR_LOG_ERROR("magic packet sent");
	return 0;
}
