/*
 * This is C++ implementation of a server component for
 * handling of logging messages.
 */
#include <stdlib.h>
#include <cstring>
#include <pthread.h>
#include <unistd.h>
#include <libconfig.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <map>
#include <queue>

#include "thornifix.h"
#include "thcon.h"
#include "thasg_websock.h"

#define THASG_DEFAULT_CONFIG_PATH1 "thor.cfg"
#define THASG_DEFAULT_CONFIG_PATH2 "../config/thor.cfg"
#define THASG_DEFAULT_CONFIG_PATH3 "/etc/thor.cfg"


/*
 * Below are tokens for keys required to read configuration
 * file.
 */
#define THASG_GEO_URL "mygeo_location"
#define THASG_ADMIN1_URL "prime_admin_url"
#define THASG_ADMIN2_URL "http://www.valyria.co.uk:/8080/dieties/"
#define THASG_COM_PORT "sec_con_port"
#define THASG_DEF_COM_PORT "11001"
#define THASG_DEF_TIMEOUT "def_time_out"
#define THASG_WEBSOCK_PORT "websock_port"
#define THASG_QUEUE_LEN_KEY "app_queue_limit"
#define THASG_DEF_WAIT_TIME 100000

#define THASG_FILE_NAME_BUFF_SZ 256
#define THASG_DEFAULT_LOG_FILE_NAME "%Y-%m-%d-%I-%M-%S.txt"

volatile sig_atomic_t _flg = 1;


class _thasg
{
private:
    int err_flg;
    int f_flg;						/* Flag to indicate complete all write actions */
    int queue_length;					/* Queue length */
    std::queue<struct _thasg_msg_wrap> _msg_queue;	/* Message queue */

    /*
     * A map is used to store the socket descriptor and its corresponding
     * file descriptor.
     * First argument shall be the socket descriptor and the second
     * argument shall be the file descriptor.
     */
    std::map<int, int> _fds;

    thcon var_con;
    config_t var_config;
    pthread_mutex_t var_mutex;
    void* _var_self;

    int create_file_name(char* f_name, size_t sz);

    _thasg_websock* var_websock;			 /* Websocket server */

public:
    _thasg();
    virtual ~_thasg();

    int add_msg(void* msg_ptr, size_t sz);
    int write_file(void);
    int start(void);
    int stop(void);

    /* Get a reference to the internal file descriptor collection */
    std::map<int, int>* get_fds();

	int create_new_file(int socket);
};


/* Callback methods for the connection object */
static int _thasgard_con_recv_msg(void* self, void* msg, size_t sz);
static int _thasgard_con_made(void* self, void* con);
static int _thasgard_con_closed(void* self, void* con, int sock);
static void _thasgard_sigterm_handler(int signo);

int main(int argc, char** argv)
{
    _thasg asg;


    /* Add signal handlers */
    /* Attach a signal handler */
    signal(SIGKILL, _thasgard_sigterm_handler);
    signal(SIGINT, _thasgard_sigterm_handler);

    asg.start();
    /* Initialise pthread object */
    while(_flg)
	{
	    usleep(THASG_DEF_WAIT_TIME);

	    /*
	     * Call write method which shall write messages
	     * from queue to the respective files
	     */
	    asg.write_file();
	}

    /*
     * This shall stop the connection object and any unwritten messages
     * are written to file before closing.
     */
    asg.stop();
    return 0;
}


/****************************************************************************/
/*----------------------- Implementation of the class ----------------------*/

/* Class constructor */
_thasg::_thasg():err_flg(0), f_flg(0), queue_length(0)
{
    int stat = 0;
    struct config_setting_t* _setting = NULL;
    const char* _t_buff = NULL;


    /* Initialise configuration settings */
    config_init(&var_config);

    /* Set this pointer as the self */
    _var_self = reinterpret_cast<void*>(this);


    var_websock = NULL;
    /* Check the default paths for the configuration file and find the settings */
    while(1)
	{
	    /*
	     * If any of the following attempts were successful, we set the status
	     * and exit the loop.
	     */

	    if(config_read_file(&var_config, THASG_DEFAULT_CONFIG_PATH1) == CONFIG_TRUE)
			goto check_path_success;

	    if(config_read_file(&var_config, THASG_DEFAULT_CONFIG_PATH2) == CONFIG_TRUE)
			goto check_path_success;

	    if(config_read_file(&var_config, THASG_DEFAULT_CONFIG_PATH3) == CONFIG_TRUE)
			goto check_path_success;

	    /*
	     * If this point was reached, that means no settings were found. Therefore
	     * we exit the loop and inidicate the function as failed
	     */
	    break;

	check_path_success:
	    stat = 1;
	    break;
	}

    /* Set the error flag and return the constructor */
    /*
     * Connection object shall not be initialised in if this
     * condition was true.
     */
    if(stat == 0)
	{
	    err_flg = 1;
	    return;
	}

    /*
     * Create connection object in the server mode if not
     * set the error and return.
     */
    if(thcon_init(&var_con, thcon_mode_server))
	{
	    err_flg = 1;
	    return;
	}
    thcon_set_ext_obj(&var_con, _var_self);
    thcon_set_recv_callback(&var_con, _thasgard_con_recv_msg);
    thcon_set_closed_callback(&var_con, _thasgard_con_closed);
    thcon_set_conmade_callback(&var_con, _thasgard_con_made);

    /* Initialise mutex */
    pthread_mutex_init(&var_mutex, NULL);

    /* Get default port name */
    _setting = config_lookup(&var_config, THASG_COM_PORT);
    if(_setting)
	{
	    _t_buff = config_setting_get_string(_setting);
	    if(_t_buff)
			thcon_set_port_name(&var_con, _t_buff);
	}
    else
		thcon_set_port_name(&var_con, THASG_DEF_COM_PORT);

    /* Get port number for the websocket server */
    _setting = config_lookup(&var_config, THASG_WEBSOCK_PORT);
    if(_setting)
	{
	    _t_buff = config_setting_get_string(_setting);
	    if(_t_buff)
		{
		    /* Create websocket server */
		    var_websock = new _thasg_websock(atoi(_t_buff));
		}
	}

    /* Set queue length */
    _setting = config_lookup(&var_config, THASG_QUEUE_LEN_KEY);
    if(_setting)
	{
	    _t_buff = config_setting_get_string(_setting);
	    if(_t_buff)
		{
		    queue_length = atoi(_t_buff);
		}
	}

    /* Reset connection struct info */
    thcon_reset_my_info(&var_con);
    return;
}


/* Destructor */
_thasg::~_thasg()
{
    std::map<int,int>::iterator _m_itr;

    _var_self = NULL;

    /* Close all open file pointers */
    for(_m_itr = _fds.begin(); _m_itr != _fds.end(); ++_m_itr)
	{
	    /* Close open files */
	    if(_m_itr->second > 0)
			close(_m_itr->second);
	}

    /* Empty container */
    _fds.erase(_fds.begin(), _fds.end());


    /* If the websocket server was created destroy it */
    if(var_websock != NULL)
		delete var_websock;

    /* Destroy the configuration object */
    config_destroy(&var_config);

    /* Destroy mutex */
    pthread_mutex_destroy(&var_mutex);

    /* Destroy connection */
    thcon_delete(&var_con);

}

/* Add message to the queue */
int _thasg::add_msg(void* msg_ptr, size_t sz)
{
    struct _thasg_msg_wrap _msg_obj;

    /* Check for arguments */
    if(msg_ptr == NULL || sz <= 0)
		return 0;

    /* Initialise message object */
    memset((void*) &_msg_obj, 0, sizeof(struct _thasg_msg_wrap));

    /* Get active socket descriptor */
    _msg_obj._fd = THCON_GET_ACTIVE_SOCK(&var_con);

    /* Copy message to the internal buffer */
    memcpy((void*) _msg_obj._msg, msg_ptr, THORNIFIX_MSG_BUFF_SZ);
    _msg_obj._msg[THORNIFIX_MSG_BUFF_SZ-1] = '\0';

    _msg_obj._msg_sz = THORNIFIX_MSG_BUFF_SZ;

    /* Insert to queue */
    pthread_mutex_lock(&var_mutex);
    _msg_queue.push(_msg_obj);
    pthread_mutex_unlock(&var_mutex);
    return 0;
}

/* Method pops the message from the queue and writes to the relevant file */
int _thasg::write_file(void)
{
    int _sock_des = 0;
    int _file_des = 0;
    char _file_name[THASG_FILE_NAME_BUFF_SZ];

    struct _thasg_msg_wrap* _t_msg = NULL;
    std::map<int, int>::iterator _m_itr;

    while(!_msg_queue.empty())
	{

	    pthread_mutex_lock(&var_mutex);
	    _t_msg = &_msg_queue.front();
	    pthread_mutex_unlock(&var_mutex);
	    if(!_t_msg)
			break;

	    /* Set socket descriptor */
	    _sock_des = _t_msg->_fd;

	    /* Check if the FDs collection is empty */
	    if(_fds.empty())
		{
			_file_des = create_new_file(_sock_des);
		    if(_file_des == -1)
				goto exit_loop;

		}
	    else
		{
		    /*
		     * The list is not empty, therefore we search for the file descriptor
		     * associated with the socket.
		     */
		    _m_itr = _fds.find(_sock_des);

		    /* Check the iterator, if the map returned end, we create a file */
		    if(_m_itr == _fds.end())
			{
				_file_des = create_new_file(_sock_des);
			    if(_file_des == -1)
					goto exit_loop;

			}
		    else
				_file_des = _m_itr->second;

		}



	    /* Write to file */
	    /* Append return character */
	    strcat(_t_msg->_msg, "\n");
	    write(_file_des, _t_msg->_msg, strlen(_t_msg->_msg));

	    /* If the web socket server was created, call to service sockets */
	    if(var_websock)
			var_websock->service_server(_t_msg->_msg, _t_msg->_msg_sz);

	exit_loop:
	    pthread_mutex_lock(&var_mutex);
	    _msg_queue.pop();
	    pthread_mutex_unlock(&var_mutex);

	    /*
	     * If the f_flg is not set that means after an single iteration we exit the loop.
	     * When this flag is set to true, all messages shall in the queue are written to
	     * their respective file descriptors.
	     */
	    if(!f_flg)
			break;
	}

    return 0;
}

/* Start the server */
int _thasg::start(void)
{
    /* Start the server */
    return thcon_start(&var_con);
}

/* Stop the server and write all messages in the queue */
int _thasg::stop(void)
{
    /* Stop the server */
    thcon_stop(&var_con);

    /*
     * Set write flag to indicate all remaining messages are to be
     * written.
     */
    f_flg = 1;
    _thasg::write_file();
    return 0;
}

/* Get a reference to the file descriptor collection */
std::map<int, int>* _thasg::get_fds()
{
    return &_fds;
}


/* Create temporary file name */
int _thasg::create_file_name(char* f_name, size_t sz)
{
    time_t _tm;
    struct tm* _tm_info;

    time(&_tm);
    _tm_info = localtime(&_tm);
    strftime(f_name, sz, THASG_DEFAULT_LOG_FILE_NAME, _tm_info);
    return 0;
}

int _thasg::create_new_file(int socket)
{
    char _file_name[THASG_FILE_NAME_BUFF_SZ];
    int _file_des = 0;

	memset(_file_name, 0, THASG_FILE_NAME_BUFF_SZ);

	/* Create file */
	_thasg::create_file_name(_file_name, THASG_FILE_NAME_BUFF_SZ);

	/* Open file and store file and socket descriptors */
	_file_des = open(_file_name, O_CREAT | O_APPEND | O_RDWR);
	if(_file_des == -1)
		return _file_des;

	/* adjust file permissions */
	fchmod(_file_des, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	/* add the new descriptor to the collection */
    _fds.insert(std::pair<int, int>(socket, _file_des));

	return _file_des;
}

/*=================================== Callback methods from the server ===================================*/
/*--------------------------------------------------------------------------------------------------------*/
static int _thasgard_con_recv_msg(void* self, void* msg, size_t sz)
{
    _thasg* _obj;

    if(self == NULL || msg == NULL || sz <= 0)
		return 0;

    /* Cast self pointer to the correct type */
    _obj = reinterpret_cast<_thasg*>(self);

    _obj->add_msg(msg, sz);
    return 0;
}

/* Callback is fired when a connection is made */
static int _thasgard_con_made(void* self, void* con)
{
	thcon* _con = NULL;
	_thasg* _asg = NULL;
	std::map<int, int>* _fds_col = NULL;
	int _file_des = -1;

	if(self == NULL || con == NULL)
		return -1;

	_con = reinterpret_cast<thcon*>(con);
	_asg = reinterpret_cast<_thasg*>(self);

	/*
	 * Check to see if the file and socket descriptor map is empty
	 * or the socket doesn't have a corresponding descriptor in collection.
	 */
	_fds_col = _asg->get_fds();
	if(_fds_col->empty() || _fds_col->find(THCON_GET_ACTIVE_SOCK(_con)) == _fds_col->end())
		_file_des =_asg->create_new_file(THCON_GET_ACTIVE_SOCK(_con));

	fprintf(stdout, "connection made on socket %i - assigned file %i\n", THCON_GET_ACTIVE_SOCK(_con), _file_des);

    return 0;
}

/* Callback fired when a connection is closed */
static int _thasgard_con_closed(void* self, void* con, int sock)
{
    _thasg* _asg;
    std::map<int,int>* _fds_col;
    std::map<int,int>::iterator _it;

    _fds_col = NULL;

    /* Check for self pointer */
    if(self == NULL)
		return 0;

    _asg = reinterpret_cast<_thasg*>(self);
    _fds_col = _asg->get_fds();

	fprintf(stdout, "socket %i, closed\n", sock);
    if(!_fds_col)
	{
	    _it = _fds_col->find(sock);
	    if(_it != _fds_col->end())
		{
		    close(_it->first);

		    /* Close the socket */
		    _fds_col->erase(_it);


		}

	}
    return 0;
}

/* Signal handler */
static void _thasgard_sigterm_handler(int signo)
{
    if(signo == SIGINT || signo == SIGKILL)
		_flg = 0;

    return;
}
