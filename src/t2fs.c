#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/t2fs.h"
#include "../include/bitmap2.h"
#include "../include/apidisk.h"

#define TRUE  1
#define FALSE 0
#define MAXFILES 20
#define FILENAMESIZE 32
#define SECTORSIZE 256
#define INODESPERSECTOR 16
#define RECORDSPERSECTOR 4
#define PTRPERSECTOR 64
#define INTSIZE 4
#define ROOTINODE 0
#define RECORDSIZE 64
#define INODESIZE 16

int isFirst = TRUE;

typedef struct t2fs_fileRecord{
	struct t2fs_record record;
	int currentPointer;
	int inodePtr;
}OPENEDFILE;

OPENEDFILE openedFiles[MAXFILES];

//superbloco
struct t2fs_superbloco superblock;

int inodeStart;
int dataStart;

// converte uma parte do buffer a partir do índice start para WORD
WORD strToWord(unsigned char *buffer, int start)
{
	unsigned char word[2];
	
	word[0] = buffer[start];
	word[1] = buffer[start+1];

	return (*((WORD *)word));
}

// converte uma parte do buffer a partir do índice start para DWORD
DWORD strToDword(unsigned char *buffer, int start)
{
	unsigned char dword[4];

	dword[0] = buffer[start];
	dword[1] = buffer[start+1];
	dword[2] = buffer[start+2];
	dword[3] = buffer[start+3];

	return (*((DWORD *)dword));
}

// converte uma parte do buffer a partir do índice start para BYTE
BYTE strToByte(unsigned char *buffer, int start)
{
	BYTE byte[1];

	byte[0] = buffer[start];

	return (*((BYTE *)byte));
}

// converte uma parte do buffer a partir do índice start para inteiro
int strToInt(unsigned char *buffer, int start)
{
	unsigned char integer[4];
	
	integer[0] = buffer[start];
	integer[1] = buffer[start+1];
	integer[2] = buffer[start+2];
	integer[3] = buffer[start+3];

	return (*((int *)integer));
}

// converte um inteiro em bytes
void intToStr(unsigned char *buffer, int start, int integer)
{	
	unsigned char *aux;
	aux = (unsigned char*)&integer;
	
	buffer[start] 	= aux[0];
	buffer[start+1] = aux[1];
	buffer[start+2] = aux[2];
	buffer[start+3] = aux[3];
}


// inicializa o array de arquivos abertos, colocando todos em ínvalidos
void initializeOpenedFiles()
{
	int i;
	for (i=0; i<MAXFILES; i++)
	{		
		openedFiles[i].record.TypeVal = TYPEVAL_INVALIDO;
	}

}

// realiza a leitura do superbloco no disco, decodificando a informação e pondo em uma variável global
int readSuperBlock()
{
	int error;
	int i;
	unsigned char buffer[SECTORSIZE];
	
	// leitura do setor zero, setor onde está localizado o superbloco	
	error = read_sector(0, buffer);

	for (i = 0; i<4; i++)
	{
		superblock.id[i] = buffer[i];
	}				
	
	// Lê WORDS
	superblock.version = strToWord(buffer, 4);
	superblock.superblockSize = strToWord(buffer, 6);
	superblock.freeBlocksBitmapSize = strToWord(buffer, 8);
	superblock.freeInodeBitmapSize = strToWord(buffer, 10);
	superblock.inodeAreaSize = strToWord(buffer, 12);
	superblock.blockSize = strToWord(buffer, 14);

	// Lê DWORD
	superblock.diskSize = strToDword(buffer, 16);

	// TESTE
	/* puts(superblock.id);
	printf("%d\n", superblock.version);
	printf("%d\n", superblock.superblockSize);
	printf("%d\n", superblock.freeBlocksBitmapSize);
	printf("%d\n", superblock.freeInodeBitmapSize);
	printf("%d\n", superblock.inodeAreaSize);
	printf("%d\n", superblock.blockSize);
	printf("%d\n", superblock.diskSize); */

	return error;
}

//retorna 0 se a função foi executada corretamente
int firstTime()
{	
	int error = 0;

	// se for a primeira vez a ser executada
	if (isFirst == TRUE)
	{	
		isFirst = FALSE;		
	
		// lê superbloco
		error = readSuperBlock();
		
		// setor onde inicia a área de inodes
		inodeStart= superblock.superblockSize + superblock.freeInodeBitmapSize + superblock.freeBlocksBitmapSize;
	
		// setor onde inicia a área de dados
		dataStart = superblock.superblockSize + superblock.freeInodeBitmapSize + superblock.freeBlocksBitmapSize +
		superblock.inodeAreaSize;
	
		// inicializa o array de arquivos abertos
		initializeOpenedFiles();
	}

	return error;
}

int identify2 (char *name, int size)
{
	char* group = "Daniel Maia - 243672\nDenyson Grellert - 243676\n Rodrigo Felipe - 242289";

	if(firstTime())
		return -1;

	if (size < 0)
		return -1;

	if (name == NULL)
		return -2;

	else
		strncpy(name, group, size);

	return 0;
} 

// abre um determinado arquivo e retorna seu handle
FILE2 openFile(struct t2fs_record record, int inodePtr)
{
	int i;

	// percorre a lista de arquivos abertos para encontrar um inválido
	for (i = 0; i<MAXFILES; i++)
	{
		if (openedFiles[i].record.TypeVal == TYPEVAL_INVALIDO)	
		{
			openedFiles[i].record = record;				
			openedFiles[i].currentPointer = 0;
			openedFiles[i].inodePtr = inodePtr; 
			return i;
		}
	}

	// erro: não temos espaço para abrir arquivo
	return -1;
}

// realiza a leitura de 4*blockSize entradas de diretórios(1 setor 4 registros)
int readDirectoryFromDisk(int inodePtr, struct t2fs_record *record)
{
	int i;
	int j = 0;
	int k;
	int error;	
	unsigned int sector;
	unsigned char buffer[SECTORSIZE];

	//printf("\n%d", inodePtr);

	for (k=0; k<superblock.blockSize; k++)
	{	
		sector = dataStart + inodePtr*superblock.blockSize + k;

		//printf("%d %d\n", inodePtr, sector);

		error = read_sector(sector, buffer);

		if(!error)
		{		
			for (j=0; j<(RECORDSPERSECTOR); j++)
			{			
				record[j + k*RECORDSPERSECTOR].TypeVal = buffer[j*RECORDSIZE];
	
				for (i = 0; i < FILENAMESIZE; i++)
				{
					record[j + k*RECORDSPERSECTOR].name[i] = buffer[i + 1 + j*RECORDSIZE];	
				}

				//puts(record[j].name);

				record[j + k*RECORDSPERSECTOR].blocksFileSize = strToDword(buffer, 33 +j*RECORDSIZE);
				record[j + k*RECORDSPERSECTOR].bytesFileSize = strToDword(buffer, 37 +j*RECORDSIZE);
				record[j + k*RECORDSPERSECTOR].inodeNumber = strToInt(buffer, 41 +j*RECORDSIZE);
			}
		}
	}
	return error;
}

// através de um inodeNumber retorna uma estrutura de inode lida do disco
struct t2fs_inode getInode(int inodeNumber)
{
	int offset;
	unsigned int sector;
	unsigned char buffer[SECTORSIZE];
	struct t2fs_inode inode;

	sector = inodeStart + inodeNumber / INODESPERSECTOR;

	read_sector(sector, buffer);

	offset = (inodeNumber % INODESPERSECTOR)*INODESIZE;
	
	inode.dataPtr[0] = strToInt(buffer, 0 + offset);
	inode.dataPtr[1] = strToInt(buffer, 4 + offset);
	inode.singleIndPtr = strToInt(buffer, 8 + offset);
	inode.doubleIndPtr = strToInt(buffer, 12 + offset);	

	//printf("%d %d %d %d \n", inode.dataPtr[0], inode.dataPtr[1], inode.singleIndPtr, inode.doubleIndPtr);

	return inode;
}

// procura um diretório a partir do seu nome, retorna TRUE se o diretório foi encontrado e FALSE caso contrário
int findDirectory(struct t2fs_inode inode, char *directory, struct t2fs_record *record, int *inodePtr)
{
	int i;
	struct t2fs_record recordAux[RECORDSPERSECTOR*superblock.blockSize];
	
	// se não for uma referência inválida
	if (inode.dataPtr[0] != INVALID_PTR)
	{		
		// lê 4*superblock.blockSize entradas de diretórios do disco
		if (!readDirectoryFromDisk(inode.dataPtr[0], recordAux))	
		{			
			for (i=0; i<RECORDSPERSECTOR*superblock.blockSize; i++)
			{	
				// se é um arquivo de diretório e tem o mesmo nome esperado
				if ((recordAux[i].TypeVal == TYPEVAL_DIRETORIO) && (!strcmp(directory, recordAux[i].name)))			
				{					
					*inodePtr = inode.dataPtr[0];
					*record = recordAux[i];
					return TRUE;
				}
			}
		}
	}
	
	// se não for uma referência inválida
	if (inode.dataPtr[1] != INVALID_PTR)
	{
		// lê 4 entradas de diretórios do disco
		if(!readDirectoryFromDisk(inode.dataPtr[1], recordAux))			
		{
			for(i=0; i<RECORDSPERSECTOR*superblock.blockSize; i++)
			{
				// se é um arquivo de diretório e tem o mesmo nome esperado
				if ((recordAux[i].TypeVal == TYPEVAL_DIRETORIO) && (!strcmp(directory, recordAux[i].name)))			
				{
					*inodePtr = inode.dataPtr[1];
					*record = recordAux[i];
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

// procura um arquivo a partir do seu nome, retorna TRUE se o diretório foi encontrado e FALSE caso contrário
int findFile(struct t2fs_inode inode, char *filnename, struct t2fs_record *record, int *inodePtr)
{
	int i;
	struct t2fs_record recordAux[RECORDSPERSECTOR*superblock.blockSize];
	
	// se não for uma referência inválida
	if (inode.dataPtr[0] != INVALID_PTR)
	{		
		// lê 4*superblock.blockSize entradas de diretórios do disco
		if (!readDirectoryFromDisk(inode.dataPtr[0], recordAux))	
		{			
			for (i=0; i<RECORDSPERSECTOR*superblock.blockSize; i++)
			{	
				// se é um arquivo de diretório e tem o mesmo nome esperado
				if ((recordAux[i].TypeVal == TYPEVAL_REGULAR) && (!strcmp(filnename, recordAux[i].name)))			
				{					
					*inodePtr = inode.dataPtr[0];
					*record = recordAux[i];
					return TRUE;
				}
			}
		}
	}
	
	// se não for uma referência inválida
	if (inode.dataPtr[1] != INVALID_PTR)
	{
		// lê 4 entradas de diretórios do disco
		if(!readDirectoryFromDisk(inode.dataPtr[1], recordAux))			
		{
			for(i=0; i<RECORDSPERSECTOR*superblock.blockSize; i++)
			{
				// se é um arquivo de diretório e tem o mesmo nome esperado
				if ((recordAux[i].TypeVal == TYPEVAL_REGULAR) && (!strcmp(filnename, recordAux[i].name)))			
				{
					*inodePtr = inode.dataPtr[1];
					*record = recordAux[i];
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

// escreve uma entrada de diretório no disco
int writeRecordDisk(struct t2fs_record record, int inodePtr, int offset)
{
	int error;
	int i;
	unsigned int sector;
	unsigned char buffer[SECTORSIZE];	

	//printf("inodePtr : %d offset: %d", inodePtr, offset);

	sector = dataStart + inodePtr * superblock.blockSize + (offset*RECORDSIZE) / (SECTORSIZE);

	//printf("\n%d", sector);

	error = read_sector(sector, buffer);

	offset = (offset) % RECORDSPERSECTOR;

	buffer[offset*RECORDSIZE] = record.TypeVal;

	for(i=0; i<FILENAMESIZE; i++)
	{
		buffer[i+1+offset*RECORDSIZE] = record.name[i];			
	}

	intToStr(buffer, offset*RECORDSIZE+33, record.blocksFileSize);	
	intToStr(buffer, offset*RECORDSIZE+37, record.bytesFileSize);
	intToStr(buffer, offset*RECORDSIZE+41, record.inodeNumber);

	if (!error)
		error = write_sector(sector, buffer);	
	
	return error;
}

// retorna os ponteiros a partir de um ponteiro de inode de indireção simples
int getSingleIndInode(int singleIndPtr, int *dataPtr)
{
	int i;
	int j;
	unsigned int sector;
	unsigned char buffer[SECTORSIZE];

	// calcula setor onde iniciam os ponteiros
	sector = dataStart + singleIndPtr * superblock.blockSize;

	// percorre o bloco lógico
	for (i=0; i<superblock.blockSize; i++)
	{
		if(!read_sector(sector, buffer))
		{
			// salva em um array os ponteiros lidos do disco
			for(j=0; j<PTRPERSECTOR; j++)
			{
				dataPtr[j] = strToInt(buffer, j*INTSIZE);
			}
		}
		// erro
		else
			return 1;
	}
	// sucesso
	return 0;
}

// deleta um arquivo do disco
int deleteFileFromDisk(int inodeNumber)
{
	int i;
	struct t2fs_inode inode;
	int dataPtr1[PTRPERSECTOR * superblock.blockSize];

	inode = getInode(inodeNumber);

	// ponteiro simples
	for (i = 0; i<2; i++)
	{
		if(inode.dataPtr[i] != INVALID_PTR)
		{
			return setBitmap2(BITMAP_DADOS, inode.dataPtr[i], 0);
		}
	}
	// ponteiro de indireção simples
	if (inode.singleIndPtr != INVALID_PTR)
	{
		// retorna a lista com os inodes de indereção simples
		getSingleIndInode(inode.singleIndPtr, dataPtr1);

		// percorre os inode presentes na área de dados
		for(i=0; i<PTRPERSECTOR*superblock.blockSize; i++)
		{
			// libera a área de dados
			if(setBitmap2(BITMAP_DADOS, dataPtr1[i], 0))
				return 1;
		}
		// libera a área de inode presente na área de dados
		return setBitmap2(BITMAP_DADOS, inode.singleIndPtr, 0);
	}
	return 1;
}

// deleta um inode do disco
int deleteInodeFromDisk(int inodeNumber)
{
	return setBitmap2(BITMAP_INODE, inodeNumber, 0);
}

// função que procura um arquivo e o deleta
int deleteFile(struct t2fs_inode inode, char *filename)
{
	int i;
	int j;
	int dataPtr1[PTRPERSECTOR*superblock.blockSize];
	struct t2fs_record recordAux[RECORDSPERSECTOR*superblock.blockSize];

	// se não for uma referência inválida
	if (inode.dataPtr[0] != INVALID_PTR)
	{		
		// lê 4*superblock.blockSize entradas de diretórios do disco
		readDirectoryFromDisk(inode.dataPtr[0], recordAux);					

		for (i=0; i<RECORDSPERSECTOR*superblock.blockSize; i++)
		{	
			// se é um arquivo regular e tem o mesmo nome esperado
			if ((recordAux[i].TypeVal == TYPEVAL_REGULAR) && (!strcmp(filename, recordAux[i].name)))			
			{					
				if (!(deleteFileFromDisk(recordAux[i].inodeNumber) && !(deleteInodeFromDisk(recordAux[i].inodeNumber))))
				{	
					recordAux[i].TypeVal = TYPEVAL_INVALIDO;
					writeRecordDisk(recordAux[i], inode.dataPtr[0], i);
					return TRUE;
				}
				
				else 
					return FALSE;
			}
		}
	}
	
	// se não for uma referência inválida
	if (inode.dataPtr[1] != INVALID_PTR)
	{
		// lê 4 entradas de diretórios do disco
		readDirectoryFromDisk(inode.dataPtr[1], recordAux);			
		
		for(i=0; i<RECORDSPERSECTOR*superblock.blockSize; i++)
		{
			// se é um arquivo regular e tem o mesmo nome esperado
			if ((recordAux[i].TypeVal == TYPEVAL_REGULAR) && (!strcmp(filename, recordAux[i].name)))			
			{
				if (!(deleteFileFromDisk(recordAux[i].inodeNumber) && !(deleteInodeFromDisk(recordAux[i].inodeNumber))))
				{	
					recordAux[i].TypeVal = TYPEVAL_INVALIDO;
					writeRecordDisk(recordAux[i], inode.dataPtr[1], i);
					return TRUE;
				}
				else 
					return FALSE;
			}
		}
	}

	if (inode.singleIndPtr != INVALID_PTR)
	{
		// leitura dos ponteiros a partir do ponteiro de indireção simples
		getSingleIndInode(inode.singleIndPtr, dataPtr1);
		
		for (i=0; i<superblock.blockSize*PTRPERSECTOR; i++)
		{
			readDirectoryFromDisk(dataPtr1[i], recordAux);

			for(j=0; j<RECORDSPERSECTOR*superblock.blockSize; j++)
			{
				// se é um arquivo regular e tem o mesmo nome esperado
				if ((recordAux[j].TypeVal == TYPEVAL_REGULAR) && (!strcmp(filename, recordAux[j].name)))			
				{
					if (!(deleteFileFromDisk(recordAux[j].inodeNumber) && !(deleteInodeFromDisk(recordAux[j].inodeNumber))))
					{	
						recordAux[i].TypeVal = TYPEVAL_INVALIDO;
						writeRecordDisk(recordAux[j], dataPtr1[j], j);
						return TRUE;
					}
					else 
						return FALSE;
				}
			}
		}
	}

	// não encontrou
	return FALSE;
}

int isEmptyDirectory(int inodeNumber)
{
	return TRUE;
}

// função que procura um arquivo de diretório e o deleta
int deleteDirectory(struct t2fs_inode inode, char *directory)
{
	int i;
	int j;
	int dataPtr1[PTRPERSECTOR*superblock.blockSize];
	struct t2fs_record recordAux[RECORDSPERSECTOR*superblock.blockSize];

	// se não for uma referência inválida
	if (inode.dataPtr[0] != INVALID_PTR)
	{		
		// lê 4*superblock.blockSize entradas de diretórios do disco
		readDirectoryFromDisk(inode.dataPtr[0], recordAux);					

		for (i=0; i<RECORDSPERSECTOR*superblock.blockSize; i++)
		{	
			// se é um arquivo de diretório e tem o mesmo nome esperado e 
			if ((recordAux[i].TypeVal == TYPEVAL_DIRETORIO) && (!strcmp(directory, recordAux[i].name)) && (isEmptyDirectory(recordAux[i].inodeNumber)))			
			{					
				deleteFileFromDisk(recordAux[i].inodeNumber);
				return TRUE;
			}
		}
	}
	
	// se não for uma referência inválida
	if (inode.dataPtr[1] != INVALID_PTR)
	{
		// lê 4 entradas de diretórios do disco
		readDirectoryFromDisk(inode.dataPtr[1], recordAux);			
		
		for(i=0; i<RECORDSPERSECTOR*superblock.blockSize; i++)
		{
			// se é um arquivo de diretório e tem o mesmo nome esperado
			if ((recordAux[i].TypeVal == TYPEVAL_DIRETORIO) && (!strcmp(directory, recordAux[i].name)) &&(isEmptyDirectory(recordAux[i].inodeNumber)))			
			{
				deleteFileFromDisk(recordAux[i].inodeNumber);
				return TRUE;
			}
		}
	}

	if (inode.singleIndPtr != INVALID_PTR)
	{
		// leitura dos ponteiros a partir do ponteiro de indireção simples
		getSingleIndInode(inode.singleIndPtr, dataPtr1);
		
		for (i=0; i<superblock.blockSize*PTRPERSECTOR; i++)
		{
			readDirectoryFromDisk(dataPtr1[i], recordAux);

			for(j=0; j<RECORDSPERSECTOR*superblock.blockSize; j++)
			{
				// se é um arquivo regular e tem o mesmo nome esperado
				if ((recordAux[j].TypeVal == TYPEVAL_DIRETORIO) && (!strcmp(directory, recordAux[j].name)))			
				{
					if (!(deleteFileFromDisk(recordAux[j].inodeNumber) && !(deleteInodeFromDisk(recordAux[j].inodeNumber))))
						return TRUE;
					else 
						return FALSE;
				}
			}
		}
	}
	return FALSE;
}

// função que retorna TRUE caso o pathname exista e FALSE caso contrário. retorna também a entrada de diretório do path
int findPath(char *pathname, struct t2fs_record *record)
{	
	int inodePtr;
	char *directory;	
	char *dirAux;
	struct t2fs_inode inode;	

	// alocar uma variável local com o tamanho da string pathname	
	dirAux = malloc(strlen(pathname) + 1); 
    
    // essa cópia é feita pois a função strtok altera a string passada como parâmetro
    strcpy(dirAux, pathname);

	// se o primeiro caractere é igual ao diretório raíz
	if (pathname[0] == '/')	
		{
			// inode do diretório raiz	
			inode = getInode(ROOTINODE);
			
			// GAMBIARRA: posiciona o ponteiro da string na posição número 1 do vetor
			// se achar uma maneira melhor favor mudar
			dirAux = &dirAux[1];

			dirAux = strtok(dirAux, "/");
		}
	else 
		// erro
		return 1;	

	directory = dirAux;

	while(directory != NULL)
	{
		dirAux = strtok(NULL, "/");
		
		if (dirAux != NULL)	
		{
			// se encontrou um diretório com o nome esperado
			if (findDirectory(inode, directory, record, &inodePtr))						
				inode = getInode(record->inodeNumber);			

			// se não encontrou retorna FALSE
			else 
				return FALSE;
		}

		directory = dirAux;
	}

	free(dirAux);

	return TRUE;	

}

// escreve um inode no disco
int writeInodeDisk(struct t2fs_inode inode, int inodeNumber)
{
	unsigned int sector;
	int offset;
	int error;
	unsigned char buffer[SECTORSIZE];

	sector = inodeNumber/INODESPERSECTOR + inodeStart;

	error = read_sector(sector, buffer);
	
	if (!error)
	{
		// calcula offset a partir do inodeNumber
		offset = inodeNumber%INODESPERSECTOR;
	
		// converte o inode para bytes
		intToStr(buffer, INODESIZE*offset, inode.dataPtr[0]);
		intToStr(buffer, INODESIZE*offset+4, inode.dataPtr[1]);
		intToStr(buffer, INODESIZE*offset+8, inode.singleIndPtr);
		intToStr(buffer, INODESIZE*offset+12, inode.doubleIndPtr);

		// escreve inode no disco
		error = write_sector(sector, buffer);
	}

	return error;
}

// função que extrai o nome do arquivo a partir de um pathname
void getFilename(char *pathname, char *filename)
{
	char *filenameAux; 

	filenameAux = strtok(pathname, "/");

	strcpy(filename, filenameAux);

	while(filenameAux != NULL)
	{
		strcpy(filename, filenameAux);
	
		filenameAux = strtok(NULL, "/");
	}
}

// devolve nome do diretório pai (caso esteja em um sub, ou do diretório caso seja um arquivo) em dir e retorna o path até o diretório 
char* getFatherDir(char *pathname, char *dir)
{
	char *dirAux = NULL;
	char dirAuxF[32];
	char *pathDir;

	pathDir = malloc(sizeof(char)*strlen(pathname));

	dirAux = strtok(pathname, "/");

	while(dirAux != NULL)
	{
		strcpy(dir, dirAuxF);
		strcpy(dirAuxF, dirAux);

		if (dir != NULL)
		{
			strcat(pathDir, "/");
			strcat(pathDir, dir);
		}

		dirAux = strtok(NULL, "/");
	}

	return pathDir;

}

// função que analisa a string do nome do arquivo e retorna TRUE caso ela seja válido e FALSE caso contrário
int validFilename(char *filename)
{
	if (strlen(filename) <= 32)
		return TRUE;
	else
		return FALSE;
} 

//  inicializa um bloco de entradasde diretório
void initializeRecordBlock(int inodePtr)
{
	int i;
	unsigned int sector;
	unsigned char buffer[SECTORSIZE];	

	sector = dataStart + inodePtr * superblock.blockSize;

	for(i=0; i<superblock.blockSize; i++)
	{
		buffer[0*RECORDSIZE] = TYPEVAL_INVALIDO;
		buffer[1*RECORDSIZE] = TYPEVAL_INVALIDO;
		buffer[2*RECORDSIZE] = TYPEVAL_INVALIDO;
		buffer[3*RECORDSIZE] = TYPEVAL_INVALIDO;

		write_sector(sector + i, buffer);
	}
}

// inicializa um bloco de ponteiros 
void initializeSingleIndBlock(int singleIndPtr)
{
	int i;
	int j;
	unsigned int sector;
	unsigned char buffer[SECTORSIZE];

	sector = dataStart + singleIndPtr * superblock.blockSize;

	for(i=0; i<superblock.blockSize; i++)
	{
		for (j=0; j<PTRPERSECTOR; j++)
			intToStr(buffer, j*INTSIZE, INVALID_PTR);
	
		write_sector(sector + i, buffer);
	}
}

// escreve um ponteiro na área de ponteiros de indereção simples
int writeSingleIndPtr(int singleIndPtr, int dataNumber)
{
	int i;
	int j;
	int ptr;
	unsigned int sector;
	unsigned char buffer[SECTORSIZE];

	sector = dataStart + singleIndPtr * superblock.blockSize;

	for(i = 0; i<superblock.blockSize; i++)
	{
		if(read_sector(sector, buffer))
		{
			for(j=0; j<PTRPERSECTOR; j++)
			{
				ptr = strToInt(buffer, j*INTSIZE);

				// se for uma entrada inválida utilizaremos
				if(ptr == INVALID_PTR)
				{
					intToStr(buffer, j*INTSIZE, dataNumber);
					
					return write_sector(sector+i, buffer);
				}
			}
		}
	}
	return -1;
}

int updateRecord(int inodePtr, struct t2fs_record record)
{
	int i;
	struct t2fs_record recordAux[RECORDSPERSECTOR*superblock.blockSize];

	if(!readDirectoryFromDisk(inodePtr, recordAux))
	{
		for(i=0; i<RECORDSPERSECTOR*superblock.blockSize; i++)
		{
			if((recordAux[i].TypeVal == record.TypeVal) && (!strcmp(recordAux[i].name, record.name)))
				return writeRecordDisk(record, inodePtr, i);
		}
	}
	return -1;
}

// cria uma nova entrada de diretório no disco
int createRecord(int inodeNumber, struct t2fs_record newRecord, int *inodePtr)
{
	int dataNumber;
	int error;	
	int i;
	int j;
	struct t2fs_inode inode;
	struct t2fs_record recordAux[RECORDSPERSECTOR*superblock.blockSize];

	inode = getInode(inodeNumber);

	for (j=0; j<2; j++)
	{
		// se o ponteiro for inválido 	
		if (inode.dataPtr[j] == INVALID_PTR)
		{
			// procura uma área de dados disponível para a entrada de diretório
			dataNumber = searchBitmap2(BITMAP_DADOS, 0);

			// seta para indisponível essa área de dados
			error = setBitmap2(BITMAP_DADOS, dataNumber, 1);

			// ponteiro do diretório pai apontando para essa nova entrada de diretório
			inode.dataPtr[j] = dataNumber;

			// escreve o inode atualizado no disco
			if (!error)
				error = writeInodeDisk(inode, inodeNumber);
	
			if (!error)		
			{
				*inodePtr = inode.dataPtr[j];

				// escreve o novo registro no disco	
				initializeRecordBlock(inode.dataPtr[j]);		
				error = writeRecordDisk(newRecord, inode.dataPtr[j], 0);
				return error;
			}
	
		}
		else  
		{	
			// le 4 diretórios do disco
			error = readDirectoryFromDisk(inode.dataPtr[j], recordAux);

			if(!error)
			{
				// procuramos se alguma entrada é inválida
				for(i=0; i<(RECORDSPERSECTOR*superblock.blockSize); i++)
				{
					// se encontrarmos uma entrada inválida podemos utilizá-la
					if(recordAux[i].TypeVal == TYPEVAL_INVALIDO)	
					{
						*inodePtr = inode.dataPtr[j];
						
						recordAux[i] = newRecord;
										
						return writeRecordDisk(newRecord, inode.dataPtr[j], i);	
					}
				}
			}
		}
	}

	if(inode.singleIndPtr == INVALID_PTR)
	{
		// procura uma área de dados disponível para a área de inodes no disco
		dataNumber = searchBitmap2(BITMAP_DADOS, 0);

		// seta para indisponível essa área de dados
		error = setBitmap2(BITMAP_DADOS, dataNumber, 1);

		// ponteiro do diretório pai apontando para essa nova entrada de diretório
		inode.singleIndPtr = dataNumber;

		// escreve o inode atualizado no disco
		if(!error)
			error = writeInodeDisk(inode, inodeNumber);

		if (!error)		
		{
			// procura uma área de dados disponível para a área de diretórios
			dataNumber = searchBitmap2(BITMAP_DADOS, 0);

			// seta para indisponível essa área de dados
			error = setBitmap2(BITMAP_DADOS, dataNumber, 1);

			if(!error)
			{
				initializeSingleIndBlock(inode.singleIndPtr);
				error = writeSingleIndPtr(inode.singleIndPtr, dataNumber);
			}
			if (!error)
			{
				*inodePtr = dataNumber;

				// inicializa bloco de dados
				initializeRecordBlock(inode.singleIndPtr);
				// escreve entrada de diretório no disco
				error = writeRecordDisk(newRecord, dataNumber, 0);
				return error;
			}
		}
	}

	// erro
	return -1;
}

// função que verifica se um diretório é o raiz
int isRootPath(char *pathname)
{
	char *path;
	char *pStart;	
	
	if (pathname[0] == '/')
	{
		// alocar uma variável local com o tamanho da string pathname	
		path = malloc(strlen(pathname) + 1); 
	    
		strcpy(path, pathname);

		// salva um ponteiro para o início da string
		pStart = path;

		path = strtok(path, "/");
		path = strtok(NULL, "/");


		if (path == NULL)
		{
			free(pStart);
			return TRUE;
		}
		else
		{ 
			free(pStart);
			return FALSE;
		}
	}

	else 
		return FALSE;
}

int isValidHandle(int handle)
{
	if ((handle >= 0) && (handle < 20))
		return TRUE;
	else 
		return FALSE;

}

FILE2 create2 (char *filename)
{
	int isRoot = FALSE;
	int error;	
	int inodeNumber;
	int dataNumber;
	int inodePtr;
	char name[FILENAMESIZE];
	struct t2fs_record record;
	struct t2fs_record newRecord;		
	struct t2fs_inode  inode;

	error = firstTime();
	
	// se for o diretório raiz > CASO ESPECIAL
	if (isRootPath(filename))
		isRoot = TRUE;
	// se path não existir retorna erro
	else if (!findPath(filename, &record))
	{
		return 1;	
	}

	// retorna o nome do arquivo
	getFilename(filename, name);

	// se o nome for válido atualizamos o nome da nova entrada de diretório
	if (validFilename(name))
		strcpy(newRecord.name, name);
	else 
		return 1;

	// inicializa registro
	newRecord.TypeVal = TYPEVAL_REGULAR;
	newRecord.blocksFileSize = 1;
	newRecord.bytesFileSize = 0;

	// procura um inode disponível
	inodeNumber = searchBitmap2(BITMAP_INODE, 0);

	// erro não encontrou um inode disponível
	if (inodeNumber < 0)
	{
		return 1;	
	}
	
	newRecord.inodeNumber = inodeNumber;

	// seta para indisponível inode atual no bitmap de inodes
	setBitmap2(BITMAP_INODE, inodeNumber, 1);
	
	// busca uma área de dados disponível
	dataNumber = searchBitmap2(BITMAP_DADOS, 0);	

	// erro não encontrou uma área de dados disponível
	if (dataNumber < 0)
	{
		return 1;	
	}
	
	// seta para válido a área de dados do registro
	setBitmap2(BITMAP_DADOS, dataNumber, 1); 

	// cria uma nova entrada de diretório
	if (!isRoot)
		createRecord(record.inodeNumber, newRecord, &inodePtr);
	// cria uma entrada de diretório no diretório raiz (CASO ESPECIAL)
	else 
		createRecord(ROOTINODE, newRecord, &inodePtr);

	// inicializa inode
	inode.dataPtr[0] = dataNumber;
	inode.dataPtr[1] = INVALID_PTR;
	inode.singleIndPtr = INVALID_PTR;
	inode.doubleIndPtr = INVALID_PTR;

	// escreve o inode no disco
	error = writeInodeDisk(inode, inodeNumber);

	// abre o arquivo
	if (!error)
		return openFile(newRecord, inodePtr);	
	else
		return -1;
}

int delete2(char *filename)
{
	int isRoot = FALSE;
	int error;
	char name[FILENAMESIZE];
	struct t2fs_record record;
	struct t2fs_inode inode;

	error = firstTime();
	
	// se arquivo estiver localizado no diretório raiz 
	if (isRootPath(filename))
		isRoot = TRUE;
	// se não achar o path retorna erro
	else if (!findPath(filename, &record))
		return 1;

	if (isRoot)
		inode = getInode(ROOTINODE);
	else 
		inode = getInode(record.inodeNumber);

	getFilename(filename, name);

	if (!error)
	{	
		error = deleteFile(inode, name);

		if(error == TRUE)
			return 0;

		return -1;
	}
	else 
		return -1;
}

int mkdir2(char *pathname)
{
	int isRoot = FALSE;
	int error;	
	int inodeNumber;
	int dataNumber;
	int inodePtr;
	char name[FILENAMESIZE];
	struct t2fs_record record;
	struct t2fs_record newRecord;		
	struct t2fs_inode  inode;

	error = firstTime();
	
	// se for o diretório raiz > CASO ESPECIAL
	if (isRootPath(pathname))
		isRoot = TRUE;
	// se path não existir retorna erro
	else if (!findPath(pathname, &record))
	{
		return 1;	
	}

	// retorna o nome do arquivo
	getFilename(pathname, name);

	// se o nome for válido atualizamos o nome da nova entrada de diretório
	if (validFilename(name))
		strcpy(newRecord.name, name);
	else 
		return -1;

	// inicializa registro
	newRecord.TypeVal = TYPEVAL_DIRETORIO;
	newRecord.blocksFileSize = 0;
	newRecord.bytesFileSize = 0;

	// procura um inode disponível
	inodeNumber = searchBitmap2(BITMAP_INODE, 0);

	// erro não encontrou um inode disponível
	if (inodeNumber < 0)
	{
		return -1;	
	}
	
	newRecord.inodeNumber = inodeNumber;

	// seta para indisponível inode atual no bitmap de inodes
	setBitmap2(BITMAP_INODE, inodeNumber, 1);
	
	// busca uma área de dados disponível
	dataNumber = searchBitmap2(BITMAP_DADOS, 0);	

	// erro não encontrou uma área de dados disponível
	if (dataNumber < 0)
	{
		return -1;	
	}
	
	// seta para válido a área de dados do registro
	setBitmap2(BITMAP_DADOS, dataNumber, 1); 

	// cria uma nova entrada de diretório
	if (!isRoot)
		createRecord(record.inodeNumber, newRecord, &inodePtr);
	// cria uma entrada de diretório no diretório raiz (CASO ESPECIAL)
	else 
		createRecord(ROOTINODE, newRecord, &inodePtr);

	// inicializa inode
	inode.dataPtr[0] = dataNumber;
	inode.dataPtr[1] = INVALID_PTR;
	inode.singleIndPtr = INVALID_PTR;
	inode.doubleIndPtr = INVALID_PTR;

	// escreve o inode no disco
	error = writeInodeDisk(inode, inodeNumber);

	return error;
}

int rmdir2(char *pathname)
{
	int isRoot = FALSE;
	int error;
	char directory[FILENAMESIZE];
	struct t2fs_record record;
	struct t2fs_inode inode;
	error = firstTime();
	
	// se arquivo estiver localizado no diretório raiz 
	if (isRootPath(pathname))
		isRoot = TRUE;
	// se não achar o path retorna erro
	else if (!findPath(pathname, &record))
		return 1;

	if (isRoot)
		inode = getInode(ROOTINODE);
	else 
		inode = getInode(record.inodeNumber);

	getFilename(pathname, directory);

	if (!error)
		return deleteDirectory(inode, directory);
	else 
		return -1;
}

int readSimplePtr(struct t2fs_inode inode, int ptrIndex, OPENEDFILE *file, int *size, char *buffer, int*bufferStart)
{
	int i;
	int j;
	int error = 0;
	int sectorLimit;
	int start;
	unsigned int sector;
	unsigned char readBuffer[SECTORSIZE];

	sector = dataStart + inode.dataPtr[ptrIndex]*superblock.blockSize + (file->currentPointer) / SECTORSIZE;

	sectorLimit = dataStart + inode.dataPtr[ptrIndex]*superblock.blockSize + 15;

	//printf("sector %d sectorlimit %d\n", sector, sectorLimit);

	while ((*size) && (sector <= sectorLimit)) 
	{
		error = read_sector(sector, readBuffer) + error;

		start = file->currentPointer % (SECTORSIZE - 1);

		j = *bufferStart;
		for (i = start; i < SECTORSIZE; i++)
		{
			buffer[j] = readBuffer[i]; 

			j++;
			*size = *size - 1;
			*bufferStart = *bufferStart + 1;
			file->currentPointer++;

			if (!*size)
			{
				break;
			}
		}

		sector ++;
	}

	return error;
}

int writeSimple(struct t2fs_inode inode, int ptrIndex, OPENEDFILE *file, int *size, char *buffer, int*bufferStart)
{
	int i;
	int j;
	int error = 0;
	int sectorLimit;
	int start;
	unsigned int sector;
	unsigned char readBuffer[SECTORSIZE];

	sector = dataStart + inode.dataPtr[ptrIndex]*superblock.blockSize + (file->currentPointer) / SECTORSIZE;

	sectorLimit = dataStart + inode.dataPtr[ptrIndex]*superblock.blockSize + 15;

	//printf("sector %d sectorlimit %d\n", sector, sectorLimit);

	while ((*size) && (sector <= sectorLimit)) 
	{
		error = read_sector(sector, readBuffer) + error;

		start = file->currentPointer % (SECTORSIZE - 1);

		j = *bufferStart;

		for (i = start; i < SECTORSIZE; i++)
		{
			readBuffer[i] = buffer[j]; 
		
			j++;
			*size = *size - 1;
			*bufferStart = *bufferStart + 1;
			file->currentPointer++;
			if (file->currentPointer >= file->record.bytesFileSize)
				file->record.bytesFileSize++;

			if (!*size)
			{
				break;
			}
		}

		write_sector(sector, readBuffer);
		sector ++;
	}

	return error;
}

int readSingleIndPtr(struct t2fs_inode inode, OPENEDFILE *file, int *size, char *buffer, int*bufferStart)
{
	int i;
	int j;
	int k;
	int error = -1;
	int dataPtr[PTRPERSECTOR * superblock.blockSize];
	unsigned int sector;
	unsigned char readBuffer[SECTORSIZE];

	getSingleIndInode(inode.singleIndPtr, dataPtr);

	sector = dataStart + dataPtr[0]*superblock.blockSize;

	k = 1;
	while (*size) 
	{
		error = read_sector(sector, readBuffer) + error;

		j = *bufferStart;
		for (i = 0; i < SECTORSIZE; i++)
		{
			buffer[j] = readBuffer[i]; 
			
			j++;
			*size = *size - 1;
			*bufferStart = *bufferStart + 1;
			file->currentPointer++;

			if (!*size)
			{
				break;
			}
		}

		if (dataPtr[k] != INVALID_PTR)
			sector = dataStart + dataPtr[k]*superblock.blockSize;
		else
			break;

		k++;
	}

	return error;
}

int writeSingleInd(struct t2fs_inode inode, OPENEDFILE *file, int *size, char *buffer, int*bufferStart)
{
	int i;
	int j;
	int k;
	int error = -1;
	int dataPtr[PTRPERSECTOR * superblock.blockSize];
	unsigned int sector;
	unsigned char readBuffer[SECTORSIZE];

	getSingleIndInode(inode.singleIndPtr, dataPtr);

	sector = dataStart + dataPtr[0]*superblock.blockSize;

	printf("sector %d", sector);

	k = 1;
	while (*size) 
	{
		error = read_sector(sector, readBuffer) + error;

		j = *bufferStart;
		for (i = 0; i < SECTORSIZE; i++)
		{
			readBuffer[i] = buffer[j]; 
			
			j++;
			*size = *size - 1;
			*bufferStart = *bufferStart + 1;
			file->currentPointer++;

			if (file->currentPointer >= file->record.bytesFileSize)
				file->record.bytesFileSize++;

			if (!*size)
			{
				break;
			}
		}

		error = write_sector(sector, readBuffer) + error;

		if (dataPtr[k] != INVALID_PTR)
			sector = dataStart + dataPtr[k]*superblock.blockSize;
		else 
			break;

		k++;
	}

	return error;
}

int read2(FILE2 handle, char *buffer, int size)
{
	int error = 0;
	int size2;
	int bufferStart = 0;
	OPENEDFILE file;
	struct t2fs_inode inode;

	error = firstTime();

	if (error)
		return -1;

	size2 = size;

	if ((isValidHandle(handle)) && (openedFiles[handle].record.TypeVal == TYPEVAL_REGULAR))
	{
		file = openedFiles[handle];

		inode = getInode(file.record.inodeNumber);
		//printf("%d\n", file.record.inodeNumber);

		if ((file.currentPointer < (superblock.blockSize * SECTORSIZE)) && (file.record.blocksFileSize >= 0))
		{
			readSimplePtr(inode, 0, &file, &size, buffer, &bufferStart);		

			if ((!error) && (size) && (file.record.blocksFileSize > 1))
				readSimplePtr(inode, 1, &file, &size, buffer, &bufferStart);

			if(size && (file.record.blocksFileSize > 2))
				readSingleIndPtr(inode, &file, &size, buffer, &bufferStart);	
		}

		else if (file.currentPointer < (2*superblock.blockSize * SECTORSIZE) && (file.record.blocksFileSize > 1))
		{
			readSimplePtr(inode, 1, &file, &size, buffer, &bufferStart);

			if(size && (file.record.blocksFileSize > 2))
				readSingleIndPtr(inode, &file, &size, buffer, &bufferStart);	
		}

		else
			return -1;

		openedFiles[handle] = file;

		if (size2 > file.record.bytesFileSize)
			return file.record.bytesFileSize;
		else
			return size2;
	}
	return -1;
}

FILE2 open2 (char *filename)
{
	int isRoot = FALSE;
	int inodeNumber;
	int inodePtr;
	char name[FILENAMESIZE];
	struct t2fs_record record;
	struct t2fs_inode inode;

	if (!firstTime())
	{
		// se arquivo estiver localizado no diretório raiz 
		if (isRootPath(filename))
			isRoot = TRUE;
		// se não achar o path retorna erro
		else if (!findPath(filename, &record))
			return 1;

		if (isRoot)
			inodeNumber = ROOTINODE;
		else 
			inodeNumber = record.inodeNumber;

		inode = getInode(inodeNumber);

		getFilename(filename, name);
		
		// procura o arquivo no disco
		if (findFile(inode, name, &record, &inodePtr))	
		{
			return openFile(record, inodePtr);
		}
	}

	return -1;
}

int close2 (FILE2 handle)
{
	OPENEDFILE file;
	
	if (!firstTime() && (isValidHandle(handle)))
	{
		file = openedFiles[handle];

		if(file.record.TypeVal == TYPEVAL_REGULAR)
		{
			if (!(updateRecord(file.inodePtr, file.record)))
			{
				openedFiles[handle].record.TypeVal = TYPEVAL_INVALIDO;
				return 0;
			}
			else return -1;
		}
		// erro
		else 
			return -1;
	}

	else
		return -1;
}

int closedir2(DIR2 handle)
{
	OPENEDFILE file;
	
	if (!firstTime() && (isValidHandle(handle)))
	{
		file = openedFiles[handle];

		if(file.record.TypeVal == TYPEVAL_DIRETORIO)
		{
			if (!(updateRecord(file.inodePtr, file.record)))
			{
				openedFiles[handle].record.TypeVal = TYPEVAL_INVALIDO;
				return 0;
			}
			else return -1;
		}
		// erro
		else 
			return -1;
	}

	else
		return -1;
	
}

DIR2 opendir2 (char *pathname)
{
	int isRoot = FALSE;
	int inodeNumber;
	int inodePtr;
	char name[FILENAMESIZE];
	struct t2fs_record record;
	struct t2fs_inode inode;

	if (!firstTime())
	{
		// se arquivo estiver localizado no diretório raiz 
		if (isRootPath(pathname))
			isRoot = TRUE;
		// se não achar o path retorna erro
		else if (!findPath(pathname, &record))
			return -1;

		if (isRoot)
			inodeNumber = ROOTINODE;
		else 
			inodeNumber = record.inodeNumber;

		inode = getInode(inodeNumber);

		getFilename(pathname, name);
		
		// procura o arquivo no disco
		if (findDirectory(inode, name, &record, &inodePtr))	
		{
			return openFile(record, inodePtr);
		}
	}

	return -1;
}

int readdir2 (DIR2 handle, DIRENT2 *dentry)
{
	OPENEDFILE file;
	struct t2fs_inode inode;
	struct t2fs_record record[64];

	if(!firstTime() && (isValidHandle(handle)))
	{
		file = openedFiles[handle];

		if((file.record.TypeVal != TYPEVAL_INVALIDO) && (file.record.TypeVal == TYPEVAL_DIRETORIO))
		{ 
			inode = getInode(file.record.inodeNumber);

			if (file.currentPointer < RECORDSPERSECTOR*superblock.blockSize)
			{
				readDirectoryFromDisk(inode.dataPtr[0], record);

				if (record[file.currentPointer].TypeVal != TYPEVAL_INVALIDO)
				{
					strcpy(dentry->name, record[file.currentPointer].name);
					dentry->fileType = record[file.currentPointer].TypeVal;
					dentry->fileSize = record[file.currentPointer].bytesFileSize;
					file.currentPointer++;

					openedFiles[handle] = file;
					
					// sucesso
					return 0;
				}

				else 
					return -1;
			}
			else
			{
				readDirectoryFromDisk(inode.dataPtr[1], record);

				if (record[file.currentPointer].TypeVal == TYPEVAL_INVALIDO)
				{
					strcpy(dentry->name, record[file.currentPointer % RECORDSPERSECTOR*superblock.blockSize].name);
					dentry->fileType = record[file.currentPointer   % RECORDSPERSECTOR*superblock.blockSize].TypeVal;
					dentry->fileSize = record[file.currentPointer   % RECORDSPERSECTOR*superblock.blockSize].bytesFileSize;
					file.currentPointer++;

					openedFiles[handle] = file;
					
					// sucesso
					return 0;
				}
				else 
					return -1;
			}
		}
		// erro
		else 
			return -1;
	}
	else
		return -1;
}

int write2 (FILE2 handle, char *buffer, int size)
{
	int bufferStart = 0;
	int error = 0;
	int dataPtr;
	int size2;
	struct t2fs_inode inode; 
	OPENEDFILE file;

	size2 = size;

	if(!firstTime() && (isValidHandle(handle)))
	{
		file = openedFiles[handle];

		inode = getInode(file.record.inodeNumber);
		//printf("%d\n", file.record.inodeNumber);

		if ((file.currentPointer < (superblock.blockSize * SECTORSIZE)) && (file.record.blocksFileSize >= 0))
		{
			error = writeSimple(inode, 0, &file, &size, buffer, &bufferStart);		

			if ((!error) && (size) && (file.record.blocksFileSize > 1))
			{
				// busca uma área de dados disponível
				dataPtr = searchBitmap2(BITMAP_DADOS, 0);	

				// erro não encontrou uma área de dados disponível
				if (dataPtr < 0)
				{
					return 1;	
				}

				// seta para válido a área de dados do registro
				setBitmap2(BITMAP_DADOS, dataPtr, 1); 

				inode.dataPtr[1] = dataPtr;

				writeInodeDisk(inode, file.record.inodeNumber);

				writeSimple(inode, 1, &file, &size, buffer, &bufferStart);
			}

			if((!error) && (size) && (file.record.blocksFileSize > 2))
			{
				// busca uma área de dados disponível para o bloco de ponteiros simples
				dataPtr = searchBitmap2(BITMAP_DADOS, 0);	

				// erro não encontrou uma área de dados disponível
				if (dataPtr < 0)
				{
					return 1;	
				}

				// seta para válido a área de dados do registro
				setBitmap2(BITMAP_DADOS, dataPtr, 1); 

				inode.singleIndPtr = dataPtr;

				writeInodeDisk(inode, file.record.inodeNumber);

				// busca uma área de dados disponível para a área de dados do registro
				dataPtr = searchBitmap2(BITMAP_DADOS, 0);	

				// erro não encontrou uma área de dados disponível
				if (dataPtr < 0)
				{
					return 1;	
				}

				// seta para válido a área de dados do registro
				setBitmap2(BITMAP_DADOS, dataPtr, 1); 

				// escreve o ponteiro para área de dados no disco 
				writeSingleIndPtr(inode.singleIndPtr, dataPtr);

				writeSingleInd(inode, &file, &size, buffer, &bufferStart);	
			}
		}

		else if (file.currentPointer < (2*superblock.blockSize * SECTORSIZE) && (file.record.blocksFileSize > 1))
		{
			error = writeSimple(inode, 1, &file, &size, buffer, &bufferStart);

			if((!error) && (size) && (file.record.blocksFileSize > 2))
			{
				// busca uma área de dados disponível para o bloco de ponteiros simples
				dataPtr = searchBitmap2(BITMAP_DADOS, 0);	

				// erro não encontrou uma área de dados disponível
				if (dataPtr < 0)
				{
					return 1;	
				}

				// seta para válido a área de dados do registro
				setBitmap2(BITMAP_DADOS, dataPtr, 1); 

				inode.singleIndPtr = dataPtr;

				writeInodeDisk(inode, file.record.inodeNumber);

				// busca uma área de dados disponível para a área de dados do registro
				dataPtr = searchBitmap2(BITMAP_DADOS, 0);	

				// erro não encontrou uma área de dados disponível
				if (dataPtr < 0)
				{
					return 1;	
				}

				// seta para válido a área de dados do registro
				setBitmap2(BITMAP_DADOS, dataPtr, 1); 

				// escreve o ponteiro para área de dados no disco 
				writeSingleIndPtr(inode.singleIndPtr, dataPtr);

				writeSingleInd(inode, &file, &size, buffer, &bufferStart);	
			}	
		}

		else
			return -1;

		openedFiles[handle] = file;

		return size2;
	}
	else
		return -1;
}

int seek2 (FILE2 handle, DWORD offset)
{
	if (!(firstTime()) && (isValidHandle(handle)))
	{
		if (offset >= 0)
		{
			openedFiles[handle].currentPointer = offset;
			return offset + 1;
		}
		else if (offset == -1)
		{
			openedFiles[handle].currentPointer = openedFiles[handle].record.bytesFileSize - 1;
			return openedFiles[handle].record.bytesFileSize;
		}
		else
			return -1;
	}
	else
		return -1;
}

int truncate2 (FILE2 handle)
{
	if((!firstTime()) && (isValidHandle(handle)))
	{
		if(openedFiles[handle].currentPointer < SECTORSIZE*superblock.blockSize)
		{
			if(openedFiles[handle].record.blocksFileSize > 1)
			{
				openedFiles[handle].record.bytesFileSize = openedFiles[handle].record.bytesFileSize - SECTORSIZE*superblock.blockSize; 
			}

			openedFiles[handle].record.bytesFileSize = openedFiles[handle].record.bytesFileSize - openedFiles[handle].currentPointer;
			return 0;
		}
		else if(openedFiles[handle].currentPointer < SECTORSIZE*superblock.blockSize*2)
		{
			openedFiles[handle].record.bytesFileSize = openedFiles[handle].record.bytesFileSize - openedFiles[handle].currentPointer;
			return 0;
		}
	}
	return -1;
}
