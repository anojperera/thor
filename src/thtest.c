/* Test program for checking the messages recieved from the server */
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "thornifix.h"
#include "thcon.h"

volatile sig_atomic_t _flg = 1;
static thcon _con;
static int _recv_callback(void* obj, void* msg, size_t sz);
static void _sig_handler(int signo);
static int _write_to_svr(void);

int main(int argc, char** argv)
{
    /* Make connection */
    if(thcon_init(&_con, thcon_mode_client))
	exit(1);

    thcon_set_server_name(&_con, "192.168.8.100");
    thcon_set_port_name(&_con, "11000");

    thcon_set_recv_callback(&_con, _recv_callback);
    signal(SIGINT, _sig_handler);
    thcon_start(&_con);

    /* seed for once */
    srand(time(NULL));
    
    while(_flg)
	{
	    _write_to_svr();
	    sleep(1);
	}

    return 0;
}

static void _sig_handler(int signo)
{
    if(signo == SIGINT)
	{
	    thcon_stop(&_con);
	    thcon_delete(&_con);
	    _flg = 0;
	}
    return;
}

static int _recv_callback(void* obj, void* msg, size_t sz)
{
    char _msg[THORINIFIX_MSG_SZ];

    if(msg == NULL || sz <= 0)
	return 0;
    
    /* Print messages */
    memset(_msg, 0, THORINIFIX_MSG_SZ);
    strncpy(_msg, msg, THORINIFIX_MSG_SZ-1);
    _msg[THORINIFIX_MSG_SZ-1] = '\0';

    fprintf(stdout, "%s\r", _msg);
    fflush(stdout);
    return 0;
}

/* Write to server */
static int _write_to_svr(void)
{
    struct thor_msg _msg;						/* message */
    char _msg_buff[THORNIFIX_MSG_BUFF_SZ];

    /* Initialise struct */
    thorinifix_init_msg(&_msg);

    _msg._ao0_val = rand()%10;
    _msg._ao1_val = rand()%10;

    /* Encode message buffer */
    thornifix_encode_msg(&_msg, _msg_buff, THORNIFIX_MSG_BUFF_SZ);

    /* Send to the server */
    thcon_send_info(&_con, (void*) &_msg_buff, THORNIFIX_MSG_BUFF_SZ);
    
    return 0;
}
