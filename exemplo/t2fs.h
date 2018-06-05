

#ifndef __LIBT2FS___
#define __LIBT2FS___

#define	SECTOR_SIZE	256

#define TYPEVAL_INVALIDO    0x00
#define TYPEVAL_REGULAR     0x01
#define TYPEVAL_DIRETORIO   0x02

#define	INVALID_PTR	-1

typedef int FILE2;
typedef int DIR2;

typedef unsigned char BYTE;
typedef unsigned short int WORD;
typedef unsigned int DWORD;

#pragma pack(push, 1)

/** Superbloco */
struct t2fs_superbloco {
	char    id[4];          	/* Identifica��o do sistema de arquivo. � formado pelas letras �T2FS�. */
	WORD    version;        	/* Vers�o atual desse sistema de arquivos: (valor fixo 0x7E2=2018; 1=1� semestre). */
	WORD    superblockSize; 	/* O superBlock ocupa o primeiro bloco do disco, isso �, o bloco zero. */
	WORD    freeBlocksBitmapSize;	/* Quantidade de blocos usados para armazenar o bitmap de blocos de dados livres e ocupados. */
	WORD    freeInodeBitmapSize;	/* Quantidade de blocos usados para armazenar o bitmap de i-nodes livres e ocupados. */
	WORD    inodeAreaSize;		/* Quantidade de blocos usados para armazenar os i-nodes do sistema. */
	WORD    blockSize;		/* Quantidade de setores que formam um bloco l�gico. */
	DWORD   diskSize;		/* Quantidade total de blocos na partição T2FS. Inclui o superBlock, �reas de bitmap, �rea de i-node e blocos de dados */
};

/** Registro de diret�rio (entrada de diret�rio) */
struct t2fs_record {
	BYTE    TypeVal;        /* Tipo da entrada. Indica se o registro � inv�lido (TYPEVAL_INVALIDO), arquivo (TYPEVAL_REGULAR) ou diret�rio (TYPEVAL_DIRETORIO) */
	char    name[59];       /* Nome do arquivo. : string com caracteres ASCII (0x21 at� 0x7A), case sensitive. */
	DWORD   inodeNumber;    /* N�mero do i-node (se inv�lido, recebe INVALID_PTR)  */
};

/** i-node */
struct t2fs_inode {
	DWORD	blocksFileSize;	/* Tamanho do arquivo expresso em quantidade de  blocos */
	DWORD	bytesFileSize;	/* Tamanho do arquivo expresso em bytes */
	DWORD	dataPtr[2];	/* Dois ponteiros diretos (little endian). Se inv�lido, recebe INVALID_PTR.        */
	DWORD	singleIndPtr;   /* Ponteiro de indire��o simples (little endian). Se inv�lido, recebe INVALID_PTR. */
	DWORD	doubleIndPtr;   /* Ponteiro de indire��o dupla (little endian) Se inv�lido, recebe INVALID_PTR.    */
	DWORD	reservado[2];	/* Reservado */
};

/** Registro com as informa��es da entrada de diret�rio, lida com readdir2 */
#define MAX_FILE_NAME_SIZE 255
typedef struct {
    char    name[MAX_FILE_NAME_SIZE+1]; /* Nome do arquivo cuja entrada foi lida do disco      */
    BYTE    fileType;                   /* Tipo do arquivo: regular (0x01) ou diret�rio (0x02) */
    DWORD   fileSize;                   /* Numero de bytes do arquivo                          */
} DIRENT2;

#pragma pack(pop)


/*-----------------------------------------------------------------------------
Fun��o: Usada para identificar os desenvolvedores do T2FS.
	Essa fun��o copia um string de identifica��o para o ponteiro indicado por "name".
	Essa c�pia n�o pode exceder o tamanho do buffer, informado pelo par�metro "size".
	O string deve ser formado apenas por caracteres ASCII (Valores entre 0x20 e 0x7A) e terminado por �\0�.
	O string deve conter o nome e n�mero do cart�o dos participantes do grupo.

Entra:	name -> buffer onde colocar o string de identifica��o.
	size -> tamanho do buffer "name" (n�mero m�ximo de bytes a serem copiados).

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size);


/*-----------------------------------------------------------------------------
Fun��o: Criar um novo arquivo.
	O nome desse novo arquivo � aquele informado pelo par�metro "filename".
	O contador de posi��o do arquivo (current pointer) deve ser colocado na posi��o zero.
	Caso j� exista um arquivo ou diret�rio com o mesmo nome, a fun��o dever� retornar um erro de cria��o.
	A fun��o deve retornar o identificador (handle) do arquivo.
	Esse handle ser� usado em chamadas posteriores do sistema de arquivo para fins de manipula��o do arquivo criado.

Entra:	filename -> nome do arquivo a ser criado.

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna o handle do arquivo (n�mero positivo).
	Em caso de erro, deve ser retornado um valor negativo.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename);


/*-----------------------------------------------------------------------------
Fun��o:	Apagar um arquivo do disco.
	O nome do arquivo a ser apagado � aquele informado pelo par�metro "filename".

Entra:	filename -> nome do arquivo a ser apagado.

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int delete2 (char *filename);


/*-----------------------------------------------------------------------------
Fun��o:	Abre um arquivo existente no disco.
	O nome desse novo arquivo � aquele informado pelo par�metro "filename".
	Ao abrir um arquivo, o contador de posi��o do arquivo (current pointer) deve ser colocado na posi��o zero.
	A fun��o deve retornar o identificador (handle) do arquivo.
	Esse handle ser� usado em chamadas posteriores do sistema de arquivo para fins de manipula��o do arquivo criado.
	Todos os arquivos abertos por esta chamada s�o abertos em leitura e em escrita.
	O ponto em que a leitura, ou escrita, ser� realizada � fornecido pelo valor current_pointer (ver fun��o seek2).

Entra:	filename -> nome do arquivo a ser apagado.

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna o handle do arquivo (n�mero positivo)
	Em caso de erro, deve ser retornado um valor negativo
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename);


/*-----------------------------------------------------------------------------
Fun��o:	Fecha o arquivo identificado pelo par�metro "handle".

Entra:	handle -> identificador do arquivo a ser fechado

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle);


/*-----------------------------------------------------------------------------
Fun��o:	Realiza a leitura de "size" bytes do arquivo identificado por "handle".
	Os bytes lidos s�o colocados na �rea apontada por "buffer".
	Ap�s a leitura, o contador de posi��o (current pointer) deve ser ajustado para o byte seguinte ao �ltimo lido.

Entra:	handle -> identificador do arquivo a ser lido
	buffer -> buffer onde colocar os bytes lidos do arquivo
	size -> n�mero de bytes a serem lidos

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna o n�mero de bytes lidos.
	Se o valor retornado for menor do que "size", ent�o o contador de posi��o atingiu o final do arquivo.
	Em caso de erro, ser� retornado um valor negativo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size);


/*-----------------------------------------------------------------------------
Fun��o:	Realiza a escrita de "size" bytes no arquivo identificado por "handle".
	Os bytes a serem escritos est�o na �rea apontada por "buffer".
	Ap�s a escrita, o contador de posi��o (current pointer) deve ser ajustado para o byte seguinte ao �ltimo escrito.

Entra:	handle -> identificador do arquivo a ser escrito
	buffer -> buffer de onde pegar os bytes a serem escritos no arquivo
	size -> n�mero de bytes a serem escritos

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna o n�mero de bytes efetivamente escritos.
	Em caso de erro, ser� retornado um valor negativo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size);


/*-----------------------------------------------------------------------------
Fun��o:	Fun��o usada para truncar um arquivo.
	Remove do arquivo todos os bytes a partir da posi��o atual do contador de posi��o (CP)
	Todos os bytes a partir da posi��o CP (inclusive) ser�o removidos do arquivo.
	Ap�s a opera��o, o arquivo dever� contar com CP bytes e o ponteiro estar� no final do arquivo

Entra:	handle -> identificador do arquivo a ser truncado

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int truncate2 (FILE2 handle);


/*-----------------------------------------------------------------------------
Fun��o:	Reposiciona o contador de posi��es (current pointer) do arquivo identificado por "handle".
	A nova posi��o � determinada pelo par�metro "offset".
	O par�metro "offset" corresponde ao deslocamento, em bytes, contados a partir do in�cio do arquivo.
	Se o valor de "offset" for "-1", o current_pointer dever� ser posicionado no byte seguinte ao final do arquivo,
		Isso � �til para permitir que novos dados sejam adicionados no final de um arquivo j� existente.

Entra:	handle -> identificador do arquivo a ser escrito
	offset -> deslocamento, em bytes, onde posicionar o "current pointer".

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int seek2 (FILE2 handle, DWORD offset);


/*-----------------------------------------------------------------------------
Fun��o:	Criar um novo diret�rio.
	O caminho desse novo diret�rio � aquele informado pelo par�metro "pathname".
		O caminho pode ser ser absoluto ou relativo.
	S�o considerados erros de cria��o quaisquer situa��es em que o diret�rio n�o possa ser criado.
		Isso inclui a exist�ncia de um arquivo ou diret�rio com o mesmo "pathname".

Entra:	pathname -> caminho do diret�rio a ser criado

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname);


/*-----------------------------------------------------------------------------
Fun��o:	Apagar um subdiret�rio do disco.
	O caminho do diret�rio a ser apagado � aquele informado pelo par�metro "pathname".
	S�o considerados erros quaisquer situa��es que impe�am a opera��o.
		Isso inclui:
			(a) o diret�rio a ser removido n�o est� vazio;
			(b) "pathname" n�o existente;
			(c) algum dos componentes do "pathname" n�o existe (caminho inv�lido);
			(d) o "pathname" indicado n�o � um arquivo;

Entra:	pathname -> caminho do diret�rio a ser criado

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int rmdir2 (char *pathname);


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
int chdir2 (char *pathname);


/*-----------------------------------------------------------------------------
Fun��o:	Informa o diret�rio atual de trabalho.
		O "pathname" do diret�rio de trabalho deve ser copiado para o buffer indicado por "pathname".
			Essa c�pia n�o pode exceder o tamanho do buffer, informado pelo par�metro "size".
		S�o considerados erros:
			(a) quaisquer situa��es que impe�am a realiza��o da opera��o
			(b) espa�o insuficiente no buffer "pathname", cujo tamanho est� informado por "size".

Entra:	pathname -> buffer para onde copiar o pathname do diret�rio de trabalho
		size -> tamanho do buffer pathname

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
		Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int getcwd2 (char *pathname, int size);


/*-----------------------------------------------------------------------------
Fun��o:	Abre um diret�rio existente no disco.
	O caminho desse diret�rio � aquele informado pelo par�metro "pathname".
	Se a opera��o foi realizada com sucesso, a fun��o:
		(a) deve retornar o identificador (handle) do diret�rio
		(b) deve posicionar o ponteiro de entradas (current entry) na primeira posi��o v�lida do diret�rio "pathname".
	O handle retornado ser� usado em chamadas posteriores do sistema de arquivo para fins de manipula��o do diret�rio.

Entra:	pathname -> caminho do diret�rio a ser aberto

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna o identificador do diret�rio (handle).
	Em caso de erro, ser� retornado um valor negativo.
-----------------------------------------------------------------------------*/
DIR2 opendir2 (char *pathname);


/*-----------------------------------------------------------------------------
Fun��o:	Realiza a leitura das entradas do diret�rio identificado por "handle".
	A cada chamada da fun��o � lida a entrada seguinte do diret�rio representado pelo identificador "handle".
	Algumas das informa��es dessas entradas devem ser colocadas no par�metro "dentry".
	Ap�s realizada a leitura de uma entrada, o ponteiro de entradas (current entry) deve ser ajustado para a pr�xima entrada v�lida, seguinte � �ltima lida.
	S�o considerados erros:
		(a) qualquer situa��o que impe�a a realiza��o da opera��o
		(b) t�rmino das entradas v�lidas do diret�rio identificado por "handle".

Entra:	handle -> identificador do diret�rio cujas entradas deseja-se ler.
	dentry -> estrutura de dados onde a fun��o coloca as informa��es da entrada lida.

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero ( e "dentry" n�o ser� v�lido)
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry);


/*-----------------------------------------------------------------------------
Fun��o:	Fecha o diret�rio identificado pelo par�metro "handle".

Entra:	handle -> identificador do diret�rio que se deseja fechar (encerrar a opera��o).

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle);


#endif
