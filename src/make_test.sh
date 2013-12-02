#!/bin/bash
gcc -g -Wall -O0 -o test main.c thsys.c -I/usr/local/natinst/nidaqmxbase/include/ /usr/local/natinst/nidaqmxbase/lib/libnidaqmxbase.so.3.7.0 -lm -lpthread
gcc -g -Wall -O0 -o thsvr thsvre.c thsys.c thcon.c -I/usr/local/natinst/nidaqmxbase/include/ /usr/local/natinst/nidaqmxbase/lib/libnidaqmxbase.so.3.7.0 -lm -lpthread

