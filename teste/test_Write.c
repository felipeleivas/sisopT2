#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../include/apidisk.h"
#include "../include/bitmap2.h"
#include "../include/t2fs.h"

#define TRUE 1
#define FALSE 0
#define FREE 0
#define OCCUPIED 1
#define INODE_BITMAP 0
#define DATA_BITMAP 1
//

int countFreeBlocks();

int main() {
    int bufferSize = 1024 * 1024;
    char auxBuffer2[1024 * 1024] = {0};

    int testSuccess = TRUE;
    int i;
    printf("-----------------------\n");
    // printf("Number of free nodes: %d before the program starts\n",countFreeBlocks());
    // printf("There is %d free blocks\n",numberOfFreeBlocks);
    FILE2 f = open2("file3");
    int bytesRead = read2(f, auxBuffer2, bufferSize);
    int numberOfBytsToWrite = (1024 * (258 + (256 * 2)) * 1) + 1;
    for (i = 0; i < bytesRead; i++) {
        if (auxBuffer2[i] == 0) {
            printf("ERROR WHILE READING THE FILE: %s \n", "file3");
            testSuccess = FALSE;
        }
    }
    // printf("Number of free nodes: %d after read a file\n",countFreeBlocks());
    int GOING_TO_WRITE = TRUE;
    int maxBufferSize = 1024 * 1024 *2;
    int totalWritingBytes = 0;
    char auxBuffer[maxBufferSize];
    for (i = 0; i < maxBufferSize; i++)
        auxBuffer[i] = 49;
    if (GOING_TO_WRITE == TRUE) {
        int writes = 1024;
        printf("\nbloco:%d\n", numberOfBytsToWrite / writes);
        for (i = 0; i < maxBufferSize/1024 ; i++) {
            int writenBytes = write2(f, auxBuffer, writes);
            totalWritingBytes += writes;
            if (writenBytes != writes) {
                testSuccess = FALSE;
                printf("ERROR writing FILE was writen %d bytes instead 1024 \n", writenBytes);
            }
        }
    	// printf("Number of free nodes: %d after read a file\n",countFreeBlocks());
        char auxBuffer3[maxBufferSize];
        FILE2 f2 = open2("file3");

        int bytesReadAfterWrite = read2(f2, auxBuffer3, maxBufferSize);
        for (i = bytesRead; i < bytesReadAfterWrite; i++) {
            if (auxBuffer3[i] != 49) {
                testSuccess = FALSE;
                printf("======== Readed the wrong character it was %d instead of 49 [%d] =========\n", auxBuffer3[i], i);
            }
        }

        if (bytesReadAfterWrite != totalWritingBytes) {
            testSuccess = FALSE;
            printf("ERROR read FILE was Readed %d bytes instead of %d \n", bytesReadAfterWrite, totalWritingBytes);

        }

        auxBuffer[0] = 'Y';
        auxBuffer[1] = 'Z';
        int writenBytes = write2(f, auxBuffer, 2);
        if (writenBytes != 2) {
            testSuccess = FALSE;
            printf("ERROR writing FILE was writen %d bytes instead of 2 \n", writenBytes);
        }
    }

    if (seek2(f, -2) == 0) {
        int bytesRead = read2(f, auxBuffer2, bufferSize);
        if (auxBuffer2[0] != 'Y') {
            testSuccess = FALSE;
            printf("Readed the wrong character it was %c instead of Y \n", auxBuffer2[0]);
        }
        if (auxBuffer2[1] != 'Z') {
            testSuccess = FALSE;
            printf("Readed the wrong character it was %c instead of Z \n", auxBuffer2[1]);
        }

    }
    else{
    	testSuccess = FALSE;
	 	printf("Coundn't seek the file");
    }
    if(seek2(f,- (maxBufferSize + 2 + bytesRead)) == 0){
    	truncate2(f);
    		FILE2 f3=open2("file3");
    		BYTE auxBuffer4[maxBufferSize];
    		int numBytesReaded =read2(f3,auxBuffer4,maxBufferSize);
    		if(numBytesReaded != 0){
				testSuccess = FALSE;
				printf("Read bytes from an file that should be empty\n");
    		}
    }

    if (testSuccess == TRUE)
        printf("CONGRATZ THE TEST PASSED\n");
    return 0;
}


int countFreeBlocks() {
    int i = 0;
    int count = 0;
    for (i = 0; i < 1024 * 8; i++) {
        if (getBitmap2(DATA_BITMAP, i) == FREE) {
            count++;
        }

    }
    return count;
}	