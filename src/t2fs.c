#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../include/apidisk.h"
#include "../include/bitmap2.h"
#include "../include/t2fs.h"

#define ERROR_CODE -1
#define SUCCESS_CODE 0

#define TRUE  1
#define FALSE 0
#define SUPER_BLOCK_AREA 0
#define FREE 0
#define OCCUPIED 1
#define INODE_BITMAP 0
#define DATA_BITMAP 1
#define INODE_SIZE sizeof(struct t2fs_inode)
#define RECORD_SIZE sizeof(struct t2fs_record)
#define INODES_PER_SECTOR (SECTOR_SIZE/INODE_SIZE)

#define MAX_OPEN_FILES_SIMULTANEOUSLY 10

////////////typedefs
typedef struct t2fs_record RECORD;
typedef struct t2fs_inode INODE;
typedef struct t2fs_superbloco SUPERBLOCK;

typedef struct {
    int active;
    int inodeId;
    int openBlockId;
    int currentPointer;
}OPEN_FILE;
////////////////// Global Variables
OPEN_FILE openFiles[MAX_OPEN_FILES_SIMULTANEOUSLY];
int initialized = FALSE;
SUPERBLOCK* superBlock;
BYTE buffer[SECTOR_SIZE] = {0};
int iNodeAreaOffset;
int blockAreaOffset;
int numberOfRecordsPerBlock;
int rootInode;
int blockBufferSize;
////////////////////////////////////////
void initialize();
int validName(char *filename);
int getInodeById(int id,INODE* inode);
int writeInode(int id, INODE* inode);
int getBlock(int id, BYTE* blockBuffer);
int getNextBlock(int lastBlockIndex, INODE* inode);
int getOpenFileStruct();
void getRecordOnBlockByPosition(BYTE *blockBuffer, int position, RECORD *record);
int getRecordNumber(int iNodeId,int indexOfRecord,RECORD *record);
int getRecordByName(int inodeNumber, char *filename,RECORD* record);
int writeBlock(int id, BYTE* blockBuffer);

WORD getWord(char leastSignificantByte, char mostSignificantByte){
    return ((WORD ) ((mostSignificantByte << 8) | leastSignificantByte));
}
DWORD getDoubleWord(BYTE *leastSignificantByteWord1){//, char mostSignificantByteWord1, char leastSignificantByteWord2, char mostSignificantByteWord2){
    return ((DWORD) ((leastSignificantByteWord1[3] << 24) | ((leastSignificantByteWord1[2] << 16) | ((leastSignificantByteWord1[1] << 8) | leastSignificantByteWord1[0]))));
}
BYTE* wordToBytes(WORD word) {
    BYTE *bytes = malloc(2*sizeof(*bytes));
    bytes[0] =  word & 0x00FF;
    bytes[1] = (word & 0xFF00) >> 8;
    return bytes;
}

BYTE* dwordToBytes(DWORD dword) {
    BYTE *bytes = malloc(4*sizeof(*bytes));
    bytes[0] =  dword & 0x00FF;
    bytes[1] = (dword & 0xFF00) >> 8;
    bytes[2] = (dword & 0xFF0000) >> 16;
    bytes[3] = (dword & 0xFF000000) >> 24;
    return bytes;
}

unsigned int blockToSector(unsigned int block){
    return block * superBlock->blockSize;
}






//
int main(){
    FILE2 f = open2("file3");
    char buffer[256]={0};
    read2(f,buffer,1);
    int i;
    for(i = 0 ; i<256; i++){
        printf("%c",buffer[i]);
    }
    printf("\n");
    read2(f,buffer,14);
    for(i = 0 ; i<256; i++){
        printf("%c",buffer[i]);
    }
    printf("\n");
    printf("foi, agr vou escrever\n");

    char buffer2[256]={0};
    strcpy(buffer2,"testFelipe");

    write2(f,buffer2,10);
    read2(f,buffer,14);
    for(i = 0 ; i<256; i++){
        printf("%c",buffer[i]);
    }
    puts("");
    FILE2 f2 = open2("file3");
    read2(f2,buffer,256);
    for(i = 0 ; i<256; i++){
        printf("%c",buffer[i]);
    }
    puts("\n");
//    for(i = 0; i < 100; i++){
//        printf("%d ",getBitmap2(DATA_BITMAP,i));
//    }

//    FILE* pf= fopen("teste","w+r");
//    fwrite("teste\0",1,5,pf);
//    fwrite("teste\0",1,5,pf);
//    fseek(pf,-7,SEEK_CUR);
//    fwrite("---\0",1,3,pf);
//    fclose(pf);
//    initialize();

    return 0;
}

void initialize(){
    superBlock = malloc(sizeof(*superBlock));
    if(!superBlock){
        printf("Malloc error\n");
        return;
    }

    if(read_sector(SUPER_BLOCK_AREA, buffer) != 0){
        printf("Read error\n");
        return;
    }

    memcpy(superBlock->id,buffer,4);
    superBlock->version = getWord(buffer[4],buffer[5]);
    superBlock->superblockSize = getWord(buffer[6],buffer[7]);
    superBlock->freeBlocksBitmapSize = getWord(buffer[8],buffer[9]);
    superBlock->freeInodeBitmapSize = getWord(buffer[10],buffer[11]);
    superBlock->inodeAreaSize = getWord(buffer[12],buffer[13]);
    superBlock->blockSize = getWord(buffer[14],buffer[15]);
    superBlock->diskSize = getDoubleWord(buffer+16);
    iNodeAreaOffset =  blockToSector(superBlock->freeBlocksBitmapSize)
                       + blockToSector(superBlock->freeInodeBitmapSize)
                       + blockToSector(superBlock->superblockSize);
    blockAreaOffset = blockToSector(iNodeAreaOffset) + blockToSector(superBlock ->inodeAreaSize);
    read_sector(blockAreaOffset,buffer);
    rootInode = getDoubleWord(buffer+60);
    blockBufferSize = SECTOR_SIZE * superBlock->blockSize;
    numberOfRecordsPerBlock = SECTOR_SIZE * superBlock->blockSize / RECORD_SIZE;
    int i;
    for(i=0;i<MAX_OPEN_FILES_SIMULTANEOUSLY;i++){
        openFiles[i].active=FALSE;
        openFiles[i].currentPointer=-1;
        openFiles[i].inodeId=-1;
        openFiles[i].openBlockId=-1;
    }

    initialized = TRUE;

}



int validName(char *filename){
    int i;
    for(i = 0; i <strlen(filename); i++){
        if( !((('0' <= filename[i]) && (filename[i] <= '9'))  ||
              (('a' <= filename[i]) && (filename[i] <= 'z'))  ||
              (('A' <= filename[i]) && (filename[i] <= 'Z'))  )){
            return FALSE;
        }
    }
    return TRUE;
}

int getInodeById(int id,INODE* inode){
    int relativePossitionOnInodeBlock = id%INODES_PER_SECTOR;
    int inodeSector = id/INODES_PER_SECTOR + iNodeAreaOffset;
    if(inodeSector >= blockToSector(blockAreaOffset)){
        printf("iNode out of bound");
        return ERROR_CODE;
    }
    if(read_sector(inodeSector,buffer) != SUCCESS_CODE){
        printf("Erro while reading the inode on disk");
        return ERROR_CODE;
    }

    inode->blocksFileSize = getDoubleWord(buffer+(INODE_SIZE * relativePossitionOnInodeBlock) +0);
    inode->bytesFileSize = getDoubleWord(buffer +(INODE_SIZE * relativePossitionOnInodeBlock) +4);
    inode->dataPtr[0] = getDoubleWord(buffer+(INODE_SIZE * relativePossitionOnInodeBlock) +8);
    inode->dataPtr[1] = getDoubleWord(buffer+(INODE_SIZE * relativePossitionOnInodeBlock) +12);
    inode->singleIndPtr = getDoubleWord(buffer+(INODE_SIZE * relativePossitionOnInodeBlock) +16);
    inode->doubleIndPtr = getDoubleWord(buffer+(INODE_SIZE * relativePossitionOnInodeBlock) +20);
    inode->reservado[0] = getDoubleWord(buffer+(INODE_SIZE * relativePossitionOnInodeBlock) +24);
    inode->reservado[1] = getDoubleWord(buffer+(INODE_SIZE * relativePossitionOnInodeBlock) +28);
    return SUCCESS_CODE;
}

int writeInode(int id, INODE* inode){
    int relativePossitionOnInodeBlock = id%INODES_PER_SECTOR;
    int inodeSector = id/INODES_PER_SECTOR + iNodeAreaOffset;
    if(inodeSector >= blockToSector(blockAreaOffset)){
        printf("iNode out of bound");
    }
    read_sector(inodeSector, buffer);
    memcpy(buffer,dwordToBytes(inode->blocksFileSize), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 4,dwordToBytes(inode->bytesFileSize), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 8,dwordToBytes(inode->dataPtr[0]), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 12,dwordToBytes(inode->dataPtr[1]), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 16,dwordToBytes(inode->singleIndPtr), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 20,dwordToBytes(inode->doubleIndPtr), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 24,dwordToBytes(inode->reservado[0]), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 28,dwordToBytes(inode->reservado[1]), 4);

    return write_sector(inodeSector,buffer);
}

int getBlock(int id, BYTE* blockBuffer){
    int i=0;
    for(i = 0; i < superBlock->blockSize; i++){
        if(read_sector(blockToSector(id) + i,buffer) != 0){
            return ERROR_CODE;
        }
        memcpy(blockBuffer + (i * SECTOR_SIZE),buffer,SECTOR_SIZE);
    }
    return 0;
}

int writeBlock(int id, BYTE* blockBuffer){
    int i=0;
    for(i = 0; i < superBlock->blockSize; i++){
        memcpy(buffer,blockBuffer + (i * SECTOR_SIZE),SECTOR_SIZE);
        write_sector(blockToSector(id) + i,buffer);
    }
    return 0;
}

int getRecordByName(int inodeNumber, char *filename,RECORD* record){
    int index = 0;
    while(getRecordNumber(inodeNumber,index,record) == SUCCESS_CODE){
//        printf("name: %s\n",record->name);
        if(strcmp(filename,record->name) == 0){
            return SUCCESS_CODE;
        }
        index++;
    }
    return ERROR_CODE;
}

int getRecordNumber(int iNodeId,int indexOfRecord,RECORD *record){

    if(indexOfRecord < numberOfRecordsPerBlock){
        INODE* inode = malloc(INODE_SIZE);
        if(getInodeById(iNodeId,inode) != SUCCESS_CODE){
            printf("Error at getting inode on getNextRecord");
            return ERROR_CODE;
        }
        BYTE blockBuffer[blockBufferSize];
        getBlock(inode->dataPtr[0],blockBuffer);
        getRecordOnBlockByPosition(blockBuffer,indexOfRecord%numberOfRecordsPerBlock,record);

        free(inode);

        return SUCCESS_CODE;
    }
    return ERROR_CODE;

}

void getRecordOnBlockByPosition(BYTE *blockBuffer, int position, RECORD *record){
    int positionOffset = position * RECORD_SIZE;
    record->TypeVal= blockBuffer[0+positionOffset];
    memcpy(record->name,blockBuffer + positionOffset + 1,59);
    record->inodeNumber = getDoubleWord(blockBuffer+positionOffset + 60);

}

int getOpenFileStruct(){
    int i;
    for(i = 0; i < MAX_OPEN_FILES_SIMULTANEOUSLY; i++){
        if(openFiles[i].active == FALSE){
            return i;
        }
    }
    return ERROR_CODE;
}

int getNextBlock(int lastBlockIndex, INODE* inode){
    if(lastBlockIndex == 0){
//        if(inode->dataPtr[1] == INVALID_PTR){
////            printf("Error trying to access dataPointer[1]");
//            return ERROR_CODE;
//        }
        return inode->dataPtr[1];
    }

    if(lastBlockIndex + 1 < (blockBufferSize/sizeof(DWORD)) + 2){
        lastBlockIndex -= 2;
        if(inode->singleIndPtr == INVALID_PTR){
//            printf("Error trying to access singleInderectPointer");
            return INVALID_PTR;
        }
        BYTE bufferBlock[blockBufferSize];
        getBlock(inode->singleIndPtr,bufferBlock);
        return getDoubleWord(bufferBlock + (sizeof(DWORD) * (lastBlockIndex + 1)));
    }
    else{
        lastBlockIndex -= (blockBufferSize/sizeof(DWORD)) + 2;
        int nextBlockIndex = 1 + lastBlockIndex;
        if(inode->doubleIndPtr == INVALID_PTR){
//            printf("Error trying to access doubleInderectPointer");
            return INVALID_PTR;
        }
        BYTE bufferBlock[blockBufferSize];
        getBlock(inode->doubleIndPtr,bufferBlock);
        int currentIndirectBlock = getDoubleWord(bufferBlock + ((nextBlockIndex/(blockBufferSize/(sizeof(DWORD)))) * sizeof(DWORD)));
        int currentIndirectBlockOffset = nextBlockIndex %  (blockBufferSize/(sizeof(DWORD)));
        getBlock(currentIndirectBlock,bufferBlock);
        return getDoubleWord(bufferBlock + (currentIndirectBlockOffset + sizeof(DWORD)));
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int create2(char *filename){
    if(initialized == FALSE){
        initialize();
    }

    if(validName(filename) != TRUE){
        printf("Error with filename");
        return ERROR_CODE;
    }
    INODE* inode = malloc(sizeof(INODE*));
    inode->blocksFileSize = 0;
    inode->bytesFileSize = 34;
    inode->dataPtr[0] = INVALID_PTR;
    inode->singleIndPtr = INVALID_PTR;
    inode->doubleIndPtr = INVALID_PTR;

    int inodeId = searchBitmap2(DATA_BITMAP,FREE);
    if(writeInode(inodeId,inode) != SUCCESS_CODE){
        printf("error writing inode");
        return ERROR_CODE;
    }

    setBitmap2(INODE_BITMAP,inodeId,OCCUPIED);

    RECORD* record = malloc(sizeof(RECORD*));
    strcpy(record->name,filename);
    record->TypeVal = TYPEVAL_REGULAR;
    record->inodeNumber = inodeId;

    return inodeId;
}

int identify2 (char *name, int size){
    return 0;
}

int delete2 (char *filename){
    return 0;
}


FILE2 open2 (char *filename){
    if(initialized == FALSE){
        initialize();
    }
    RECORD *record = malloc(RECORD_SIZE);
    getRecordByName(rootInode,filename,record);
    int openFileIndex = getOpenFileStruct();
    if(record->TypeVal != TYPEVAL_REGULAR){
        printf("Can't open these kind of file\n");
    }
    if(openFileIndex == ERROR_CODE){
        printf("Maximum number of open files reached\n");
        return ERROR_CODE;
    }

    openFiles[openFileIndex].active = TRUE;
    openFiles[openFileIndex].inodeId = record->inodeNumber;
    openFiles[openFileIndex].openBlockId = -1;
    openFiles[openFileIndex].currentPointer = 0;

    free(record);
    return openFileIndex;
}

int close2 (FILE2 handle){
    return 0;
}


int read2 (FILE2 handle, char *buffer, int size){
    if(initialized == FALSE){
        initialize();
    }
    int currentPointerOffset = openFiles[handle].currentPointer;
    int currentBlockOffset = currentPointerOffset/blockBufferSize;
    int blockCurrentPointerOffset = (currentPointerOffset % blockBufferSize);

    INODE* inode = malloc(INODE_SIZE);
    getInodeById(openFiles[handle].inodeId,inode);

    if(inode->bytesFileSize < currentPointerOffset + size){
        size = inode->bytesFileSize - currentPointerOffset;
    }


    if(openFiles[handle].openBlockId == -1){
        if(inode->dataPtr[0] == INVALID_PTR){
            printf("Cant read this part of file");
            return ERROR_CODE;
        }
        openFiles[handle].openBlockId = inode->dataPtr[0];
    }


    BYTE blockBuffer[blockBufferSize];
    int bufferOffset = 0;
    while(currentPointerOffset + size > (currentBlockOffset + 1) * blockBufferSize){

        int partSize = (currentBlockOffset + 1) * blockBufferSize - currentBlockOffset - 1;

        getBlock(openFiles[handle].openBlockId,blockBuffer);

        memcpy(buffer + bufferOffset,blockBuffer + blockCurrentPointerOffset,partSize);

        bufferOffset += partSize;
        currentPointerOffset += partSize;
        size -= partSize;
        blockCurrentPointerOffset = 0;

        openFiles[handle].openBlockId = getNextBlock(currentBlockOffset, inode);
        if(openFiles[handle].openBlockId == INVALID_PTR){
            printf("Error reading nextBlock");
            return  -1;
        }
        currentBlockOffset++;
    }
    getBlock(openFiles[handle].openBlockId,blockBuffer);
    memcpy(buffer + bufferOffset,blockBuffer + blockCurrentPointerOffset,size);



    openFiles[handle].currentPointer += size;
    free(inode);
    return 0;
}

int write2 (FILE2 handle, char *buffer, int size){
    if(initialized == FALSE){
        initialize();
    }
    int currentPointerOffset = openFiles[handle].currentPointer;
    int currentBlockOffset = currentPointerOffset/blockBufferSize;
    int blockCurrentPointerOffset = (currentPointerOffset%blockBufferSize);

    INODE* inode = malloc(INODE_SIZE);
    getInodeById(openFiles[handle].inodeId,inode);

    BYTE blockBuffer[blockBufferSize];
    getBlock(openFiles[handle].openBlockId,blockBuffer);
    memcpy(blockBuffer + blockCurrentPointerOffset,buffer,size);
    writeBlock(openFiles[handle].openBlockId,blockBuffer);

    currentPointerOffset += size;
    if(currentPointerOffset > inode->bytesFileSize){
        inode->bytesFileSize = currentPointerOffset;
    }

    int bufferOffset = 0;
    while(currentPointerOffset + size > (currentBlockOffset + 1) * blockBufferSize){

        int partSize = (currentBlockOffset + 1) * blockBufferSize - currentBlockOffset - 1;

        getBlock(openFiles[handle].openBlockId,blockBuffer);

        memcpy(blockBuffer + blockCurrentPointerOffset,buffer + bufferOffset,partSize);

        bufferOffset += partSize;
        currentPointerOffset += partSize;
        blockCurrentPointerOffset = 0;
        size -= partSize;

        openFiles[handle].openBlockId = getNextBlock(currentBlockOffset, inode);
        if(openFiles[handle].openBlockId == INVALID_PTR){
            int freeBlockId = searchBitmap2(BITMAP_DADOS,FREE);
            if(freeBlockId < 0 ){
                printf("Cannot find free space");
                return ERROR_CODE;
            }


        }
        currentBlockOffset++;
    }

    writeInode(openFiles[handle].inodeId, inode);

    return 0;
}


int truncate2 (FILE2 handle){
    return 0;
}


int seek2 (FILE2 handle, DWORD offset){
    return 0;
}


int mkdir2 (char *pathname){
    return 0;
}


int rmdir2 (char *pathname){
    return 0;
}


int chdir2 (char *pathname){
    return 0;
}


int getcwd2 (char *pathname, int size){
    return 0;
}


DIR2 opendir2 (char *pathname){
    return 0;
}
int readdir2 (DIR2 handle, DIRENT2 *dentry){
    return 0;
}

int closedir2 (DIR2 handle){
    return 0;
}

//int readRecord()
