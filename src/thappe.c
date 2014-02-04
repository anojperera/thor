/* Main executable for all applications */

#include <stdlib.h>
#include <menu.h>
#include <unistd.h>
#include "thapp.h"
#include "thapp_ahu.h"
#include "thapp_lkg.h"

#define THAPP_MENU_ITEMS 2

int main(int argc, char** argv)
{
    /* thapp* _ahu; */
    char* _menu_opts[] = {
	"AHU Testing",
	"Leak Testing"
    };
    
    ITEM* _menu_items[THAPP_MENU_ITEMS+1];
    MENU* _menu;

    int i, c;
    
    /*
     * Initialise ncurses system.
     */
    initscr();

    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    for(i=0; i<THAPP_MENU_ITEMS; i++)
	_menu_items[i] = new_item(_menu_opts[i], _menu_opts[i]);
    _menu_items[i] = NULL;

    /* Create new menu */
    _menu = new_menu((ITEM**) _menu_items);

    mvprintw(LINES - 2, 0, "F1 to Exit");
    post_menu(_menu);
    refresh();

    while((c = getch()) != KEY_F(1))
	{
	    switch(c)
		{
		case KEY_DOWN:
		    menu_driver(_menu, REQ_DOWN_ITEM);
		    break;
		case KEY_UP:
		    menu_driver(_menu, REQ_UP_ITEM);
		    break;
		case 10:
		    
		}

	    usleep(100000);
	}

    free_item(_menu_items[0]);
    free_item(_menu_items[1]);
    free_menu(_menu);
    
    /* _ahu = thapp_ahu_new(); */

    /* thapp_start(_ahu); */
    
    /* thapp_ahu_delete(THOR_AHU(_ahu)); */

    endwin();
    return 0;
}

