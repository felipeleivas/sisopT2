#!/usr/bin/env bash
if [ "$1" == "-r" ]
then
    echo reloading the disk
    cp t2fs_disk.dat teste/t2fs_disk.dat
fi
cd src/
gcc -c t2fs.c -Wall
cd ..
cd teste/
#gcc -c test_Write.c -Wall
#gcc -o test_Write test_Write.o ../src/t2fs.o ../lib/apidisk.o  ../lib/bitmap2.o -Wall
gcc -c test_create_delete_mkdir_rmdir.c -Wall
gcc -o test_create_delete_mkdir_rmdir.o ../src/t2fs.o ../lib/apidisk.o  ../lib/bitmap2.o -Wall
#gcc -o t2fs.exe ../exemplo/main.c t2fs.o ../lib/apidisk.o  ../lib/bitmap2.o -Wall
echo -e "\nOutput of #main \n"
./test_create_delete_mkdir_rmdir
rm test_create_delete_mkdir_rmdir
rm test_create_delete_mkdir_rmdir.o
rm ../src/t2fs.o
cd ..
