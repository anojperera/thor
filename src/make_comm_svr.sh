#!/bin/bash

# Get directory paths
cwd=`pwd`
parent=$(dirname $cwd )
include_folder="/inc/"
include_path="$parent$include_folder"

# Creates a shared object of which expose a communication server
gcc -g -Wall -O0 -c -fPIC thcon.c thornifix.c -I$include_path -I/usr/include/libxml2/
mv *.o ../bin/
gcc -shared -Wl,-soname,libcomm.so.1 -o ../bin/libcomm.so.1.0.1 ../bin/*.o
rm ../bin/*.o
# Create static archive
gcc -g -Wall -O0 -c thcon.c thornifix.c -I$include_path -I/usr/include/libxml2/
mv *.o ../bin/
ar rcs ../bin/libcomm.a ../bin/*.o
rm ../bin/*.o
exit 0
