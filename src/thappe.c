/* Main executable for all applications */

#include <stdlib.h>
#include <menu.h>
#include <unistd.h>
#include "thapp.h"
#include "thapp_ahu.h"
#include "thapp_lkg.h"

#define THAPP_MENU_ITEMS 2
#define THAPP_QUIT_CODE1 81
#define THAPP_QUIT_CODE2 113
#define THAPP_NEW_LINE_CODE 10
#define THAPP_RETURN_CODE 13

/* Test methods */
static int _thor_test_ahu(void);
static int _thor_test_lkg(void);

int main(int argc, char** argv)
{

    char* _menu_opts[] = {
	"AHU Testing",
	"Leak Testing"
    };
    
    ITEM* _menu_items[THAPP_MENU_ITEMS+1];
    MENU* _menu;
    ITEM* _cur_item;
    int i, c;
    unsigned int _exit_flg = 0;
    int (*_item_fptr)(void);
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

    /* Set user pointers to call function pointer for the relevant test */
    set_item_userptr(_menu_items[0], (void*) _thor_test_ahu);
    set_item_userptr(_menu_items[1], (void*) _thor_test_lkg);

    /* Create new menu */
    _menu = new_menu((ITEM**) _menu_items);

    while(1)
	{
	    mvprintw(LINES - 2, 0, "Select test from above, press 'q' to Exit");
	    post_menu(_menu);
	    refresh();

	    while((c = getch()) != THAPP_QUIT_CODE2)
		{
		    switch(c)
			{
			case KEY_DOWN:
			    menu_driver(_menu, REQ_DOWN_ITEM);
			    break;
			case KEY_UP:
			    menu_driver(_menu, REQ_UP_ITEM);
			    break;
			case THAPP_NEW_LINE_CODE:
			case THAPP_RETURN_CODE:
			    /* Get current item */
			    _cur_item = current_item(_menu);
			    _exit_flg = 1;
			    break;
			}

		    /* If Pressed return exit loop */
		    if(_exit_flg)
			break;
	    
		    usleep(100000);
		}

	    /* If application was quit in the main screen it will exit it */
	    if(c == THAPP_QUIT_CODE2)
		break;

	    /* Get user pointer */
	    _item_fptr = (int(*)(void)) item_userptr(_cur_item);
        
	    /* Unpost menu and clear the screen */
	    c = item_count(_menu);
	    unpost_menu(_menu);
	    clear();

	    /* Call the function */
	    _item_fptr();
	    clear();
	}
    
    clear();
    free_menu(_menu);
    for(i=0; i<c; i++)
	{
	    free_item(_menu_items[i]);
	    _menu_items[i] = NULL;
	}
    
    _menu_items[i] = NULL;
    _menu = NULL;
    
    endwin();
    return 0;
}

/* AHU Test */
static int _thor_test_ahu(void)
{
    thapp* _ahu;
    _ahu = thapp_ahu_new();

    thapp_start(_ahu);
    
    thapp_ahu_delete(THOR_AHU(_ahu));
    return 0;
}

/* Leakage test */
static int _thor_test_lkg(void)
{
    thapp* _ahu;
    _ahu = thapp_lkg_new();

    thapp_start(_ahu);
    
    thapp_lkg_delete(THOR_LKG(_ahu));
    return 0;
}
