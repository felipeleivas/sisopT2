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

////////////typedefs
typedef struct t2fs_record RECORD;
typedef struct t2fs_inode INODE;
typedef struct t2fs_superbloco SUPERBLOCK;

////////////////// Global Variables

int initialized = FALSE;
SUPERBLOCK* superBlock;
BYTE buffer[SECTOR_SIZE] = {0};
int iNodeAreaOffset;
int blockAreaOffset;
////////////////////////////////////////
void initialize();
int validName(char *filename);
INODE* getInodeById(int id);
int writeInode(int id, INODE* inode);

WORD getWord(char leastSignificantByte, char mostSignificantByte){
    return ((WORD ) ((mostSignificantByte << 8) | leastSignificantByte));
}
DWORD getDoubleWord(char leastSignificantByteWord1, char mostSignificantByteWord1, char leastSignificantByteWord2, char mostSignificantByteWord2){
    return ((DWORD) ((mostSignificantByteWord2 << 24) | ((leastSignificantByteWord2 << 16) | ((mostSignificantByteWord1 << 8) | leastSignificantByteWord1))));
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







int main(){
    initialize();

    int block = 67;
    for(;block<73;block++){
        printf("------------------\n");
        RECORD* record = malloc(sizeof(RECORD*));
        int i;
        int j;
        for(j=0;j<3;j++){
            read_sector(blockToSector(block) + j + 0,buffer);
            for(i=0;i<3;i++){
                record->TypeVal = buffer[(i * 64) + 0];
                strcpy(record->name,(char *)buffer+1 + (64 * i));
                record->inodeNumber = getDoubleWord(buffer[(64 * i) + 60],buffer[(64 * i) + 61],buffer[(64 * i) + 62],buffer[(64 * i) + 63]);
                printf("Type %d\n",record->TypeVal );
                puts(record->name);
                printf("inode %d\n\n",record->inodeNumber);
            }
        }
    }

    INODE* inode = getInodeById(5);
    printf("sdsa\n");
    printf("%d\n",inode->dataPtr[0]);
    printf("%d\n",inode->dataPtr[1]);


    return 0;
}

int identify2 (char *name, int size){
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
    superBlock->diskSize = getDoubleWord(buffer[16],buffer[17],buffer[18],buffer[19]);


    iNodeAreaOffset =  blockToSector(superBlock->freeBlocksBitmapSize)
                       + blockToSector(superBlock->freeInodeBitmapSize)
                       + blockToSector(superBlock->superblockSize);
    blockAreaOffset = iNodeAreaOffset + blockToSector(superBlock ->inodeAreaSize);
    initialized = TRUE;

}

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

INODE* getInodeById(int id){
    int relativePossitionOnInodeBlock = id%4;
    int inodeSector = id/4 + iNodeAreaOffset;
    if(inodeSector >= blockToSector(blockAreaOffset)){
        printf("iNode out of bound");
    }
    if(read_sector(inodeSector,buffer) != SUCCESS_CODE){
        printf("Erro while reading the inode on disk");
        exit(-1);
    }
    INODE* inode = malloc(sizeof(INODE*));

    inode->blocksFileSize = getDoubleWord(buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +0],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +1],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +2],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +3]);
    inode->bytesFileSize = getDoubleWord(buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +4],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +5],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +6],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +7]);
    inode->dataPtr[0] = getDoubleWord(buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +8],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +9],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +10],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +11]);
    inode->dataPtr[1] = getDoubleWord(buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +12],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +13],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +14],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +15]);
    inode->singleIndPtr = getDoubleWord(buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +16],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +17],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +18],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +19]);
    inode->doubleIndPtr = getDoubleWord(buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +20],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +21],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +22],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +23]);
    inode->reservado[0] = getDoubleWord(buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +sizeof(INODE)],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +25],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +27],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +28]);
    inode->reservado[1] = getDoubleWord(buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +28],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +29],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +30],buffer[(sizeof(INODE) * relativePossitionOnInodeBlock) +31]);

    return inode;
}

int writeInode(int id, INODE* inode){
    int inodeSize = sizeof(INODE);
    int relativePossitionOnInodeBlock = id%4;
    int inodeSector = id/4 + iNodeAreaOffset;
    if(inodeSector >= blockToSector(blockAreaOffset)){
        printf("iNode out of bound");
    }
    read_sector(inodeSector, buffer);
    memcpy(buffer,dwordToBytes(inode->blocksFileSize), 4);
    memcpy(buffer + (inodeSize * relativePossitionOnInodeBlock) + 4,dwordToBytes(inode->bytesFileSize), 4);
    memcpy(buffer + (inodeSize * relativePossitionOnInodeBlock) + 8,dwordToBytes(inode->dataPtr[0]), 4);
    memcpy(buffer + (inodeSize * relativePossitionOnInodeBlock) + 12,dwordToBytes(inode->dataPtr[1]), 4);
    memcpy(buffer + (inodeSize * relativePossitionOnInodeBlock) + 16,dwordToBytes(inode->singleIndPtr), 4);
    memcpy(buffer + (inodeSize * relativePossitionOnInodeBlock) + 20,dwordToBytes(inode->doubleIndPtr), 4);
    memcpy(buffer + (inodeSize * relativePossitionOnInodeBlock) + 24,dwordToBytes(inode->reservado[0]), 4);
    memcpy(buffer + (inodeSize * relativePossitionOnInodeBlock) + 28,dwordToBytes(inode->reservado[1]), 4);

    return write_sector(inodeSector,buffer);
}

//int readRecord()
