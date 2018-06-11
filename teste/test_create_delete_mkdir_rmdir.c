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
