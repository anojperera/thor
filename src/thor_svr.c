#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#define THOR_EXEC_PROGRAM 

int main(int argc, char** argv)
{
  pid_t _pid;

  /* Fork the process here */
  _pid = fork();

  /* Errors have occured */
  if(_pid < 0)
    exit(EXIT_FAILURE);

  /*
   * This is the parent process, therefore
   * we exit the parent.
   */
  if(_pid > 0)
    exit(EXIT_SUCCESS);

  /* Spawn the server program in an exec */
  execvp("thsvr", argv);
     
  return 0;
}
