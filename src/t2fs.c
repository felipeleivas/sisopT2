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
//int i;
////////////////////////////////////////

void initialize();
int validName(char *filename);
int getInodeById(int id, INODE* inode);
int writeInode(int id, INODE* inode);
int getBlock(int id, BYTE* blockBuffer);
int getNextBlock(int lastBlockIndex, INODE* inode);
int getOpenFileStruct();
void getRecordOnBlockByPosition(BYTE *blockBuffer, int position, RECORD *record);
int getRecordByIndex(int iNodeId, int indexOfRecord, RECORD *record);
int getRecordByName(int inodeNumber, char *filename, RECORD* record);
int writeBlock(int id, BYTE* blockBuffer);
int assignBlockToInode(int blockIndex, int freeBlockId, INODE* inode );

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


// Recebe o ID do bloco e retorna o setor do disco que ele está.
unsigned int blockToSector(unsigned int block){
    return block * superBlock->blockSize;
}

//
int main(){
    FILE2 f = open2("file3");
    char buffer[(1024 * 1024)]={0};
//    read2(f,buffer,1);
    int i;

    for(i = 0 ; i<256; i++){
        printf("%c",buffer[i]);
    }
    printf("\n");
//    read2(f,buffer,14);
    for(i = 0 ; i<256; i++){
        printf("%c",buffer[i]);
    }

    char buffer2[256]={0};
    strcpy(buffer2,"testFeli");
//    for(i = 0; i <(1024 * 258) -1 ; i++){
//        buffer2[0] = 48 + (i%10);
//        if(write2(f,buffer2,1) == ERROR_CODE)
//            printf("ERROR");
//    }
//    buffer2[0] = 'X';
//    if(write2(f,buffer2,1) == ERROR_CODE)
//        printf("ERROR");


    puts("");
    FILE2 f2 = open2("file3");
    int k = read2(f2,buffer,1024*1024);
        for(i=0; i <k; i++){
            printf("%c",buffer[i]);
        }
    printf("\n%d\n",k);

    return 0;
}

// Inicialização do Sistema
void initialize() {
    superBlock = malloc(sizeof(*superBlock));
    if(!superBlock) {
        printf("Malloc error\n");
        return;
    }

    if(read_sector(SUPER_BLOCK_AREA, buffer) != 0) {
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
    iNodeAreaOffset = blockToSector(superBlock->freeBlocksBitmapSize)
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

// Verificação do nome do arquivo
int validName(char *filename) {
    int i;
    for(i = 0; i <strlen(filename); i++) {
        if( !((('0' <= filename[i]) && (filename[i] <= '9'))  ||
              (('a' <= filename[i]) && (filename[i] <= 'z'))  ||
              (('A' <= filename[i]) && (filename[i] <= 'Z'))  )) {
            return FALSE;
        }
    }
    return TRUE;
}

// Recebe o ID do inode e um ponteiro para a estrutura de um inode,
// localiza os dados desse inode no disco, e aloca esses dados para
// a estrutura do inode recebida.
int getInodeById(int id, INODE* inode) {
    int relativePossitionOnInodeBlock = id % INODES_PER_SECTOR;
    int inodeSector = id / INODES_PER_SECTOR + iNodeAreaOffset;

    if(inodeSector >= blockToSector(blockAreaOffset)) {
        printf("[ERROR] iNode out of bound\n");
        return ERROR_CODE;
    }

    if(read_sector(inodeSector,buffer) != SUCCESS_CODE) {
        printf("[ERROR] Erro while reading the inode on disk\n");
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

// Recebe o ID do inode e um ponteiro para a estrutura de um inode,
// e escreve os dados dessa estrutura no disco.
int writeInode(int id, INODE* inode) {
    int relativePossitionOnInodeBlock = id % INODES_PER_SECTOR;
    int inodeSector = id / INODES_PER_SECTOR + iNodeAreaOffset;
    if(inodeSector >= blockToSector(blockAreaOffset)) {
        printf("[ERROR] iNode out2 of bound\n");
    }
    read_sector(inodeSector, buffer);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock),dwordToBytes(inode->blocksFileSize), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 4, dwordToBytes(inode->bytesFileSize), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 8, dwordToBytes(inode->dataPtr[0]), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 12, dwordToBytes(inode->dataPtr[1]), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 16, dwordToBytes(inode->singleIndPtr), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 20, dwordToBytes(inode->doubleIndPtr), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 24, dwordToBytes(inode->reservado[0]), 4);
    memcpy(buffer + (INODE_SIZE * relativePossitionOnInodeBlock) + 28, dwordToBytes(inode->reservado[1]), 4);

    return write_sector(inodeSector, buffer);
}

// Recebe o ID do bloco e um buffer, e aloca os dados desse bloco
// que estão no disco para dentro desse buffer.
int getBlock(int id, BYTE* blockBuffer) {
    int i = 0;
    for(i = 0; i < superBlock->blockSize; i++) {
        if(read_sector(blockToSector(id) + i, buffer) != 0) {
            printf("[ERROR] Error while reading block data.\n");
            return ERROR_CODE;
        }
        memcpy(blockBuffer + (i * SECTOR_SIZE), buffer, SECTOR_SIZE);
    }
    return 0;
}

// Recebe o ID do bloco e um buffer dos dados do bloco,
// e escreve no disco esse buffer.
int writeBlock(int id, BYTE* blockBuffer) {
    int i = 0;
    for(i = 0; i < superBlock->blockSize; i++){
        memcpy(buffer,blockBuffer + (i * SECTOR_SIZE), SECTOR_SIZE);
        write_sector(blockToSector(id) + i, buffer);
    }
    return 0;
}

// Recebe o ID de um inode, o nome de um arquivo e o ponteiro para uma estrutura
// record, e então percorre todos os registros dentro do bloco de dados apontado
// pelo inode, para encontrar o registro cujo nome seja igual ao do arquivo.
// Quando encontrar esse registro, carrega os dados dele para dentro da estrutura
// record.
int getRecordByName(int inodeNumber, char *filename, RECORD* record) {
    int index = 0;
    while(getRecordByIndex(inodeNumber, index, record) == SUCCESS_CODE) {
        if(strcmp(filename, record->name) == 0) {
            return SUCCESS_CODE;
        }
        index++;
    }
    printf("[ERROR] Error while getting record by name");
    return ERROR_CODE;
}

// Recebe o ID de um inode, o index do registro e o ponteiro para uma estrutura
// record, e então carrega os dados desse inode do disco, acessa o primeiro
// bloco de dados apontado por esse inode, carrega esse bloco do disco e então
// carrega os dados do registro que estão dentro desse bloco para dentro da
// estrutura record.
int getRecordByIndex(int iNodeId, int indexOfRecord, RECORD *record) {

    if(indexOfRecord < numberOfRecordsPerBlock) {
        INODE* inode = malloc(INODE_SIZE);

        if(getInodeById(iNodeId, inode) != SUCCESS_CODE) {
            printf("[ERROR] Error at getting inode on getRecordByIndex\n");
            return ERROR_CODE;
        }

        BYTE blockBuffer[blockBufferSize];
        // @question Como tu sabe que os dados do registro estão no primeiro
        // bloco de dados apontados pelo inode?
        getBlock(inode->dataPtr[0], blockBuffer);
        getRecordOnBlockByPosition(blockBuffer, indexOfRecord % numberOfRecordsPerBlock, record);

        free(inode);

        return SUCCESS_CODE;
    }
    printf("[ERROR] Error while getting record.\n");
    return ERROR_CODE;
}

// Recebe o Buffer do Bloco, a posição do registro dentro desse Bloco,
// e então aloca os dados desse registro para dentro da variável record.
void getRecordOnBlockByPosition(BYTE *blockBuffer, int position, RECORD *record) {
    int positionOffset = position * RECORD_SIZE;
    record->TypeVal = blockBuffer[0 + positionOffset];
    memcpy(record->name, blockBuffer + positionOffset + 1, 59);
    record->inodeNumber = getDoubleWord(blockBuffer + positionOffset + 60);
}

int getOpenFileStruct() {
    int i;
    for(i = 0; i < MAX_OPEN_FILES_SIMULTANEOUSLY; i++){
        if(openFiles[i].active == FALSE){
            return i;
        }
    }
    return ERROR_CODE;
}

int getNextBlock(int lastBlockIndex, INODE* inode) {
    printf("lBI: %d iBS: %d\n",lastBlockIndex,inode->blocksFileSize);
    if(lastBlockIndex == 0){
        return inode->dataPtr[1];
    }
    if(lastBlockIndex + 1  == inode->blocksFileSize){
        return INVALID_PTR;
    }
    if(lastBlockIndex + 1 < (blockBufferSize/sizeof(DWORD)) + 2) {
        lastBlockIndex -= 2;
        if(inode->singleIndPtr == INVALID_PTR) {
            return INVALID_PTR;
        }
        BYTE bufferBlock[blockBufferSize];
        getBlock(inode->singleIndPtr,bufferBlock);
        return  getDoubleWord(bufferBlock + (sizeof(DWORD) * (lastBlockIndex + 1)));
    } else {
//        exit(-1);
        lastBlockIndex -= (blockBufferSize/sizeof(DWORD)) + 2;
        int nextBlockIndex = 1 + lastBlockIndex;
        if(inode->doubleIndPtr == INVALID_PTR) {
            return INVALID_PTR;
        }
        int i;
        BYTE bufferBlock[blockBufferSize];
        getBlock(inode->doubleIndPtr,bufferBlock);
        printf("INDIRect Pointer: \n");
        for(i=0; i<256;i++) {
            printf("%d[%d] ", getDoubleWord(bufferBlock + (i * 4)),i);
        }
        puts("");
        int currentIndirectBlock = getDoubleWord(bufferBlock + ((nextBlockIndex/(blockBufferSize/(sizeof(DWORD)))) * sizeof(DWORD)));
        int currentIndirectBlockOffset = nextBlockIndex %  (blockBufferSize/(sizeof(DWORD)));
        getBlock(currentIndirectBlock,bufferBlock);
        printf("ENTROU %d %d\n",currentIndirectBlockOffset,getDoubleWord(bufferBlock + (currentIndirectBlockOffset + sizeof(DWORD))));
        for(i=0; i<256;i++) {
            printf("%d[%d] ", getDoubleWord(bufferBlock + (i * 4)),i);
        }
        puts("");
        printf("\nretorno %d\n",getDoubleWord(bufferBlock + (currentIndirectBlockOffset * sizeof(DWORD))));
//        exit(-1);
        return getDoubleWord(bufferBlock + (currentIndirectBlockOffset + sizeof(DWORD)));
    }
}

int allocInode(){
    int inodeId = searchBitmap2(INODE_BITMAP,FREE);
    if(inodeId > 0){
        setBitmap2(INODE_BITMAP,inodeId,OCCUPIED);
    }
    return inodeId;
}
int allocBlock(){
    int inodeId = searchBitmap2(DATA_BITMAP,FREE);
    if(inodeId > 0){
        setBitmap2(DATA_BITMAP,inodeId,OCCUPIED);
    }
    return inodeId;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int create2(char *filename){
//    if(initialized == FALSE){
//        initialize();
//    }
//
//    if(validName(filename) != TRUE){
//        printf("Error with filename");
//        return ERROR_CODE;
//    }
//    INODE* inode = malloc(sizeof(INODE*));
//    inode->blocksFileSize = 0;
//    inode->bytesFileSize = 34;
//    inode->dataPtr[0] = INVALID_PTR;
//    inode->singleIndPtr = INVALID_PTR;
//    inode->doubleIndPtr = INVALID_PTR;
//
    int inodeId = searchBitmap2(DATA_BITMAP,FREE);
//    if(writeInode(inodeId,inode) != SUCCESS_CODE){
//        printf("error writing inode");
//        return ERROR_CODE;
//    }
//
//    setBitmap2(INODE_BITMAP,inodeId,OCCUPIED);
//
//    RECORD* record = malloc(sizeof(RECORD*));
//    strcpy(record->name,filename);
//    record->TypeVal = TYPEVAL_REGULAR;
//    record->inodeNumber = inodeId;

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
    INODE* inode = malloc(INODE_SIZE);
    getInodeById(record->inodeNumber,inode);
    openFiles[openFileIndex].active = TRUE;
    openFiles[openFileIndex].inodeId = record->inodeNumber;
    openFiles[openFileIndex].openBlockId = inode->dataPtr[0];
    openFiles[openFileIndex].currentPointer = 0;

    free(inode);
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
    int numOfBytesReaded;
    getInodeById(openFiles[handle].inodeId,inode);

//    puts("\nREAD: ");
//    printf("currentPointerOffset %d\n", currentPointerOffset);
//    printf("size %d\n", size);
//    printf("currentBlockOffset %d\n", currentBlockOffset);
//    printf("fileMaxSize %d\n", inode->bytesFileSize);



    if(inode->bytesFileSize < currentPointerOffset + size){
        size = inode->bytesFileSize - currentPointerOffset;
    }
    numOfBytesReaded = size;

    BYTE blockBuffer[blockBufferSize];
    int bufferOffset = 0;

    while(currentPointerOffset + size > (currentBlockOffset + 1) * blockBufferSize - 1) {
        int partSize = (currentBlockOffset + 1) * blockBufferSize - currentPointerOffset - 1;
        getBlock(openFiles[handle].openBlockId,blockBuffer);

        memcpy(buffer + bufferOffset,blockBuffer + blockCurrentPointerOffset,partSize);
        bufferOffset += partSize;
        currentPointerOffset += partSize;
        size -= partSize;
        blockCurrentPointerOffset = 0;

        printf("\n\nsinglePointer s ind: %d\n",inode->singleIndPtr);
        printf("doubleePointer ind: %d\n",inode->doubleIndPtr);
        printf("dirPointer[0] ind: %d\n",inode->dataPtr[0]);
        printf("dirPointer[1] ind: %d\n",inode->dataPtr[1]);
        printf("openblockid: %d\n",openFiles[handle].openBlockId);

        openFiles[handle].openBlockId = getNextBlock(currentBlockOffset, inode);
        if(openFiles[handle].openBlockId == INVALID_PTR){
            printf("Error reading nextBlock\n");
            return  -1;
        }
        currentBlockOffset++;
    }
    getBlock(openFiles[handle].openBlockId,blockBuffer);
    memcpy(buffer + bufferOffset,blockBuffer + blockCurrentPointerOffset,size);
    currentPointerOffset += size;


    openFiles[handle].currentPointer = currentPointerOffset;
    free(inode);
    return numOfBytesReaded;
}

int write2 (FILE2 handle, char *buffer, int size){
    if(initialized == FALSE){
        initialize();
    }
    int currentPointerOffset = openFiles[handle].currentPointer;
    int currentBlockOffset = currentPointerOffset/blockBufferSize;
    int blockCurrentPointerOffset = (currentPointerOffset%blockBufferSize);

    INODE* inode = malloc(INODE_SIZE);
    if(inode == NULL){
        printf("Error malloc inode\n");
        return ERROR_CODE;
    }
    getInodeById(openFiles[handle].inodeId,inode);

    BYTE blockBuffer[blockBufferSize];
    int bufferOffset = 0;
    while(currentPointerOffset + size > ((currentBlockOffset + 1) * blockBufferSize) -1){
        int partSize = (currentBlockOffset + 1) * blockBufferSize - currentPointerOffset - 1;

        getBlock(openFiles[handle].openBlockId,blockBuffer);

        memcpy(blockBuffer + blockCurrentPointerOffset,buffer + bufferOffset,partSize);
        writeBlock(openFiles[handle].openBlockId,blockBuffer);

        bufferOffset += partSize;
        currentPointerOffset += partSize;
        blockCurrentPointerOffset = 0;
        size -= partSize;

        openFiles[handle].openBlockId = getNextBlock(currentBlockOffset, inode);
//        printf("\n\nsinglePointer ind: %d\n",inode->singleIndPtr);
//        printf("doubleePointer ind: %d\n",inode->doubleIndPtr);
//        printf("dirPointer[0] ind: %d\n",inode->dataPtr[0]);
//        printf("dirPointer[1] ind: %d\n",inode->dataPtr[1]);
//        printf("openblockid: %d\n",openFiles[handle].openBlockId);


        if(openFiles[handle].openBlockId == 0){
            printf("\nSAINDO PQ DEU RUIM\n");
//            exit(-1);
        }
        if(openFiles[handle].openBlockId == INVALID_PTR){
            int freeBlockId = allocBlock();
            if(freeBlockId <= 0 ){
                printf("Cannot find free space");
                return ERROR_CODE;
            }
            inode->blocksFileSize++;

            assignBlockToInode(currentBlockOffset + 1, freeBlockId, inode);

            openFiles[handle].openBlockId = freeBlockId;

        }
        currentBlockOffset++;

//        printf("openBlock Id : %d\n",openFiles[handle].openBlockId);
//        printf("currentBlockOffset : %d\n",currentBlockOffset);
//        printf("currentPointerOffset: %d\n",currentPointerOffset);
//        printf("size: %d\n",size);
//        printf("partSize: %d\n",partSize);
//        printf("currentBlockOffset: %d\n",currentBlockOffset);
    }
    getBlock(openFiles[handle].openBlockId,blockBuffer);
    memcpy(blockBuffer + blockCurrentPointerOffset,buffer + bufferOffset,size);
    writeBlock(openFiles[handle].openBlockId,blockBuffer);
    currentPointerOffset += size;

    if(currentPointerOffset > inode->bytesFileSize){
        inode->bytesFileSize = currentPointerOffset;
    }

    writeInode(openFiles[handle].inodeId, inode);
    openFiles[handle].currentPointer = currentPointerOffset;
    return 0;
}

int assignBlockToInode(int blockIndex, int freeBlockId, INODE* inode ){
    if(blockIndex< 2){
        inode->dataPtr[blockIndex] = freeBlockId;
        return SUCCESS_CODE;

    }
    if(blockIndex < (blockBufferSize/sizeof(DWORD)) + 2){
        if(inode->singleIndPtr == INVALID_PTR){
            int newBlockId = allocBlock();
            inode->singleIndPtr = newBlockId;
        }
        BYTE blockBuffer[blockBufferSize];
        getBlock(inode->singleIndPtr,blockBuffer);
        memcpy(blockBuffer + (blockIndex - 2) * sizeof(DWORD),dwordToBytes(freeBlockId), sizeof(DWORD));
        writeBlock(inode->singleIndPtr,blockBuffer);
    }
    else{
        if(inode->doubleIndPtr == INVALID_PTR){
            int newBlockId = allocBlock();
            inode->doubleIndPtr = newBlockId;
        }
        int newIndirectBlockId = allocBlock();
        BYTE blockBuffer[blockBufferSize];
        getBlock(inode->doubleIndPtr,blockBuffer);
        memcpy(blockBuffer + ((blockIndex - 2) /(blockBufferSize * sizeof(DWORD))),dwordToBytes(newIndirectBlockId), sizeof(DWORD));
        writeBlock(inode->doubleIndPtr,blockBuffer);
//        printf("Indirect pointer -> %d %d\n",newIndirectBlockId,((blockIndex - 2) /(blockBufferSize * sizeof(DWORD))) -1);
//        int i;
//        for(i=0; i<256; i++){
//            printf("%d(%d) ",getDoubleWord(blockBuffer + (i * 4)),i);
//        }
//        printf("\nblock pointed by indirect pointer -> %d %d\n",freeBlockId,((blockIndex - 2) % (blockBufferSize / sizeof(DWORD))));

        getBlock(newIndirectBlockId,blockBuffer);
        memcpy(blockBuffer + ((blockIndex - 2) % (blockBufferSize / sizeof(DWORD))),dwordToBytes(freeBlockId), sizeof(DWORD));
        writeBlock(newIndirectBlockId,blockBuffer);
//        for(i=0; i<256; i++){
//            printf("%d(%d) ",getDoubleWord(blockBuffer + (i * 4)),i);
//        }
//                printf("asdsa\n");
//        exit(-1);
    }
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
