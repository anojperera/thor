#!/bin/bash
gcc -g -Wall -O0 -o test main.c thsys.c -I/usr/local/natinst/nidaqmxbase/include/ /usr/local/natinst/nidaqmxbase/lib/libnidaqmxbase.so.3.7.0 -lm -lpthread
gcc -g -Wall -O0 -o thsvr thsvre.c thsvr.c thsys.c thcon.c thornifix.c -I/usr/local/natinst/nidaqmxbase/include/ -I/usr/include/libxml2/ /usr/local/natinst/nidaqmxbase/lib/libnidaqmxbase.so.3.7.0 -lm -lalist -lxml2 -lconfig -lpthread
gcc -g -Wall -O0 -o thclient thtest.c thcon.c -I/usr/local/natinst/nidaqmxbase/include/ -I/usr/include/libxml2/ /usr/local/natinst/nidaqmxbase/lib/libnidaqmxbase.so.3.7.0 -lalist -lxml2 -lconfig -lm -lpthread

