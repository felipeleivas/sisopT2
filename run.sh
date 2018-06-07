#!/usr/bin/env bash
if [ "$1" == "-r" ]
then
    echo reloading the disk
    cp t2fs_disk.dat src/t2fs_disk.dat
fi
cd src/
gcc -c t2fs.c -Wall > /dev/null
gcc -o t2fs.exe t2fs.o ../lib/apidisk.o  ../lib/bitmap2.o -Wall > /dev/null
#gcc -o t2fs.exe ../exemplo/main.c t2fs.o ../lib/apidisk.o  ../lib/bitmap2.o -Wall
echo -e "\nOutput of #main \n\n\n"
./t2fs.exe
rm t2fs.exe
rm t2fs.o
cd ..