#!/bin/bash
echo "compiling"
gcc -g -Wall -O0 -c -m32 *.c
echo "complile complete"
mv *.o ../bin/
echo "moved object files to bin"
rm ../bin/thor.a
echo "remove thor"
ar -rc ../bin/thor.a ../bin/*.o
echo "removing files"
rm -v ../bin/*.o
echo "compile thorexec"
gcc -g -Wall -O0 -o ../bin/thorexec thorexec.c thlkg.c thahup.c thsov.c thactr.c thgsens.c thvelsen.c thpid.c thlinreg.c thbuff.c c:/MinGW/lib/alist.a c:/MinGW/lib/NIDAQmx.lib
