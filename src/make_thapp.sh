#!/bin/bash
gcc -g -Wall -O0 -o thor thappe.c thapp_ahu.c thapp_lkg.c thapp.c thcon.c thsen.c thgsensor.c thvprb.c thvsen.c thsmsen.c thspd.c thornifix.c \
-I/usr/include/libxml2/ -lalist -lxml2 -lcurl -lconfig -lm -lalist -lmenu -lncurses -lpthread