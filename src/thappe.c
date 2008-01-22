/* Main executable for all applications */

#include <stdlib.h>
#include "thapp.h"
#include "thapp_ahu.h"

int main(int argc, char** argv)
{
    thapp* _ahu;

    _ahu = thapp_ahu_new();

    thapp_start(_ahu);
    
    thapp_ahu_delete(THOR_AHU(_ahu));
    return 0;
}

