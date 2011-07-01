#!/bin/bash
echo "compiling"
gcc -g -Wall -O0 -c *.c
echo "complile complete"
mv *.o ../bin/
echo "moved object files to bin"
rm ../bin/thor.a
echo "remove thor"
ar -rc ../bin/thor.a ../bin/*.o
echo "removing files"
rm -v ../bin/*.o
