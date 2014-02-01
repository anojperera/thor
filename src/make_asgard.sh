#!/bin/bash
g++ -g -Wall -O0 -o asgard thasgard.cc thcon.c -I/usr/include/libxml2 -lstdc++ -lpthread -lxml2 -lconfig -lcurl -lalist
