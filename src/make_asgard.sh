#!/bin/bash
#
g++ -g -Wall -O0 -o asgard thasgard.cc thasg_websock.cc thcon.c \
	-I/home/pyrus/Prog/C++/libwebsockets/lib/ -I/usr/include/libxml2/ -I/home/pyrus/Prog/C++/thor/inc/ \
	-lstdc++ -lpthread -lxml2 -lz -lm -lssl -lcrypto\
	-L/usr/lib/x86_64-linux-gnu/imlib2/loaders/ -lconfig -lcurl \
	/home/pyrus/Prog/C++/libwebsockets/lib/lib/libwebsockets.a -lalist
#
#
# Make daemon
gcc -g -Wall -O0 -o asgard_svr thasgarde.c
