/* Main executable for all applications */

#include <stdlib.h>
#include "thapp.h"
#include "thapp_ahu.h"
#include "thapp_lkg.h"

int main(int argc, char** argv)
{
    thapp* _ahu;
    
    /*
     * Initialise ncurses system.
     */
    initscr();
    
    _ahu = thapp_ahu_new();

    thapp_start(_ahu);
    
    thapp_ahu_delete(THOR_AHU(_ahu));

    endwin();
    return 0;
}

