#include <stdlib.h>
#include <stdio.h>

#include "thcon.h"

int main(int argc, char** argv)
{
    thcon _con;
    if(thcon_init(&_con, thcon_mode_server))
	return -1;

    thcon_set_geo_ip(&_con, "http://www.iplocation.net");
    thcon_get_my_geo(&_con);

    thcon_delete(&_con);
    return 0;
}
