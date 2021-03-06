#!/bin/bash
#
# Test program
gcc -g -Wall -O0 -o ../bin/test -DTHOR_INC_NI main.c thsys.c \
	-I/usr/local/natinst/nidaqmxbase/include/ \
	/usr/local/natinst/nidaqmxbase/lib/libnidaqmxbase.so.3.7.0 -lm -lalist -lpthread

gcc -g -Wall -O0 -o thclient -DTHOR_INC_NI thtest.c thcon.c \
	-I/usr/local/natinst/nidaqmxbase/include/ -I/usr/include/libxml2/ \
	/usr/local/natinst/nidaqmxbase/lib/libnidaqmxbase.so.3.7.0 -lalist -lxml2 -lcurl -lconfig -lm -lalist -lpthread

# Server component
gcc -g -Wall -O0 -o thsvr -DTHOR_INC_NI thsvre.c thsvr.c thsys.c thcon.c thornifix.c \
	-I/usr/local/natinst/nidaqmxbase/include/ -I/usr/include/libxml2/ \
	/usr/local/natinst/nidaqmxbase/lib/libnidaqmxbase.so.3.7.0 -lm -lalist -lxml2 -lcurl -lconfig -lpthread

exit 0
