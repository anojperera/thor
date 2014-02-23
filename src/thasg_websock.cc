/* Implementation of the websocket server wrap */
#include <cstring>
#include "thasg_websock.h"

/* Callback method for handling the connection */
static int _thasg_websock_callback(struct libwebsocket_context* context,
				   struct libwebsocket* wsi,
				   enum libwebsocket_callback_reason reason,
				   void* user,
				   void* in,
				   size_t len);

struct libwebsocket_protocols _protocols[] = {
    {
	"default",
	_thasg_websock_callback,
	sizeof(struct _thasg_msg_wrap),
	THORNIFIX_MSG_BUFF_SZ,
	NULL,
	0
    },
    {NULL, NULL, 0, 0, NULL, 0}
};


/* Constructor */
_thasg_websock::_thasg_websock(int port):_num_cons(0),_err_flg(0), _cont_flg(0)
{
    _websock_context = NULL;
    /* intialise the web socket information struct */
    memset(reinterpret_cast<void*>(&_websock_info), 0, sizeof(struct lws_context_creation_info));

    /* Set self pointer */
    _self_ptr = reinterpret_cast<void*>(this);

    /* initialise the lws info struct */
    _websock_info.port = port;
    _websock_info.iface = NULL;
    _websock_info.protocols = _protocols;
    _websock_info.extensions = libwebsocket_get_internal_extensions();
    _websock_info.ssl_cert_filepath = NULL;
    _websock_info.ssl_private_key_filepath = NULL;
    _websock_info.ssl_ca_filepath = NULL;
    _websock_info.ssl_cipher_list = NULL;
    _websock_info.gid = -1;
    _websock_info.uid = -1;
    _websock_info.options = 0;
    _websock_info.user = _self_ptr;
    _websock_info.ka_time = 0;
    _websock_info.ka_probes = 0;
    _websock_info.ka_interval = 0;

    /* Create websocket context */
    _websock_context = libwebsocket_create_context(&_websock_info);
    if(_websock_context == NULL)
	{
	    _err_flg = 1;
	    return;
	}
    
}

/* Destructor */
virtual _thasg_websock::~_thasg_websock()
{
    /* Check if errors occured */
    if(_err_flg)
	return;

    /* Set continue flag and service any outstanding messages */
    _cont_flg = 1;
    _thasg_websock::service_server();

    /* Destroy webcontext */
    libwebsocket_context_destroy(_websock_context);

    return;
}

/* Serivce web sockets */
int _thasg_websock::service_server(const char* msg, size_t sz)
{
    struct _thasg_msg_wrap _msg_wrap;
    /* Check for connections if no connections exist, return function */
    if(_num_cons == 0)
	return 0;

    /* Add message to the queue */
    memset(reinterpret_cast<void*>(&_msg_wrap), 0, sizeof(struct _thasg_msg_wrap));
    _msg_wrap._fd = 0;
    strncpy(_msg_wrap._msg, msg, (sz > (THORNIFIX_MSG_BUFF_SZ-1)? THORNIFIX_MSG_BUFF_SZ-1 : sz));
    _msg_wrap._msg[THORNIFIX_MSG_BUFF_SZ-1] = '\0';
    _msg_wrap._msg_sz = sz;

    _msg_queue.puch(_msg_wrap);

    /* Service all pending sockets */
    libwebsocket_service(_websock_context, 0);
    libwebsocket_callback_on_writable_all_protocol(&_protocols[0]);

    return 0;
}

/* Increment operator for number of connections */
_thasg_websock* _thasg_websock::operator ++()
{
    this->_num_cons++;
    return this;
}

/* Decrement number of connections */
_thasg_websock* _thasg_websock::operator --()
{
    if(this->_num_cons > 0)
	this->_num_cons--;

    return this;
}

/* Get the queue front */
struct _thasg_msg_wrap* _thasg_websock::get_queue_front()
{
    /* if the queue is empty return a null pointer */
    if(_msg_queue.empty())
	return NULL;

    return _msg_queue.front();
}

/* Remove the element from the queue */
int _thasg_websock::pop_queue()
{
    if(_msg_queue.empty())
	return 1;

    _msg_queue.pop();
    return 0;
}

/*======================================================================*/
/***************************** Private Methods **************************/
static int _thasg_websock_callback(struct libwebsocket_context* context,
				   struct libwebsocket* wsi,
				   enum libwebsocket_callback_reason reason,
				   void* user,
				   void* in,
				   size_t len)
{
    struct _thasg_msg_wrap* _msg_ptr;
    void* _t_ptr;
    char _t_buff[LWS_SEND_BUFFER_PRE_PADDING+THORNIFIX_MSG_BUFF_SZ+LWS_SEND_BUFFER_POST_PADDING];
    _thasg_websock* _websock_obj;

    
    if(context == NULL)
	return 0;

    /* Get object pointer */
    _t_ptr = libwebsocket_context_user(context);
    if(_t_ptr == NULL)
	return 0;

    _websock_obj = reinterpret_cast<_thasg_websock*>(_t_ptr);
    memset(reinterpret_cast<void*>(_t_buff), 0, sizeof(LWS_SEND_BUFFER_PRE_PADDING+THORNIFIX_MSG_BUFF_SZ+LWS_SEND_BUFFER_POST_PADDING));
    
    switch(reason)
	{
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
	    /* Increment counter */
	    _websock_obj++;
	    break;

	case LWS_CALLBACK_CLOSED:
	    _websock_obj--;
	    break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
	    /* Get message pointer */
	    _msg_ptr = _websock_obj->get_queue_front();
	    if(_msg_ptr == NULL)
		break;

	    /* Write to the websocket */
	    strncpy(_t_buff+LWS_SEND_BUFFER_PRE_PADDING, _msg_ptr->_msg, THORNIFIX_MSG_BUFF_SZ-1);
	    _t_buff[LWS_SEND_BUFFER_PRE_PADDING+THORNIFIX_MSG_BUFF_SZ-1] = '\0';

	    libwebsocket_write(wsi,
			       _t_buff+LWS_SEND_BUFFER_PRE_PADDING,
			       strlen(_t_buff+LWS_SEND_BUFFER_PRE_PADDING),
			       LWS_WRITE_TEXT);
	    
	    _websock_obj->pop_queue();
	    break;
	default:
	    break;

	}

    return 0;
}
