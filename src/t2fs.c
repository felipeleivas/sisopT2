#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../include/apidisk.h"
#include "../include/bitmap2.h"
#include "../include/t2fs.h"

#define ERROR_CODE -1
#define ERROR_CODE_FILE_NOT_FOUND -2
#define ERROR_CODE_FILE_WRONG_PATH -3
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
#define INODES_PER_SECTOR (SECTOR_SIZE / INODE_SIZE)

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
} OPEN_FILE;

////////////////// Global Variables
OPEN_FILE openFiles[MAX_OPEN_FILES_SIMULTANEOUSLY];
int initialized = FALSE;
SUPERBLOCK superBlock;
// BYTE buffer[SECTOR_SIZE] = {0};
int iNodeAreaOffset;
int blockAreaOffset;
int numberOfRecordsPerBlock;
int rootInodeId;

INODE rootInode;
RECORD rootRecord;
RECORD actualDirectory; // Começa estando na root

int blockBufferSize;

////////////////////////////////////////


typedef struct nameNode {
    char name[59];
    struct nameNode *next;
} nameNode;

void initialize();

int validName(char *filename);

int getInodeById(int id, INODE *inode);

int writeInode(int id, INODE *inode);

int getBlock(int id, BYTE *blockBuffer);

int getNextBlockId(int lastBlockIndex, INODE *inode);

int getOpenFileStruct();

void getRecordOnBlockByPosition(BYTE *blockBuffer, int position, RECORD *record);

void setRecordOnBlockByPosition(BYTE *blockBuffer, int position, RECORD *record);

int getRecordByIndex(int iNodeId, int indexOfRecord, RECORD *record);

int setRecordAtIndex(int iNodeId, int indexOfRecord, RECORD *record);

int getRecordByName( char *filename, RECORD *record);

int writeBlock(int id, BYTE *blockBuffer);

int assignBlockToInode(int blockIndex, int freeBlockId, INODE *inode);

int desallocBlocksOfInode(int from, int to, INODE *inode);

int desallocBlock(int blockId);

void printAllStuff() {
  printf("\n __________________ PRINT STUFF ___________________\n");
  printf("Diretorio atual: %s com o inode de ID %d", actualDirectory.name, actualDirectory.inodeNumber);
  printf("Diretorio root: %s com o inode de ID %d", rootRecord.name, rootRecord.inodeNumber);
  printf("\n_____________________ ARVORE _________________________\n");
  printStuff(&rootRecord);
}

void printStuff(RECORD *record) {
  printf("-![%s]!-", record.name);
  INODE inode;
  RECORD recordAux;
  inode = getInodeById(record.inodeNumber, inode);
  if(record.typeVal == TYPEVAL_DIRETORIO) {
    int numberOfRecords = inode.bytesFileSize / RECORD_SIZE;
    printf(" - Number of Records inside this file: %d -", numberOfRecords);
    int i = 0;
    for(i = 2; i < numberOfRecords; i++) {
      getRecordByIndex(record.inodeNumber, i, recordAux);
      if(recordAux.typeVal == TYPEVAL_DIRETORIO) {
        printStuff(&recordAux);
      } else {
        printf("-[%s]-", recordAux.name);
      }
    }
  }
}

int variavelQueEUvouDELEtar = 0;

WORD getWord(char leastSignificantByte, char mostSignificantByte) {
    return ((WORD) ((mostSignificantByte << 8) | leastSignificantByte));
}

DWORD getDoubleWord(
        BYTE *leastSignificantByteWord1) {//, char mostSignificantByteWord1, char leastSignificantByteWord2, char mostSignificantByteWord2){
    return ((DWORD) ((leastSignificantByteWord1[3] << 24) | ((leastSignificantByteWord1[2] << 16) |
                                                             ((leastSignificantByteWord1[1] << 8) |
                                                              leastSignificantByteWord1[0]))));
}

BYTE *wordToBytes(WORD word, BYTE *bytes) {
    // BYTE *bytes = malloc(2*sizeof(*bytes));
    // BYTE bytes[2];
    bytes[0] = word & 0x00FF;
    bytes[1] = (word & 0xFF00) >> 8;
    return bytes;
}

void dwordToBytes(DWORD dword, BYTE *bytes) {
    // BYTE *bytes = malloc(4*sizeof(*bytes));
    // static BYTE bytes[4 * sizeof(BYTE)];
    bytes[0] = dword & 0x00FF;
    bytes[1] = (dword & 0xFF00) >> 8;
    bytes[2] = (dword & 0xFF0000) >> 16;
    bytes[3] = (dword & 0xFF000000) >> 24;
}


unsigned int blockToSector(unsigned int block) {
    return block * superBlock.blockSize;
}

void initialize() {

    BYTE buffer[SECTOR_SIZE];
    if (read_sector(SUPER_BLOCK_AREA, buffer) != 0) {
        printf("Read error\n");
        return;
    }

    memcpy(superBlock.id, buffer, 4);
    superBlock.version = getWord(buffer[4], buffer[5]);
    superBlock.superblockSize = getWord(buffer[6], buffer[7]);
    superBlock.freeBlocksBitmapSize = getWord(buffer[8], buffer[9]);
    superBlock.freeInodeBitmapSize = getWord(buffer[10], buffer[11]);
    superBlock.inodeAreaSize = getWord(buffer[12], buffer[13]);
    superBlock.blockSize = getWord(buffer[14], buffer[15]);
    superBlock.diskSize = getDoubleWord(buffer + 16);
    iNodeAreaOffset = blockToSector(superBlock.freeBlocksBitmapSize)
                      + blockToSector(superBlock.freeInodeBitmapSize)
                      + blockToSector(superBlock.superblockSize);
    blockAreaOffset = blockToSector(iNodeAreaOffset) + blockToSector(superBlock.inodeAreaSize);
    read_sector(blockAreaOffset, buffer);
    rootInodeId = getDoubleWord(buffer + 60);
    blockBufferSize = SECTOR_SIZE * superBlock.blockSize;
    numberOfRecordsPerBlock = SECTOR_SIZE * superBlock.blockSize / RECORD_SIZE;

    // Inicializar o i-node root
    // rootInode = malloc(sizeof(INODE));
    rootInode.blocksFileSize = 1;
    rootInode.bytesFileSize = 128;
    rootInode.dataPtr[0] = 0;
    rootInode.dataPtr[1] = INVALID_PTR;
    rootInode.singleIndPtr = INVALID_PTR;
    rootInode.doubleIndPtr = INVALID_PTR;

    // Inicializar o record root
    // rootRecord = malloc(sizeof(RECORD));
    rootRecord.TypeVal = TYPEVAL_DIRETORIO;
    strcpy(rootRecord.name, "/");
    rootRecord.inodeNumber = rootInodeId;

    actualDirectory = rootRecord;

    int i;
    for (i = 0; i < MAX_OPEN_FILES_SIMULTANEOUSLY; i++) {
        openFiles[i].active = FALSE;
        openFiles[i].currentPointer = -1;
        openFiles[i].inodeId = -1;
        openFiles[i].openBlockId = -1;
    }


    initialized = TRUE;
}

int validName(char *filename) {
    char charFileNameCopy[1000];
    strcpy(charFileNameCopy,filename);
    char *token;

    token = strtok (charFileNameCopy,"/");
    while(token != NULL){
        int i;
        for (i = 0; i < strlen(token); i++) {
            if (!((('0' <= token[i]) && (token[i] <= '9')) ||
                  (('a' <= token[i]) && (token[i] <= 'z')) ||
                  (('A' <= token[i]) && (token[i] <= 'Z')))) {
                return FALSE;
            }
        }
        token = strtok (NULL, "/");
    }
    return TRUE;
}

int getFileName(char *fullFilename, char *filename, char *path){
    char charFileNameCopy[1000];
    char charFileNameCopy2[1000];
    strcpy(charFileNameCopy,fullFilename);
    char *token;
    char *tokenAux = NULL;
    int i = 0;
    if(fullFilename[0] == '/'){
        i++;
        charFileNameCopy2[0] ='/';
    }
    token = strtok (charFileNameCopy,"/");

    while(token != NULL){
        if(tokenAux != NULL){
            // printf("--%s\n",tokenAux);
            strcpy(charFileNameCopy2 + i,tokenAux);
            i += strlen(tokenAux);
            memcpy(charFileNameCopy2 + i,"/",1);
            i += 1;
        }
        tokenAux = token;

        token = strtok (NULL, "/");
    }
    memcpy(charFileNameCopy2 + i,"\0",1);

    strcpy(filename ,tokenAux);
    strcpy(path ,charFileNameCopy2);
    return SUCCESS_CODE;
}

int isAbsolutePath(char *filename) {
    if (filename[0] == '/') {
        return TRUE;
    } else {
        return FALSE;
    }
}

int getRecordByName(char *filename, RECORD *record) {
    int index = 0;
    int inodeNumber;
    char fileNameCpy[1000];
    strcpy(fileNameCpy,filename);

    if(strlen(fileNameCpy) == 0 || (strlen(fileNameCpy) == 1 && fileNameCpy[0] == '/'))
        strcpy(fileNameCpy,"./");

    if(fileNameCpy[0] == '/')
        inodeNumber = rootInodeId;
    else
        inodeNumber = actualDirectory.inodeNumber;


    char charFileNameCopy[1000];
    strcpy(charFileNameCopy,fileNameCpy);
    char *token;

    token = strtok (charFileNameCopy,"/");
    int foundRecordWithName = FALSE;
    int foundRecordWithToken = TRUE;
    while (token != NULL && foundRecordWithToken == TRUE){
        index = 0;
        foundRecordWithToken = FALSE;
        foundRecordWithName  = FALSE;
        while (foundRecordWithName == FALSE && getRecordByIndex(inodeNumber, index, record) == SUCCESS_CODE){
            foundRecordWithName = FALSE;
            printf("-->%s",record->name);
            if (strcmp(token, record->name) == 0 && record->TypeVal != TYPEVAL_INVALIDO) {
                inodeNumber = record->inodeNumber;
                foundRecordWithName = TRUE;
                foundRecordWithToken = TRUE;
                printf("Achou o token %s\n", token );
            }
            printf("  -->%d\n",foundRecordWithName);
            index++;
        }
        token = strtok (NULL, "/");

    }
    if(token == NULL && foundRecordWithName == TRUE){
            // puts(record->name);
    puts("Acabou SUCCESS_CODE");
        return SUCCESS_CODE;
    }

    // printf("[ERROR] Error while getting record by name");
    if(token == NULL && foundRecordWithName == FALSE){
    printf("Acabou foundRecordWithName %d\n",foundRecordWithName);
        return ERROR_CODE_FILE_NOT_FOUND;
    }

    if(token != NULL){
        printf("------------%s-----------\n",filename);
        return ERROR_CODE_FILE_WRONG_PATH;
    }

    return ERROR_CODE;
}

int getRecordByIndex(int iNodeId, int indexOfRecord, RECORD *record) {
    INODE inode;

    if (getInodeById(iNodeId, &inode) != SUCCESS_CODE) {
        printf("[ERROR] Error at getting inode on getRecordByIndex\n");
        return ERROR_CODE;
    }


    if (indexOfRecord < inode.blocksFileSize * numberOfRecordsPerBlock) {

        BYTE blockBuffer[blockBufferSize];
        int recordBelongingBlockIndex = (indexOfRecord - 1)/numberOfRecordsPerBlock;
        int recordBelongingBlockId = getNextBlockId(recordBelongingBlockIndex-1 , &inode);

        getBlock(recordBelongingBlockId, blockBuffer);
        getRecordOnBlockByPosition(blockBuffer, indexOfRecord % numberOfRecordsPerBlock, record);

        return SUCCESS_CODE;
    }
    return ERROR_CODE;
}

int setRecordAtIndex(int iNodeId, int indexOfRecord, RECORD *record) {
    INODE inode;

    if (getInodeById(iNodeId, &inode) != SUCCESS_CODE) {
        printf("[ERROR] Error at getting inode on setRecordAtIndex\n");
        return ERROR_CODE;
    }
    if (indexOfRecord < inode.blocksFileSize * numberOfRecordsPerBlock) {


        BYTE blockBuffer[blockBufferSize];
        int recordBelongingBlockIndex = (indexOfRecord - 1)/numberOfRecordsPerBlock;
        int recordBelongingBlockId = getNextBlockId(recordBelongingBlockIndex-1 , &inode);

        getBlock(recordBelongingBlockId, blockBuffer);

        setRecordOnBlockByPosition(blockBuffer, indexOfRecord % numberOfRecordsPerBlock, record);
        writeBlock(recordBelongingBlockId,blockBuffer);

        return SUCCESS_CODE;
    }
    return ERROR_CODE;
}

void getRecordOnBlockByPosition(BYTE *blockBuffer, int position, RECORD *record) {
    int positionOffset = position * RECORD_SIZE;

    record->TypeVal = blockBuffer[0 + positionOffset];
    memcpy(record->name, blockBuffer + positionOffset + 1, 59);
    record->inodeNumber = getDoubleWord(blockBuffer + positionOffset + 60);
}

void setRecordOnBlockByPosition(BYTE *blockBuffer, int position, RECORD *record) {
    int positionOffset = position * RECORD_SIZE;
    BYTE aux[4];
    dwordToBytes(record->inodeNumber, aux);

    blockBuffer[0 + positionOffset] = record->TypeVal;
    memcpy( blockBuffer + positionOffset + 1, record->name, 59);
    memcpy(blockBuffer + positionOffset + 60,aux,4);
}

int getInodeById(int id, INODE *inode) {
    int relativePossitionOnInodeBlock = id % INODES_PER_SECTOR;
    int inodeSector = id / INODES_PER_SECTOR + iNodeAreaOffset;
    BYTE buffer[SECTOR_SIZE];
    if (inodeSector >= blockToSector(blockAreaOffset)) {
        printf("[ERROR] iNode out of bound\n");
        return ERROR_CODE;
    }

    if (read_sector(inodeSector, buffer) != SUCCESS_CODE) {
        printf("[ERROR] Erro while reading the inode on disk\n");
        return ERROR_CODE;
    }

    inode->blocksFileSize = getDoubleWord(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 0);
    inode->bytesFileSize = getDoubleWord(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 4);
    inode->dataPtr[0] = getDoubleWord(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 8);
    inode->dataPtr[1] = getDoubleWord(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 12);
    inode->singleIndPtr = getDoubleWord(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 16);
    inode->doubleIndPtr = getDoubleWord(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 20);
    inode->reservado[0] = getDoubleWord(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 24);
    inode->reservado[1] = getDoubleWord(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 28);

    return SUCCESS_CODE;
}

int writeInode(int id, INODE *inode) {
    BYTE buffer[SECTOR_SIZE];
    int relativePossitionOnInodeBlock = id % INODES_PER_SECTOR;
    int inodeSector = id / INODES_PER_SECTOR + iNodeAreaOffset;
    if (inodeSector >= blockToSector(blockAreaOffset)) {
        printf("[ERROR] iNode out2 of bound\n");
    }
    read_sector(inodeSector, buffer);

    BYTE aux[4];

    dwordToBytes(inode->blocksFileSize, aux);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 0, aux, 4);

    dwordToBytes(inode->bytesFileSize, aux);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 4, aux, 4);

    dwordToBytes(inode->dataPtr[0], aux);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 8, aux, 4);

    dwordToBytes(inode->dataPtr[1], aux);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 12, aux, 4);

    dwordToBytes(inode->singleIndPtr, aux);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 16, aux, 4);

    dwordToBytes(inode->doubleIndPtr, aux);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 20, aux, 4);

    dwordToBytes(inode->reservado[0], aux);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 24, aux, 4);

    dwordToBytes(inode->reservado[1], aux);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 28, aux, 4);

    return write_sector(inodeSector, buffer);
}

int getBlock(int id, BYTE *blockBuffer) {
    BYTE buffer[SECTOR_SIZE];
    int i = 0;
    for (i = 0; i < superBlock.blockSize; i++) {
        if (read_sector(blockToSector(id) + i, buffer) != 0) {
            printf("[ERROR] Error while reading block data. %d\n", id);
            return ERROR_CODE;
        }
        memcpy(blockBuffer + (i * SECTOR_SIZE), buffer, SECTOR_SIZE);
    }
    return 0;
}

int writeBlock(int id, BYTE *blockBuffer) {
    BYTE buffer[SECTOR_SIZE];
    int i = 0;
    for (i = 0; i < superBlock.blockSize; i++) {
        memcpy(buffer, blockBuffer + (i * SECTOR_SIZE), SECTOR_SIZE);
        write_sector(blockToSector(id) + i, buffer);
    }
    return 0;
}

int getOpenFileStruct() {
    int i;
    for (i = 0; i < MAX_OPEN_FILES_SIMULTANEOUSLY; i++) {
        if (openFiles[i].active == FALSE) {
            return i;
        }
    }
    return ERROR_CODE;
}

int allocInode() {
    int inodeId = searchBitmap2(INODE_BITMAP, FREE);
    if (inodeId > 0) {
        setBitmap2(INODE_BITMAP, inodeId, OCCUPIED);
    }
    return inodeId;
}

int allocBlock() {
    int blockId = searchBitmap2(DATA_BITMAP, FREE);
    if (blockId > 0) {
        setBitmap2(DATA_BITMAP, blockId, OCCUPIED);
    }
    return blockId;
}

int getNextBlockId(int lastBlockIndex, INODE *inode) {
    if (lastBlockIndex + 1 == inode->blocksFileSize) {
        // printf("lastBlockIndex: %d inode->blocksFileSize: %d\n",lastBlockIndex,inode->blocksFileSize);
        return INVALID_PTR;
    }

    if (lastBlockIndex < 1) {
        return inode->dataPtr[lastBlockIndex + 1];
    }

    if (lastBlockIndex + 1 < (blockBufferSize / sizeof(DWORD)) + 2) {
        lastBlockIndex -= 2;
        if (inode->singleIndPtr == INVALID_PTR) {
            return INVALID_PTR;
        }
        BYTE bufferBlock[blockBufferSize];
        getBlock(inode->singleIndPtr, bufferBlock);
        return getDoubleWord(bufferBlock + (sizeof(DWORD) * (lastBlockIndex + 1)));
    } else {

        lastBlockIndex -= (blockBufferSize / sizeof(DWORD)) + 2;
        int nextBlockIndex = 1 + lastBlockIndex;
        if (inode->doubleIndPtr == INVALID_PTR) {
            return INVALID_PTR;
        }
        BYTE bufferBlock[blockBufferSize];
        getBlock(inode->doubleIndPtr, bufferBlock);

        int currentIndirectBlock = getDoubleWord(
                bufferBlock + ((nextBlockIndex / (blockBufferSize / (sizeof(DWORD)))) * sizeof(DWORD)));
        int currentIndirectBlockOffset = nextBlockIndex % (blockBufferSize / (sizeof(DWORD)));
        getBlock(currentIndirectBlock, bufferBlock);

        int returnBlockId = getDoubleWord(bufferBlock + (currentIndirectBlockOffset * sizeof(DWORD)));

        return returnBlockId;
    }
}

int assignBlockToInode(int blockIndex, int freeBlockId, INODE *inode) {
    if (blockIndex < 2) {
        inode->dataPtr[blockIndex] = freeBlockId;
        return SUCCESS_CODE;
    }

    if (blockIndex < (blockBufferSize / sizeof(DWORD)) + 2) {
        if (inode->singleIndPtr == INVALID_PTR) {
            int newBlockId = allocBlock();
            inode->singleIndPtr = newBlockId;
        }
        BYTE blockBuffer[blockBufferSize];
        getBlock(inode->singleIndPtr, blockBuffer);
        BYTE bytes[4];
        dwordToBytes(freeBlockId, bytes);
        memcpy(blockBuffer + (blockIndex - 2) * sizeof(DWORD), bytes, sizeof(DWORD));
        writeBlock(inode->singleIndPtr, blockBuffer);
    } else {
        if (inode->doubleIndPtr == INVALID_PTR) {
            int newBlockId = allocBlock();
            inode->doubleIndPtr = newBlockId;
        }

        int inderectPointer;
        BYTE blockBuffer[blockBufferSize];

        int blockBufferOffset =
                ((blockIndex - 2 - (blockBufferSize / sizeof(DWORD))) / (blockBufferSize / sizeof(DWORD))) *
                (sizeof(DWORD));
        if (((blockIndex - 2) % (blockBufferSize / sizeof(DWORD))) == 0) {

            inderectPointer = allocBlock();
            getBlock(inode->doubleIndPtr, blockBuffer);
            BYTE bytes[4];
            dwordToBytes(inderectPointer, bytes);
            memcpy(blockBuffer + blockBufferOffset, bytes, sizeof(DWORD));
            writeBlock(inode->doubleIndPtr, blockBuffer);

        } else {
            getBlock(inode->doubleIndPtr, blockBuffer);
            inderectPointer = getDoubleWord(blockBuffer + (blockBufferOffset));
        }

        int blockPointerOffset = ((blockIndex - 2) % (blockBufferSize / sizeof(DWORD)));
        getBlock(inderectPointer, blockBuffer);
        BYTE bytes[4];
        dwordToBytes(freeBlockId, bytes);
        memcpy(blockBuffer + (blockPointerOffset * sizeof(DWORD)), bytes, sizeof(DWORD));
        writeBlock(inderectPointer, blockBuffer);
    }
    return 0;
}

int desallocBlocksOfInode(int from, int to, INODE *inode) {
    if (from > to) {
        printf("YOU HAVE DONE SHIT WHILE PROGRAMMING\n");
        return ERROR_CODE;
    }
    int i;
    for (i = from; i < to; i++) {
        // printf("Apagou\n");
        if (desallocBlock(getNextBlockId(i - 1, inode)) != 0) {
            printf("[ERROR] ERROR WHILE desallocBlocksOfInode\n");
            return ERROR_CODE;
        }
    }

    if (from < 2) {
        for (i = from; i < 2 && i < to; i++) {
            if (desallocBlock(inode->dataPtr[i]) != 0) {
                printf("[ERROR] ERROR WHILE desallocBlocksOfInode\n");
                return ERROR_CODE;
            }
            inode->dataPtr[i] = INVALID_PTR;
        }
    }
    if (from < 3 && i < to) {
        if (desallocBlock(inode->singleIndPtr) != 0) {
            printf("[ERROR] ERROR WHILE desallocBlocksOfInode\n");
            return ERROR_CODE;
        }
        inode->singleIndPtr = INVALID_PTR;
    }

    BYTE bufferBlock[blockBufferSize];

    if (from < (blockBufferSize / sizeof(DWORD)) + 2 && to > (blockBufferSize / sizeof(DWORD)) + 2) {
        getBlock(inode->doubleIndPtr, bufferBlock);

        for (i = (blockBufferSize / sizeof(DWORD)) + 2; i < to; i++) {
            if (desallocBlock(bufferBlock[i * sizeof(DWORD)]) != FREE) {
                printf("[ERROR] ERROR WHILE desallocBlocksOfInode\n");
                return ERROR_CODE;
            }
        }

        if (desallocBlock(inode->doubleIndPtr) != FREE) {
            printf("[ERROR] ERROR WHILE desallocBlocksOfInode\n");
            return ERROR_CODE;
        }
        inode->doubleIndPtr = INVALID_PTR;
    }
    return 0;
}

int desallocBlock(int blockId) {
    return setBitmap2(DATA_BITMAP, blockId, FREE);
}

int createNewRecordOnRecord(RECORD *path, RECORD *newRecord, INODE *inode){
    int currentMaxNumberOfRecordsOnThisRecord = inode->blocksFileSize * numberOfRecordsPerBlock;
    int numOfRecordOnInode = (inode->bytesFileSize/sizeof(RECORD));
    if(currentMaxNumberOfRecordsOnThisRecord <= numOfRecordOnInode){
        int newBlockId = allocBlock();
        if(newBlockId < 0){
            // printf("AAAAAAAAAAAsAAAAAAAA %s - %s\n", path->name,newRecord->name);
            return ERROR_CODE;
        }
        if(assignBlockToInode(inode->blocksFileSize, newBlockId,inode) == SUCCESS_CODE){
            return ERROR_CODE;
        }
        inode->blocksFileSize ++;
        writeInode(path->inodeNumber,inode);

    }
    if(setRecordAtIndex(path->inodeNumber,numOfRecordOnInode,newRecord) != SUCCESS_CODE){
        return SUCCESS_CODE;
    }
    inode->bytesFileSize += sizeof(RECORD);
    writeInode(path->inodeNumber,inode);

    return SUCCESS_CODE;
}

int insertAnRecordOnRecord(RECORD *path, RECORD *newRecord){
    INODE inode;
    getInodeById(path->inodeNumber, &inode);
    int numOfRecordOnInode = (inode.bytesFileSize/sizeof(RECORD));
    int i = 0;
    RECORD record;

    for(i = 0; i<numOfRecordOnInode; i++){
        getRecordByIndex(path->inodeNumber, i, &record);
        if(record.TypeVal == TYPEVAL_INVALIDO){
            if(setRecordAtIndex(path->inodeNumber,i,newRecord) == SUCCESS_CODE){
                return SUCCESS_CODE;
            }
            else{
                printf("Error when trying to save the record on record\n");
                return ERROR_CODE;
            }
        }
    }
    // return 0;
    return createNewRecordOnRecord(path, newRecord, &inode);
}

int createFile(char *filename, BYTE typeVal){
    if (initialized == FALSE) {
        initialize();
    }
    char onlyFilename[59];
    char filePath[255];
    getFileName(filename,onlyFilename,filePath);
    printf("FILEPATH %s\n",filePath );
    printf("FILENAME %s\n",onlyFilename );
    if (validName(onlyFilename) != TRUE) {
        return ERROR_CODE;
    }
    RECORD record;
    INODE inode;
    if(getRecordByName(filename,&record) != ERROR_CODE_FILE_NOT_FOUND){
        return ERROR_CODE;
    }

    inode.blocksFileSize = 1;
    inode.bytesFileSize = 0;
    inode.dataPtr[0] = allocBlock();
    inode.dataPtr[1] = INVALID_PTR;
    inode.singleIndPtr = INVALID_PTR;
    inode.doubleIndPtr = INVALID_PTR;

    if(inode.dataPtr[0] == ERROR_CODE){
        printf("[ERROR] error allocing inode");
        return ERROR_CODE;
    }

    int inodeId = allocInode();
    if(inodeId == ERROR_CODE){
        printf("[ERROR] error allocing inode");
        return ERROR_CODE;
    }
    if (writeInode(inodeId, &inode) != SUCCESS_CODE) {
        printf("[ERROR] error writing inode");
        return ERROR_CODE;
    }
    strcpy(record.name, onlyFilename);
    record.TypeVal = typeVal;
    record.inodeNumber = inodeId;

    RECORD pathRecord;
    getRecordByName(filePath,&pathRecord);

    if(insertAnRecordOnRecord(&pathRecord, &record) == ERROR_CODE){
        return ERROR_CODE;
    }
    if(typeVal == TYPEVAL_DIRETORIO){
        RECORD writtenDir;
        // printf("resultado do create: %d\n",getRecordByName(filename,&writtenDir));
        RECORD parentDir;
        strcpy(parentDir.name,"..");
        parentDir.TypeVal = TYPEVAL_DIRETORIO;
        parentDir.inodeNumber = pathRecord.inodeNumber;
        insertAnRecordOnRecord(&writtenDir,&parentDir);

        RECORD curDir;
        strcpy(curDir.name,".");
        curDir.TypeVal = TYPEVAL_DIRETORIO;
        curDir.inodeNumber = inodeId;
        insertAnRecordOnRecord(&writtenDir,&curDir);

    }

    return SUCCESS_CODE;
}

int openFile(char *filename, BYTE typeVal){
    if (initialized == FALSE) {
        initialize();
    }

    int openFileIndex;

    RECORD record;


    if(getRecordByName(filename, &record) != SUCCESS_CODE){
        return getRecordByName(filename, &record);

    }

    openFileIndex = getOpenFileStruct();
    if (record.TypeVal != typeVal) {
        printf("Can't open these kind of file\n");
    }

    if (openFileIndex == ERROR_CODE) {
        // printf("Maximum number of open files reached\n");
        return ERROR_CODE;
    }

    INODE inode;

    getInodeById(record.inodeNumber, &inode);

    openFiles[openFileIndex].active = TRUE;
    openFiles[openFileIndex].inodeId = record.inodeNumber;
    openFiles[openFileIndex].openBlockId = inode.dataPtr[0];
    openFiles[openFileIndex].currentPointer = 0;

    return openFileIndex;
}

int deleteFile(char *filename, BYTE typeVal){
    char onlyFilename[59];
    char filePath[255];
    INODE inode;
    RECORD path;
    RECORD record;

    RECORD invalidRecord;
    invalidRecord.TypeVal=TYPEVAL_INVALIDO;
    invalidRecord.inodeNumber = INVALID_PTR;

    getFileName(filename,onlyFilename,filePath);
    if(getRecordByName(filePath, &path) != SUCCESS_CODE){
        return ERROR_CODE;
    }

    getInodeById(path.inodeNumber, &inode);
    int numOfRecordOnInode = (inode.bytesFileSize/sizeof(RECORD));

    int i = 0;
    for(i = 0; i<numOfRecordOnInode; i++){
        getRecordByIndex(path.inodeNumber, i, &record);
        if(strcmp(record.name,onlyFilename) == 0 && record.TypeVal == typeVal){
            truncate(0,0,path.inodeNumber);
            desallocBlock(inode.dataPtr[0]);
            setBitmap2(INODE_BITMAP, path.inodeNumber, FREE);
            if(setRecordAtIndex(path.inodeNumber,i,&invalidRecord) == SUCCESS_CODE){
                return SUCCESS_CODE;
            }
            else{
                printf("Error when trying to delete an record\n");
                return ERROR_CODE;
            }
        }
    }


    return 0;
}

int truncate(int currentPointerOffset, int currentBlockOffset, int inodeId){
    INODE inode;
    getInodeById(inodeId, &inode);
    desallocBlocksOfInode(currentBlockOffset + 1, inode.blocksFileSize, &inode);

    inode.bytesFileSize = currentPointerOffset;
    inode.blocksFileSize = currentBlockOffset + 1;

    writeInode(inodeId, &inode);
    return SUCCESS_CODE;

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int create2(char *filename) {
    if(createFile(filename,TYPEVAL_REGULAR) == SUCCESS_CODE ){
        return open2(filename);
    }
    return ERROR_CODE;
}

int identify2(char *name, int size) {
    return 0;
}

int delete2(char *filename) {
    return deleteFile(filename,TYPEVAL_REGULAR);
}

// Função que abre um arquivo existente no disco.
FILE2 open2(char *filename) {
    return openFile(filename, TYPEVAL_REGULAR);
}

int close2(FILE2 handle) {
    if(initialized == FALSE){
        initialize();
    }
    if(handle >= 0 && handle <= 9){
        if(openFiles[handle].active == TRUE){
            openFiles[handle].active = FALSE;
            return SUCCESS_CODE;
        }
    }
    return ERROR_CODE;
}

int read2(FILE2 handle, char *buffer, int size) {
    if (initialized == FALSE) {
        initialize();
    }

    if (size < 0) {
        return ERROR_CODE;
    }
    int currentPointerOffset = openFiles[handle].currentPointer;
    int currentBlockOffset = (currentPointerOffset -1) / blockBufferSize;
    int blockCurrentPointerOffset = (currentPointerOffset % blockBufferSize);

    INODE inode;

    int numOfBytesReaded;

    getInodeById(openFiles[handle].inodeId, &inode);

    if(currentPointerOffset > inode.bytesFileSize){
        currentPointerOffset = inode.bytesFileSize;
        currentBlockOffset = (currentPointerOffset - 1) / blockBufferSize;
        blockCurrentPointerOffset = (currentPointerOffset % blockBufferSize);

        openFiles[handle].openBlockId = getNextBlockId(currentBlockOffset -1, &inode);
    }

    if (openFiles[handle].openBlockId == INVALID_PTR) {
        printf("[ERROR] Error while reading the file, the pointer to the block was an invalid pointer\n");
    }

    if (inode.bytesFileSize < currentPointerOffset + size) {
        size = inode.bytesFileSize - currentPointerOffset;
    }

    numOfBytesReaded = size;

    BYTE blockBuffer[blockBufferSize];
    int bufferOffset = 0;
    while (currentPointerOffset + size > ((currentBlockOffset + 1) * blockBufferSize)  ) {
        int partSize = (currentBlockOffset + 1) * blockBufferSize - currentPointerOffset ;

        if (getBlock(openFiles[handle].openBlockId, blockBuffer) != SUCCESS_CODE) {
            printf("[ERROR] gettingblock %d on currentPointer: %d blockOffset: %d \n", openFiles[handle].openBlockId,
                   currentPointerOffset, currentBlockOffset);
        }
        memcpy(buffer + bufferOffset, blockBuffer + blockCurrentPointerOffset, partSize);
        bufferOffset += partSize;
        currentPointerOffset += partSize;
        size -= partSize;
        blockCurrentPointerOffset = 0;

        // printf("currentPointerOffset %d =partSize: %d  size: %d 1023:%d blockCurrentPointerOffset: %d\n",
        //     currentPointerOffset,partSize,size,blockCurrentPointerOffset + partSize,blockCurrentPointerOffset );

        openFiles[handle].openBlockId = getNextBlockId(currentBlockOffset, &inode);
        if (openFiles[handle].openBlockId == INVALID_PTR) {
            printf("[ERROR] Error reading nextBlock %d %d\n", currentBlockOffset, inode.bytesFileSize);
            return -1;
        }
        currentBlockOffset++;

    }
    getBlock(openFiles[handle].openBlockId, blockBuffer);
    memcpy(buffer + bufferOffset, blockBuffer + blockCurrentPointerOffset, size);

    // printf("%d %d %d\n",buffer[bufferOffset + size -1],blockBuffer[blockCurrentPointerOffset + size -1], blockCurrentPointerOffset + size -1);
    currentPointerOffset += size;

    openFiles[handle].currentPointer = currentPointerOffset;

    // printf("[buffer[%d] %d blockBuffer[%d] %d size: %d currentPointer:%d\n",bufferOffset + size-1, buffer[bufferOffset + size -1],
    //     blockCurrentPointerOffset + size-1,blockBuffer[blockCurrentPointerOffset + size - 1],size,currentPointerOffset);
    return numOfBytesReaded;
}

int write2(FILE2 handle, char *buffer, int size) {
    if (initialized == FALSE) {
        initialize();
    }
    int currentPointerOffset = openFiles[handle].currentPointer;
    int currentBlockOffset = (currentPointerOffset - 1) / blockBufferSize;
    int blockCurrentPointerOffset = (currentPointerOffset % blockBufferSize);
    if (size < 0) {
        return ERROR_CODE;
    }
    if (currentPointerOffset + size > ((2 + (SECTOR_SIZE * superBlock.blockSize / sizeof(WORD)) +
                                        ((SECTOR_SIZE * superBlock.blockSize / sizeof(WORD)) *
                                         (SECTOR_SIZE * superBlock.blockSize / sizeof(WORD)))) *
                                       (SECTOR_SIZE * superBlock.blockSize))) {

            size = ((2 + (SECTOR_SIZE * superBlock.blockSize / sizeof(WORD)) +
                 ((SECTOR_SIZE * superBlock.blockSize / sizeof(WORD)) *
                  (SECTOR_SIZE * superBlock.blockSize / sizeof(WORD)))) * (SECTOR_SIZE * superBlock.blockSize)) -
               currentPointerOffset;
    }
    int bytesWriten = size;

    INODE inode;// = malloc(INODE_SIZE);

    getInodeById(openFiles[handle].inodeId, &inode);

    if(currentPointerOffset > inode.bytesFileSize){
        currentPointerOffset = inode.bytesFileSize;
        currentBlockOffset = (currentPointerOffset - 1) / blockBufferSize;
        blockCurrentPointerOffset = (currentPointerOffset % blockBufferSize);

        openFiles[handle].openBlockId = getNextBlockId(currentBlockOffset -1, &inode);

    }

    BYTE blockBuffer[blockBufferSize];
    int bufferOffset = 0;

    while (currentPointerOffset + size > ((currentBlockOffset + 1) * blockBufferSize) ) {
        int partSize = (currentBlockOffset + 1) * blockBufferSize - currentPointerOffset ;

        if (getBlock(openFiles[handle].openBlockId, blockBuffer) != SUCCESS_CODE) {
            printf("[ERROR] gettingblock %d on currentPointer: %d blockOffset: %d\n", openFiles[handle].openBlockId,
                   currentPointerOffset, currentBlockOffset);
        }

        // printf("currentPointerOffset %d =partSize: %d  size: %d 1023:%d blockCurrentPointerOffset: %d blockOffset: %d\n",
        //     currentPointerOffset,partSize,size,blockCurrentPointerOffset + partSize,blockCurrentPointerOffset,currentBlockOffset );

        memcpy(blockBuffer + blockCurrentPointerOffset, buffer + bufferOffset, partSize);
        writeBlock(openFiles[handle].openBlockId, blockBuffer);

        bufferOffset += partSize;
        currentPointerOffset += partSize;
        blockCurrentPointerOffset = 0;
        size -= partSize;

        openFiles[handle].openBlockId = getNextBlockId(currentBlockOffset, &inode);

        if (openFiles[handle].openBlockId == 0) {
            printf("\n[ERROR] Internal Error when trying to write a file\n");
            return ERROR_CODE;
        }
        if (openFiles[handle].openBlockId == INVALID_PTR) {
            int freeBlockId = allocBlock();
            if (freeBlockId <= 0) {
                printf("Cannot find free space");
                return ERROR_CODE;
            }
            inode.blocksFileSize++;

            assignBlockToInode(currentBlockOffset + 1, freeBlockId, &inode);

            openFiles[handle].openBlockId = freeBlockId;

        }
        currentBlockOffset++;

    }

    getBlock(openFiles[handle].openBlockId, blockBuffer);

    memcpy(blockBuffer + blockCurrentPointerOffset, buffer + bufferOffset, size);
    writeBlock(openFiles[handle].openBlockId, blockBuffer);
    currentPointerOffset += size;

    if (currentPointerOffset > inode.bytesFileSize)
        inode.bytesFileSize = currentPointerOffset;

    if (currentBlockOffset > inode.blocksFileSize)
        inode.blocksFileSize = currentBlockOffset;

    writeInode(openFiles[handle].inodeId, &inode);
    openFiles[handle].currentPointer = currentPointerOffset;

    return bytesWriten;
}

int truncate2(FILE2 handle) {
    if (initialized == FALSE) {
        initialize();
    }
    int currentPointerOffset = openFiles[handle].currentPointer;
    int currentBlockOffset = currentPointerOffset / blockBufferSize;

    return truncate(currentPointerOffset,currentBlockOffset,openFiles[handle].inodeId);
}

int seek2(FILE2 handle, DWORD offset) {
    if (initialized == FALSE) {
        initialize();
    }
    INODE inode;

    getInodeById(openFiles[handle].inodeId, &inode);

    if (offset == -1) {
        openFiles[handle].currentPointer = inode.bytesFileSize - 1;
        int currentBlockOffset = openFiles[handle].currentPointer / blockBufferSize;
        openFiles[handle].openBlockId = getNextBlockId(currentBlockOffset - 1, &inode);
        return 0;
    }

    if ((int) (openFiles[handle].currentPointer + offset) < 0) {
        offset = 0 - openFiles[handle].currentPointer;
    }

    if (openFiles[handle].currentPointer + offset > inode.bytesFileSize) {
        offset = inode.bytesFileSize - openFiles[handle].currentPointer;
    }

    openFiles[handle].currentPointer += offset;
    int currentBlockOffset = openFiles[handle].currentPointer / blockBufferSize;
    openFiles[handle].openBlockId = getNextBlockId(currentBlockOffset - 1, &inode);
    return 0;
}

int mkdir2(char *pathname) {
    return createFile(pathname, TYPEVAL_DIRETORIO);
}

int rmdir2(char *pathname) {
    if (initialized == FALSE) {
        initialize();
    }
    return 0;
    RECORD dir;
    RECORD auxRecord;
    if(getRecordByName(pathname, &dir) != SUCCESS_CODE || dir.TypeVal != TYPEVAL_DIRETORIO){
        return ERROR_CODE;
    }
    INODE inode;

    getInodeById(dir.inodeNumber,&inode);
    int i;
    int numOfRecordOnInode = (inode.bytesFileSize/sizeof(RECORD));
    for(i=0;i<numOfRecordOnInode; i++){
        if(getRecordByIndex(dir.inodeNumber, i, &auxRecord) == SUCCESS_CODE){
            if(auxRecord.TypeVal != TYPEVAL_INVALIDO)
                return ERROR_CODE;
        }
    }
    return deleteFile(pathname, TYPEVAL_DIRETORIO);
}

/*-----------------------------------------------------------------------------
Fun��o:	Altera o diret�rio atual de trabalho (working directory).
		O caminho desse diret�rio � informado no par�metro "pathname".
		S�o considerados erros:
			(a) qualquer situa��o que impe�a a realiza��o da opera��o
			(b) n�o exist�ncia do "pathname" informado.

Entra:	pathname -> caminho do novo diret�rio de trabalho.

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
		Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int chdir2(char *pathname) {
    if (initialized == FALSE) {
        initialize();
    }


    return 0;
}


int getcwd2(char *pathname, int size) {
    if (initialized == FALSE) {
        initialize();
    }
    return 0;
}


DIR2 opendir2(char *pathname) {
    if (initialized == FALSE) {
        initialize();
    }
    return 0;
}

int readdir2(DIR2 handle, DIRENT2 *dentry) {
    if (initialized == FALSE) {
        initialize();
    }
    return 0;
}

int closedir2(DIR2 handle) {
    if (initialized == FALSE) {
        initialize();
    }
    return 0;
}
