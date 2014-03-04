/*
 * Daemonise the asgard server.
 * This server listens on port 11003 (configurable by configuration file)
 * for any incomming connections and relays the messages to a
 * logging file and a websocket if connected.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char** argv)
{
    pid_t _process_id = 0;
    pid_t _set_id;
    /* Create the child process */
    _process_id = fork();

    /*
     * Check process id for errors and parent.
     */
    if(_process_id < 0)
	{
	    fprintf(stderr, "Daemonising failed\n");
	    exit(1);
	}

    if(_process_id > 0)
	exit(0);

    /* Unmask the file name */
    unmask(0);

    /* Set new session id */
    _set_id = setsid();
    if(_set_id < 0)
	exit(1);

    /* Change the current directory to temp */
    chdir("/tmp/");

    /* Close standard files */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    execvp("asgard");
    return 0;
}
