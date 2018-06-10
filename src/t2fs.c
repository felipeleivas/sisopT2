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

int getRecordByIndex(int iNodeId, int indexOfRecord, RECORD *record);

int getRecordByName(int inodeNumber, char *filename, RECORD *record);

int writeBlock(int id, BYTE *blockBuffer);

int assignBlockToInode(int blockIndex, int freeBlockId, INODE *inode);

nameNode *filenameTooNamesList(char *filename);

void destroyNamesList(nameNode *namesList);

int appendRecordTooDirectory(int inodeNumber, RECORD *record);

int desallocBlocksOfInode(int from, int to, INODE *inode);

int desallocBlock(int blockId);

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

BYTE *dwordToBytes(DWORD dword, BYTE *bytes) {
    // BYTE *bytes = malloc(4*sizeof(*bytes));
    // static BYTE bytes[4 * sizeof(BYTE)];
    bytes[0] = dword & 0x00FF;
    bytes[1] = (dword & 0xFF00) >> 8;
    bytes[2] = (dword & 0xFF0000) >> 16;
    bytes[3] = (dword & 0xFF000000) >> 24;
}

// }
// Recebe o ID do bloco e retorna o setor do disco que ele está.
unsigned int blockToSector(unsigned int block) {
    return block * superBlock.blockSize;
}

// Inicialização do Sistema
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

// Verificação do nome do arquivo
int validName(char *filename) {
    int i;
    for (i = 0; i < strlen(filename); i++) {
        if (!((('0' <= filename[i]) && (filename[i] <= '9')) ||
              (('a' <= filename[i]) && (filename[i] <= 'z')) ||
              (('A' <= filename[i]) && (filename[i] <= 'Z')))) {
            return FALSE;
        }
    }
    return TRUE;
}

// Verifica se o nome do arquivo é um caminho absoluto
int isAbsolutePath(char *filename) {
    if (filename[0] == '/') {
        return TRUE;
    } else {
        return FALSE;
    }
}

// Recebe um filename e transforma em uma lista de nomes.
nameNode *filenameTooNamesList(char *filename) {
    int i = 0, characterCounter = 0;
//  int numberOfWords = 1, flag = 0;

    nameNode *firstName = (nameNode *) malloc(sizeof(nameNode));

    firstName->next = NULL;
    firstName->name[0] = '/';
    firstName->name[1] = '\0';

    nameNode *actualName;
    nameNode *auxName = (nameNode *) malloc(sizeof(nameNode));
    actualName = firstName;

    for (i = 1; i < strlen(filename) + 1; i++) {

        // Chegamos na próxima barra, logo, acabou o nome
        if (filename[i] == '/') {
            auxName->name[characterCounter] = '\0';
            actualName->next = auxName;
            auxName->next = NULL;
            actualName = auxName;
            auxName = auxName->next;
            auxName = (nameNode *) malloc(sizeof(nameNode));
            characterCounter = 0;
        } else if (filename[i] == '\0') {
            auxName->name[characterCounter] = '\0';
            actualName->next = auxName;
            auxName->next = NULL;
            actualName = auxName;
            auxName = auxName->next;
            auxName = (nameNode *) malloc(sizeof(nameNode));
            characterCounter = 0;
        } else {
            // Caractere não é barra, logo, faz parte do nome
            auxName->name[characterCounter] = filename[i];
            characterCounter++;
        }
    }

    return firstName;
}

void destroyNamesList(nameNode *namesList) {
    nameNode *namesAux = namesList;
    nameNode *namesAux2 = namesAux;

    while (namesAux != NULL) {
        namesAux = namesAux->next;
        free(namesAux2);
        namesAux2 = namesAux;
    }
}

// Recebe o ID do inode e um ponteiro para a estrutura de um inode,
// localiza os dados desse inode no disco, e aloca esses dados para
// a estrutura do inode recebida.
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

// Recebe o ID do inode e um ponteiro para a estrutura de um inode,
// e escreve os dados dessa estrutura no disco.
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

// Recebe o ID do bloco e um buffer, e aloca os dados desse bloco
// que estão no disco para dentro desse buffer.
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

// Recebe o ID do bloco e um buffer dos dados do bloco,
// e escreve no disco esse buffer.
int writeBlock(int id, BYTE *blockBuffer) {
    BYTE buffer[SECTOR_SIZE];
    int i = 0;
    for (i = 0; i < superBlock.blockSize; i++) {
        memcpy(buffer, blockBuffer + (i * SECTOR_SIZE), SECTOR_SIZE);
        write_sector(blockToSector(id) + i, buffer);
    }
    return 0;
}


// Recebe o ID de um inode, o nome de um arquivo e o ponteiro para uma estrutura
// record, e então percorre todos os registros dentro do bloco de dados apontado
// pelo inode, para encontrar o registro cujo nome seja igual ao do arquivo.
// Quando encontrar esse registro, carrega os dados dele para dentro da estrutura
// record.
int getRecordByName(int inodeNumber, char *filename, RECORD *record) {
    int index = 0;

    while (getRecordByIndex(inodeNumber, index, record) == SUCCESS_CODE) {
        if (strcmp(filename, record->name) == 0) {
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
    if (indexOfRecord < numberOfRecordsPerBlock) {
        // INODE* inode = malloc(INODE_SIZE);
        INODE inode;

        if (getInodeById(iNodeId, &inode) != SUCCESS_CODE) {
            printf("[ERROR] Error at getting inode on getRecordByIndex\n");
            return ERROR_CODE;
        }


        BYTE blockBuffer[blockBufferSize];
        // @question Como tu sabe que os dados do registro estão no primeiro
        // bloco de dados apontados pelo inode?
        getBlock(inode.dataPtr[0], blockBuffer);
        getRecordOnBlockByPosition(blockBuffer, indexOfRecord % numberOfRecordsPerBlock, record);

        // free(inode);
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

int appendRecordTooDirectory(int directoryInodeNumber, RECORD *record) {
    INODE *directoryInode = malloc(sizeof(INODE *));
    getInodeById(directoryInodeNumber, directoryInode);
    BYTE blockBuffer[blockBufferSize];
    getBlock(directoryInode->dataPtr[0], blockBuffer);

    int blockToWrite = directoryInode->blocksFileSize;
    if (blockToWrite == 1) {
        // blockBuffer[directoryInode->bytesFileSize] = record;
        writeBlock(directoryInode->dataPtr[0], blockBuffer);

        return SUCCESS_CODE;
    } else if (blockToWrite == 2) {
        // blockBuffer[directoryInode->bytesFileSize - blockBufferSize] = record;
        writeBlock(directoryInode->dataPtr[1], blockBuffer);

        return SUCCESS_CODE;
    } else {
        printf("ainda n feito");
        return ERROR_CODE;
    }

    return ERROR_CODE;
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
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int create2(char *filename) {
    if (initialized == FALSE) {
        initialize();
    }

    if (validName(filename) != TRUE) {
        printf("Error with filename");
        return ERROR_CODE;
    }

    // Criação de um novo INODE
    INODE *inode = malloc(sizeof(INODE *));
    inode->blocksFileSize = 0;
    inode->bytesFileSize = 0;
    inode->dataPtr[0] = INVALID_PTR;
    inode->dataPtr[1] = INVALID_PTR;
    inode->singleIndPtr = INVALID_PTR;
    inode->doubleIndPtr = INVALID_PTR;

    // Procurar pelo inode livre no BITMAP
    int inodeId = searchBitmap2(INODE_BITMAP, FREE);
    if (writeInode(inodeId, inode) != SUCCESS_CODE) {
        printf("[ERROR] error writing inode");
        return ERROR_CODE;
    }
    setBitmap2(INODE_BITMAP, inodeId, OCCUPIED);
    // inode gravado no disco e setado como ocupado.

    // Criação da entrada do arquivo.
    RECORD *record = malloc(sizeof(RECORD *));
    strcpy(record->name, filename);
    record->TypeVal = TYPEVAL_REGULAR;
    record->inodeNumber = inodeId;

    // Gravar a entrada do arquivo nos dados do diretório.
    if (isAbsolutePath(filename)) {

        // Arquivo será gravado nos dados do diretório absoluto.
        // Encontrar a entrada do diretorio dado pelo pathname.
        nameNode *namesList = filenameTooNamesList(filename);
        nameNode *namesAux = namesList;
        RECORD *recordAux = malloc(sizeof(RECORD *));
        int actualInodeId = rootInodeId;
        while (namesAux->next != NULL) {
            getRecordByName(actualInodeId, namesAux->name, recordAux);
            actualInodeId = recordAux->inodeNumber;
            namesAux = namesAux->next;
        }

        // Aqui, recordAux deve conter a entrada do diretório onde o arquivo será criado.
        if (appendRecordTooDirectory(recordAux->inodeNumber, record) == SUCCESS_CODE) {
            return inodeId;
        } else {
            printf("[ERROR] Error in create2.\n");
            return ERROR_CODE;
        }

    } else {

        // Arquivo será gravado nos dados do diretório atual.
        if (appendRecordTooDirectory(actualDirectory.inodeNumber, record) == SUCCESS_CODE) {
            return inodeId;
        } else {
            printf("[ERROR] Error in create2.\n");
            return ERROR_CODE;
        }
    }
}

int identify2(char *name, int size) {
    return 0;
}

int delete2(char *filename) {
    return 0;
}

// Função que abre um arquivo existente no disco.
FILE2 open2(char *filename) {
    if (initialized == FALSE) {
        initialize();
    }

    int openFileIndex;

    if (isAbsolutePath(filename) || TRUE) {
        // nameNode *namesList = filenameTooNamesList(filename);
        // RECORD *record = malloc(RECORD_SIZE);
        RECORD record;
        // nameNode *namesAux = namesList;
        int inodeId = rootInodeId;

        // while(namesAux != NULL) {
        getRecordByName(inodeId, filename, &record);
        //   getRecordByName(inodeId, namesAux->name, record);
        //   inodeId = record->inodeNumber;
        //   namesAux = namesAux->next;
        //   printf("\nNome atual: %s\n", record->name);
        // }


        openFileIndex = getOpenFileStruct();
        if (record.TypeVal != TYPEVAL_REGULAR) {
            printf("Can't open these kind of file\n");
        }

        if (openFileIndex == ERROR_CODE) {
            printf("Maximum number of open files reached\n");
            return ERROR_CODE;
        }

        INODE inode;
        // INODE* inode = malloc(INODE_SIZE);
        getInodeById(record.inodeNumber, &inode);

        openFiles[openFileIndex].active = TRUE;
        openFiles[openFileIndex].inodeId = record.inodeNumber;
        openFiles[openFileIndex].openBlockId = inode.dataPtr[0];
        openFiles[openFileIndex].currentPointer = 0;

        // free(inode);
        // free(record);
    } else {

    }
    return openFileIndex;
}

int close2(FILE2 handle) {
    return 0;
}


int read2(FILE2 handle, char *buffer, int size) {
    if (initialized == FALSE) {
        initialize();
    }

    if (size < 0) {
        return ERROR_CODE;
    }
    int currentPointerOffset = openFiles[handle].currentPointer;
    int currentBlockOffset = currentPointerOffset / blockBufferSize;
    int blockCurrentPointerOffset = (currentPointerOffset % blockBufferSize);

    INODE inode;

    int numOfBytesReaded;

    getInodeById(openFiles[handle].inodeId, &inode);

    if (openFiles[handle].openBlockId == INVALID_PTR) {
        printf("[ERROR] Error while reading the file, the pointer to the block was an invalid pointer\n");
    }

    if (inode.bytesFileSize < currentPointerOffset + size) {
        size = inode.bytesFileSize - currentPointerOffset;
    }
    numOfBytesReaded = size;
    int i;

    BYTE blockBuffer[blockBufferSize];
    int bufferOffset = 0;

    while (currentPointerOffset + size > (currentBlockOffset + 1) * blockBufferSize) {
        int partSize = (currentBlockOffset + 1) * blockBufferSize - currentPointerOffset - 1;
        if (getBlock(openFiles[handle].openBlockId, blockBuffer) != SUCCESS_CODE) {
            printf("[ERROR] gettingblock %d on currentPointer: %d blockOffset: %d\n", openFiles[handle].openBlockId,
                   currentPointerOffset, currentBlockOffset);
        }
        memcpy(buffer + bufferOffset, blockBuffer + blockCurrentPointerOffset, partSize);
        bufferOffset += partSize;
        currentPointerOffset += partSize;
        size -= partSize;
        blockCurrentPointerOffset = 0;


        openFiles[handle].openBlockId = getNextBlockId(currentBlockOffset, &inode);
        if (openFiles[handle].openBlockId == INVALID_PTR) {
            printf("[ERROR] Error reading nextBlock %d %d\n", currentBlockOffset, inode.bytesFileSize);
            return -1;
        }
        currentBlockOffset++;

    }
    getBlock(openFiles[handle].openBlockId, blockBuffer);

    memcpy(buffer + bufferOffset, blockBuffer + blockCurrentPointerOffset, size);
    currentPointerOffset += size;

    openFiles[handle].currentPointer = currentPointerOffset;


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

    BYTE blockBuffer[blockBufferSize];
    int bufferOffset = 0;

    while (currentPointerOffset + size > ((currentBlockOffset + 1) * blockBufferSize)) {

        int partSize = (currentBlockOffset + 1) * blockBufferSize - currentPointerOffset;

        if (getBlock(openFiles[handle].openBlockId, blockBuffer) != SUCCESS_CODE) {
            printf("[ERROR] gettingblock %d on currentPointer: %d blockOffset: %d\n", openFiles[handle].openBlockId,
                   currentPointerOffset, currentBlockOffset);
        }
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

int truncate2(FILE2 handle) {
    if (initialized == FALSE) {
        initialize();
    }
    int currentPointerOffset = openFiles[handle].currentPointer;
    int currentBlockOffset = currentPointerOffset / blockBufferSize;

    INODE inode;// = malloc(INODE_SIZE);

    // 
    getInodeById(openFiles[handle].inodeId, &inode);
    desallocBlocksOfInode(currentBlockOffset + 1, inode.blocksFileSize, &inode);

    inode.bytesFileSize = currentPointerOffset;
    inode.blocksFileSize = currentBlockOffset + 1;

    writeInode(openFiles[handle].inodeId, &inode);
    return 0;
}

int desallocBlocksOfInode(int from, int to, INODE *inode) {

    if (from > to) {
        printf("YOU HAVE DONE SHIT WHILE PROGRAMMING\n");
        return ERROR_CODE;
    }
    int i;
    printf("\nfrom: %d to: %d\n", from, to);
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

int seek2(FILE2 handle, DWORD offset) {
    if (initialized == FALSE) {
        initialize();
    }
    INODE inode;
    getInodeById(openFiles[handle].inodeId, &inode);
    if (offset == -1) {
        openFiles[handle].currentPointer = inode.bytesFileSize;
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
    return 0;
}


int rmdir2(char *pathname) {
    return 0;
}


int chdir2(char *pathname) {
    return 0;
}


int getcwd2(char *pathname, int size) {
    return 0;
}


DIR2 opendir2(char *pathname) {
    return 0;
}

int readdir2(DIR2 handle, DIRENT2 *dentry) {
    return 0;
}

int closedir2(DIR2 handle) {
    return 0;
}