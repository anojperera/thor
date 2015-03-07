#!/bin/bash

# Creates a shared object of which expose a communication server
gcc -g -Wall -O0 -c -fPIC thcon.c thornifix.c -I/home/pyrus/Prog/C++/thor/inc/ -I/usr/include/libxml2/
mv *.o ../bin/
gcc -shared -Wl,-soname,libcomm.so.1 -o ../bin/libcomm.so.1.0.1 ../bin/*.o
rm ../bin/*.o
exit 0
