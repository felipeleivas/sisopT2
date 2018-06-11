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
#define ERROR_CODE -1
#define ERROR_CODE_FILE_NOT_FOUND -2
#define ERROR_CODE_FILE_WRONG_PATH -3
#define SUCCESS_CODE 0

//

int countFreeBlocks();

int main() {

    int bufferSize = 1024 * 1024;
    char auxBuffer2[1024 * 1024] = {0};

    int testSuccess = TRUE;
    int i;
    /* printf("-----------------------\n");
    int numOfFreeBlocksStart= (countFreeBlocks());
    // printf("There is %d free blocks\n",countFreeBlocks());
    FILE2 f = open2("file3");
    FILE2 f2;
    FILE2 f3;
    int bytesRead = read2(f, auxBuffer2, bufferSize);
    int numberOfBytsToWrite = (1024 * (258 + (256 * 2)) * 1) + 1;
    for (i = 0; i < bytesRead; i++) {
        if (auxBuffer2[i] == 0) {
            // printf("ERROR WHILE READING THE FILE: %s \n", "file3");
            testSuccess = FALSE;
        }
    }
    int numOfFreeBlocksAfterRead= (countFreeBlocks());
    if(numOfFreeBlocksStart > numOfFreeBlocksAfterRead ){
    	printf("Shouldn't alloc any block\n");
    	testSuccess = FALSE;
    }
    int GOING_TO_WRITE = TRUE;
    int maxBufferSize = 1024 * 1024 *2;
    int totalWritingBytes = 0;
    char auxBuffer[maxBufferSize];
    for (i = 0; i < maxBufferSize; i++)
        auxBuffer[i] = 49;
    if (GOING_TO_WRITE == TRUE) {
        int writes = 1024;
        for (i = 0; i < maxBufferSize/1024 ; i++) {
            int writenBytes = write2(f, auxBuffer, writes);
            totalWritingBytes += writes;
            if (writenBytes != writes) {
                testSuccess = FALSE;
                printf("ERROR writing FILE was writen %d bytes instead 1024 \n", writenBytes);
            }
        }
        int numOfFreeBlocksAfterWrite= (countFreeBlocks());
    	if(numOfFreeBlocksAfterWrite > numOfFreeBlocksAfterRead ){
	    	printf("Should alloc some blocks\n");
	    	testSuccess = FALSE;
    	}
        char auxBuffer3[maxBufferSize];
        f2 = open2("/dir1/../file3");

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
    		f3 = open2("/file3");
    		BYTE auxBuffer4[maxBufferSize];
    		int numBytesReaded =read2(f3,auxBuffer4,maxBufferSize);
    		if(numBytesReaded != 0){
				testSuccess = FALSE;
				printf("Read bytes from an file that should be empty\n");
    		}
    }
    	int numOfFreeBlocksAfterTruncate= (countFreeBlocks());
    	if(numOfFreeBlocksAfterTruncate < numOfFreeBlocksAfterRead ){
	    	printf("Should free the blocks\n");
	    	testSuccess = FALSE;
    	}
    if(open2("/pineapple") != ERROR_CODE_FILE_NOT_FOUND){
    	testSuccess = FALSE;
    	printf("SHOULD RETURN %d but return %d\n",ERROR_CODE_FILE_NOT_FOUND,open2("/pineapple"));
    }
    if(open2("/asassasa/pineapple") != ERROR_CODE_FILE_WRONG_PATH){
    	testSuccess = FALSE;
    	printf("SHOULD RETURN %d but return %d\n",ERROR_CODE_FILE_WRONG_PATH,open2("/pineapple"));
    }
    if(close2(f) != SUCCESS_CODE){
    	testSuccess = FALSE;
    	printf("Couldn't close the file f\n");
    }
    if(close2(f2) != SUCCESS_CODE){
    	testSuccess = FALSE;
    	printf("Couldn't close the file f2\n");
    }
    if(close2(f3) != SUCCESS_CODE){
    	testSuccess = FALSE;
    	printf("Couldn't close the file f3\n");
    }
    if(close2(-1) == SUCCESS_CODE){
    	testSuccess = FALSE;
    	printf("Should't close the file cause its not a file\n");
    }

     printAllStuff();
     FILE2 createdFile = create2("/dir1/teste");
     BYTE batata[10]= {'a','y','r','v','h','u','p','4','-','a'};
     int writenBytesOnTheCreatedFile = write2(createdFile,batata,10);
     if(writenBytesOnTheCreatedFile != 10){
     	testSuccess = FALSE;
     	printf("Didn't wrote the 10 bytes on the new file\n");
     }
     if(create2("dir1/teste") > 0){
     	testSuccess = FALSE;
     	printf("This file already exist in these path\n");
     }
     if(open2("teste") >= 0){
     	testSuccess = FALSE;
     	printf("This file doens't exist in these path\n");
     }
     seek2(createdFile,-10);


     BYTE batata2[10];
     int readedBytesOnTheCreatedFile = read2(createdFile,batata2,10);

     if(readedBytesOnTheCreatedFile != 10){
     	testSuccess = FALSE;
     	printf("Didn't read the 10 bytes on the new file\n");
     }

     if(close2(createdFile) != SUCCESS_CODE){
		testSuccess = FALSE;
     	printf("Should close the createdFile\n");
     }

     if(delete2("/dir1/teste") != SUCCESS_CODE){
		testSuccess = FALSE;
     	printf("Should delete the createdFile\n");
     }

     printAllStuff();

     if(delete2("/file3") != SUCCESS_CODE){
		testSuccess = FALSE;
     	printf("Should delete the file3\n");
     }
     if(delete2("/dir1/../file3") != SUCCESS_CODE){
		testSuccess = FALSE;
     	printf("Should delete the file3\n");
     }

     printAllStuff();

	 if(delete2("/2dir1/teste") == SUCCESS_CODE){
		testSuccess = FALSE;
     	printf("Shouldn't be able to delete this file\n");
     }

     printAllStuff();

     if(open2("/dir1/teste") == SUCCESS_CODE){
     	testSuccess = FALSE;
     	printf("Shouldn't be able to open this file\n");
     }
 	printf("########CREATE NEW directory##########################\n");

     if(mkdir2("newDir") != SUCCESS_CODE){
     	testSuccess = FALSE;
     	printf("Should be able to create the directory\n");
     }

     printAllStuff();
     	printf("########CREATE FILE ON NEW directory##########################\n");
     	FILE2 newDirFile = create2("newDir/file1");
     if(newDirFile < 0){
		testSuccess = FALSE;
     	printf("Should be able to create the file at newDir %d\n",newDirFile);
     }

     printAllStuff();

    if (testSuccess == TRUE)
        printf("CONGRATZ THE TEST PASSED\n");
    else
        printf("CONGRATZ THE YOU BREAK THE CODE\n"); */


        printf("\n__________ PRINTANDO A SITUACAO ATUAL DO SISTEMA _________\n");
        printAllStuff();

        printf("\n\n________ CRIANDO A FILE teste no diretorio dir1, caminho absoluto __________");
        create2("/dir1/teste");
        printAllStuff();

        printf("\n\n________ CRIANDO A FILE teste no diretorio dir1, NAO DEVE CRIAR __________");
        create2("/dir1/teste");
        printAllStuff();

        printf("\n\n________ CRIANDO O DIRETORIO dirnovo no diretorio dir1, caminho absoluto ___________");
        mkdir2("/dir1/dirnovo");
        printAllStuff();

        printf("\n\n________ CRIANDO O DIRETORIO dirnovo no diretorio dir1, caminho absoluto, NAO DEVE CRIAR ___________");
        mkdir2("/dir1/dirnovo");
        printAllStuff();

        printf("\n\n________ CRIANDO O DIRETORIO dirnovodois no diretorio raiz, caminho absoluto ___________");
        mkdir2("/dirnovodois");
        printAllStuff();

        printf("\n\n________ CRIANDO A FILE filepradeletar no diretorio dirnovo, caminho absoluto ___________");
        create2("/dir1/dirnovo/filepradeletar");
        printAllStuff();

        printf("\n\n________ TENTANDO DELETAR O DIRETORIO dirnovo, NAO DEVE DELETAR! ___________");
        rmdir2("/dir1/dirnovo");
        printAllStuff();

        printf("\n\n________ TENTANDO DELETAR A FILE filepradeletar, DEVE DELETAR! ___________");
        delete2("/dir1/dirnovo/filepradeletar");
        printAllStuff();

        printf("\n\n________ TENTANDO DELETAR O DIRETORIO dirnovo, DEVE DELETAR! ___________");
        rmdir2("/dir1/dirnovo");
        printAllStuff();

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
