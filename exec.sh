cd src/
gcc -c t2fs.c -Wall
gcc -o t2fs.exe t2fs.o ../lib/apidisk.o  ../lib/bitmap2.o -Wall
echo -e "\n\n\nOutput of main \n\n\n"
./t2fs.exe
rm t2fs.exe
rm t2fs.o
cd ..
