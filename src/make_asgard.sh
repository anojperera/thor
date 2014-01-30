#!/bin/bash
g++ -g -Wall -O0 -o asgard thasgard.cc thcon.c -I/usr/include/libxml2 -lpthread -lxml2 -lconfig -lcurl -lalist
