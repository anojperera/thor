#include "thcon.h"
#include "thornifix.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>

#define THCON_CLIENT_RECV_SLEEP_TIME 100000			/* wait time for receiving */
#define THCON_MAX_CLIENTS 10 					/* maximum connections */
#define THCON_MAX_EVENTS 64					/* maximum events */
#define HTML_STACK_SZ 16

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
#define HTML_PARSER_TRY_INCREMENT(obj)			\
    ++(obj)->stack_ix < (obj)->stack_count? 0 : 1
#define HTML_PARSER_GET_DIR(obj)		\
    (obj)->stack_elms[(obj)->stack_ix].dir
#define HTML_PARSER_ELEM_INC_TRY(obj)					\
    (obj)->stack_elms[(obj)->stack_ix].act_cnt++ < (obj)->stack_elms[(obj)->stack_ix].num? 1 : 0

/* initialise elements in parser stack */
#define HTML_PARSER_ELEM_INIT(obj)			\
    for(i=0; i<(obj)->stack_count; i++)			\
	{						\
	    (obj)->stack_elms[i].num = 0;		\
	    (obj)->stack_elms[i].act_cnt = 0;		\
	    (obj)->stack_elms[i].dir = _html_nav_into;	\
	}

/* set element direction */
#define HTML_PARSER_ELEM_SET_DIR(obj, ix, nav_dir, nav_num)	\
    if(ix < (obj)->stack_count)					\
	{							\
	    (obj)->stack_elms[ix].dir = nav_dir;		\
	    (obj)->stack_elms[ix].num = nav_num;		\
	}

/*---------------------------------------------------------------------------*/

/* url memory struct */
struct _curl_mem
{
    char* memory;
    size_t size;
};

/* callback for handling content from curl lib */
static _thcon_copy_to_mem(void* contents, size_t size, size_t memb, void* usr_obj);


/* thread methods for handling start and clean up process */
static void* _thcon_thread_function_client(void* obj);
static void* _thcon_thread_function_server(void* obj);
static void* _thcon_thread_function_write_server(void* obj);

static void _thcon_thread_cleanup_server(void* obj);
static void _thcon_thread_cleanup_client(void* obj);



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
static int _thcon_read_from_int_buff(thcon* obj, int socket_fd);
/*
 *
/*---------------------------------------------------------------------------*/

/*
 * Internal file descriptor handling methods.
 */
inline __attribute__ (always_inline) static int _thcon_alloc_fds(thcon* obj);
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

    obj->var_con_sock = 0;
    obj->var_acc_sock = 0;
    obj->var_flg = 1;

    obj->_var_con_stat = thcon_disconnected;
    obj->_var_con_mode = mode;

    obj->var_num_conns = 0;
    obj->var_geo_flg = 0;
    obj->var_ip_flg; = 0;
    
    obj->_var_cons_fds = NULL;

    obj->var_my_info._init_flg = 0;
    obj->_ext_obj = NULL;
    obj->_thcon_recv_callback = NULL;
    obj->_thcon_write_callback = NULL;

    /* initialise queue */
    sem_init(&obj->_var_sem, 0, 0);
    pthread_mutex_init(&obj->_var_mutex, NULL);
    pthread_mutex_init(&obj->_var_mutex_q, NULL);
    gqueue_new(&obj->_msg_queue, _thcon_queue_del_helper);
    
    return 0;
}

/* Destructor */
void thcon_delete(thcon* obj)
{
    obj->_ext_obj = NULL;
    obj->_thcon_recv_callback = NULL;
    obj->_thcon_write_callback = NULL;
    
    /* check scope */
    sem_destroy(obj->_var_sem);
    pthread_mutex_destroy(&obj->_var_mutex);
    pthread_mutex_destroy(&obj->_var_mutex_q);
    
    /* delete socket fd array */
    if(obj->var_num_conns && obj->_var_cons_fds)
	free(obj->_var_cons_fds);
    obj->_var_cons_fds = NULL;

    /* delete queue */
    gqueue_delete(&obj->_msg_queue);
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
	return obj->_my_address;
    else
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
    

    _res = curl_easy_perform(_curl);

    /* check for errors */
    curl_easy_cleanup(_curl);
    
 contact_admin_cleanup:
    curl_formfree(_form_post);
    curl_slist_free_all(_header_list);

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

/* send information to the socket */
int thcon_send_info(thcon* obj, void* data, sizt_t sz)
{
    int i = 0;
    if(obj == NULL || data == NULL)
	return -1;

    /* check if the connection was made*/
    if(obj->_var_con_stat == thcon_disconnected)
	return -1;
    
    /* call private method for sending the information */
    for(i = 0; i < obj->var_num_conns; i++)
	_thcon_send_info(obj->_var_cons_fds[i], data, sz);
}

/*
 * Function duplicates data between all sockets.
 * May be use vmsplice, tee and splice for performance
 */
int thcon_multicast(thcon* obj, void* data, size_t sz)
{
    struct _curl_mem* _msg;
    int i;
    
    /* check for argument pointers */
    if(!obj || !data || !sz)
	return -1;
    
    /* check it its running in the server mode */
    if(obj->_var_con_mode == thcon_disconnected)
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
    /*--------------------------------------------------*/
    return 0;
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

    /* client mode was selected set the accept socket same as connect socket */
    if(obj->_var_con_mode == thcon_mode_client)
	obj->var_acc_sock = obj->var_con_sock;
    
    obj->_var_con_stat = thcon_connected;
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
    curl_global_init(CURL_GLOBAL_ALL);

    _url_handle = curl_easy_init();

    /* specify url to get */
    curl_easy_setopt(_url_handle, CURLOPT_URL, ip_addr);

    /* set all data to the function */
    curl_easy_setopt(_url_handle, CURLOPT_WRITEFUNCTION, _thcon_copy_to_mem);

    /* set write data */
    curl_easy_setopt(_url_handle, CURLOPT_WRITEDATA, (void*) mem);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(_url_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    _res = curl_easy_perform(_url_handle);

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
    int i;
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
			    i = 0
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
				    _child_ptr =  xmlNextElementSibling(_ptr);
				    i++;
				}
			}

		    /* Get second table information */
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

				    _child_ptr =  xmlNextElementSibling(_ptr);
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
    if(ptr == NULL || stack == NULL) return 0;
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
    char _t_buff[THORNIFIX_MSG_BUFF_SZ];
    
    /* check object pointer */
    if(obj == NULL)
	return NULL;

    /* initialise buffer */
    memset((void*) _t_buff, 0, THORNIFIX_MSG_BUFF_SZ);
    
    /* cast object pointer to connection type */
    _obj = (thcon*) obj;
    
    pthread_testcancel(void);
    
    /* loop while connection is active and recieving messages */
    do
	{
	    pthread_testcancel(void);
	    _stat = recv(_obj->var_acc_sock, _t_buff, THORNIFIX_MSG_BUFF_SZ, MSG_DONTWAIT);


	    /* disable thread cancel state */
	    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &_cancel_state);
	    
	    /*
	     * If a callback pointer was set, it shall be called immediately.
	     * Later on, the message should be queued and have the callback pop following
	     * execution.
	     */
	    if(_obj->_thcon_recv_callback)
		_obj->_thcon_recv_callback(_obj->_ext_obj, _t_buff, _stat);

	    /* enable thread cancel state */
	    pthread_setcancelstate(_cancel_state, NULL);
	    
	    pthread_testcancel(void);
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
     *
     */
    do
	{
	    _buff_sent += send(fd, data+_buff_sent, sz, MSG_DONTWAIT);
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
    int _i = 0, _n = 0;
    int _e_sock = 0;
    int _stat = 0, _complete = 0;
    thcon* _obj;
    struct epoll_event _event, *_events;
    
    /* check for object pointer */
    if(obj == NULL)
	return NULL;

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
	goto epoll_exit;

    _event.data.fd = _obj->var_con_sock;
    _event.events = EPOLLIN | EPOLLET;

    if(epoll_ctl(_e_sock, EPOLL_CTL_ADD, _obj->var_con_sock, &_event))
	goto epol_exit;

    _events = calloc(THCON_MAX_EVENTS, sizeof(struct epoll_event));

    /* allocate memory for the incomming connections */
    obj->_var_cons_fds = (int*) calloc(THCON_MAX_CLIENTS, sizeof(int));
    /* main event loop */
    while(1)
	{
	    _n = epoll_wait(_e_sock, _events, THCON_MAX_EVENTS, -1);
	    for(_i = 0; _i < _n; _i++)
		{
		    _complete = 0;
		    
		    /* check for errors */
		    if((_events[_i].events & EPOLLERR) ||
		       (_events[_i].events & EPOLLHUP) ||
		       (_events[_i].events & EPOLLIN))
			{
			    /* errors have occured */
			    fprintf(stderr, "epoll error\n");
			    if(_events[i].data.fd != _obj->var_con_sock)
				{
				    _thcon_adjust_fds(obj, _events[i].data.fd);
				    close(_events[i].data.fd);
				    obj->var_num_conns--;
				}
			    continue;
			}
		    else if(_obj->var_con_sock == _events[_i].data.fd)
			{
			    /*
			     * Information on listening socket, means we have a connection.
			     * Call internal method to haddle the incomming connection and add
			     * to the epoll instance.
			     */
			    _thcon_accept_conn(obj, _obj->var_con_sock, _e_sock, &_event);
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
				    _stat = _thcon_write_to_int_buff(obj, _events[i].data.fd);
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
		    if(_complete)
			{
			    /* remote client has closed the connection */
			    fprintf(stdout, "Connection closed on socket - %i\n", _event[i].data.fd);
			    _thcon_adjust_fds(obj, _event[i].data.fd);
			    obj->var_num_conns--;
			    
			    /* close connection so that epoll shall remove the watching descriptor */
			    close(_event[i].data.fd);
			}
		}
	}

 epoll_exit:
    if(_e_sock)
	close(_e_sock);

 listening_sock_exit:
    if(_obj->var_con_sock)
	close(_e_sock);

    return NULL;
}

/* Accept connection */
static int _thcon_accept_conn(thcon* obj, int list_sock, int epoll_inst, struct epoll_event* event)
{
    struct sockaddr _in_addr;
    socklen_t _in_len;
    int _fd, _stat;
    char _hbuf[NI_MAXHOST], _sbuf[NI_MAXSERV];
    
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
			    fprintf(stderr, "Accept handling error\n");
			    break;
			}
		}

	    /* get information about the connection and log */
	    _stat = getnameinfo(&_in_addr, _in_len,
				_hbuf, NI_MAXHOST,
				_sbuf, NI_MAXSERV,
				NI_NUMERICHOST | NI_NUMERICSERV);

	    if(_stat == 0)
		fprintf(stdout, "Accepted connection from %s on port %s\n", _hbuf, _sbuf);

	    /* make the connection non blocking  and add to the epoll instance */
	    _thcon_make_socket_nonblocking(_fd);

	    event->data.fd = _fd;
	    event->events = EPOLLIN | EPOLLET;
	    epoll_ctl(list_sock, EPOLL_CTL_ADD, _fd, event);

	    /*
	     * Check if buffer needs expanding or not and set fd value.
	     */
	    _thcon_alloc_fds(obj);
	    obj->_var_cons_fds[obj->var_num_conns++] = fd;
	}

    return 0;
}

/* Write data on socket to the buffer */
static int _thcon_write_to_int_buff(thcon* obj, int socket_fd)
{
    static int _sz;

    _sz = 0;
    do
	{
	    _sz += read(socket_fd, &obj->var_membuff_in[_sz], THORNIFIX_MSG_BUFF_SZ);

	}while(_sz > 0);

    return _sz;
}

/* Read from the buffer and write to the socket */
static int _thcon_read_from_int_buff(thcon* obj, int socket_fd)
{
    int _max_length;
    static int _sz;

    _max_length = strlen(obj->var_membuff_out);

    _sz = 0;
    do
	{
	    _sz += write(socket_fd, obj->var_membuff_out[_sz], _max_length-_sz);

	}while(_sz < _max_length);

    return _sz;
}

/* Alocate memory */
inline __attribute__ (always_inline) static int _thcon_alloc_fds(thcon* obj)
{
    int _t_exs_sz;
    int* _t_buff;
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
	    pthread_mutex_lock(&obj->_var_mutex);
	    free(obj->_var_cons_fds);
	    obj->_var_cons_fds = _t_buff;
	    pthread_mutex_unlock(&obj->_var_mutex);
	    /*--------------------------------------------------*/	    
	}

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
    int i, a;
    /* check if connection count is 0, exit method */
    if(obj->var_num_conns < 1)
	return 0;

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
    return 0;
}


/*
 * The thread function peeks at the queue. If the messages exist in
 * the queue, its written to the socket.
 */
static void* _thcon_thread_function_write_server(void* obj)
{
    int i;
    thcon* _obj;
    struct _curl_mem* _msg;
    if(obj == NULL)
	return NULL;

    /* cast argument to correct object pointer */
    _obj = (thcon*) obj;

    while(1)
	{
	    /* wait on semaphore */
	    sem_wait(&obj->_var_sem);

	    /*
	     * If the message queue is empty we continue the loop
	     * and wait on semaphore until, posted from enqueu
	     * method.
	     */
	    if(gqueue_count(&obj->_msg_queue) == 0)
		continue;

	    /* Pop message from queue */
	    gqueue_out(&_obj->_msg_queue, (void**) &_msg);

	    /* write to all sockets */
	    /*--------------------------------------------------*/
	    /************* Mutex Lock This Section **************/
	    for(i = 0; i < _obj->var_num_conns; i++;)
		_thcon_send_info(_obj->_var_cons_fds[i], _msg->memory, _msg->size);
	    /*--------------------------------------------------*/

	    /* free memory */
	    free(_msg->memory);
	    free(_msg);
	}

    return NULL;
}
