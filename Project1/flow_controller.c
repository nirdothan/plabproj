/*
This module is the main flow controller which follows the algorithm 
depicted in page 28 of the course booklet
*/
#include "assemblerTypes.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

Word_t g_programSegment[MAX_PC];	/* global Program Segment */
int g_ICWordCount[MAX_PC];			/* global counter of words per each base element in g_programSegment */
int g_ICFlag[MAX_PC];				/* global flag per each element in g_programSegment */
Word_t g_dataSegment[MAX_PC];		/* global Data Segment */
int g_IC = INIT_IC;					/* global instructions counter */
int g_DC = INIT_DC;					/* global data counter */
Symbol_t *g_symbolTable;			/* global symbol table */
int g_symbolTableSize=0;			/* global symbol table current size */
Symbol_t *g_externalTable;			/* global external table */
int g_externalTableSize = 0;		/* global external table current size */
Symbol_t *g_entryTable;				/* global entry table */
int g_entryTableSize = 0;			/* global entry table current size */
static char labelFlag = 0;			/* module level label idenfification flag */


int parseRowFirst(const char *);
int parseRowSecond(const char *);
void updateDataSegemnt();
void cleanup();

int firstPass(char *inputFile){
	int status;
	char *row=NULL;

	g_symbolTable = (Symbol_t*)malloc(sizeof(Symbol_t) * MAX_SYMBOLS);
	if (!g_symbolTable)
		return reportError("Fatal! Unable to allocate memory\n", FATAL);
	g_entryTable = (Symbol_t*)malloc(sizeof(Symbol_t) * MAX_SYMBOLS);
	if (!g_entryTable)
		return reportError("Fatal! Unable to allocate memory\n", FATAL);
	g_externalTable = (Symbol_t*)malloc(sizeof(Symbol_t) * MAX_SYMBOLS);
	if (!g_entryTable)
		return reportError("Fatal! Unable to allocate memory\n", FATAL);



	/* open input file for reading*/
	if (FATAL == initForFile(inputFile))
		return FATAL;


	/* process one row at a time*/
	while (END != readLine(&row)){
		status = parseRowFirst(row);
		free(row);
		if (FATAL == status)
			return FATAL;
		
	}

	return NORMAL;
}

int secondPass()
{
	int status;
	char *row = NULL;


	rewindInputFile();

	/* process one row at a time*/
	while (END != readLine(&row)){
		status = parseRowSecond(row);
		free(row);
		if (FATAL == status)
			return FATAL;

	}


	return NORMAL;
}



/* row level parsing activities for first pass */
int parseRowFirst(const char *row){
	int status, orig_address=0;
	char label[MAX_LABEL_SIZE + 1];
	int instructionFlag = 0;	 /*  instruction idenfification flag */


	/* TODO remove 
	printf("row is <%s>\n", row); */

	if (isCommentOrEmpty(row))
		return NORMAL;


	/* 
	step #3-4 on p 28 
	if label is found the labelFlag is set to 1
	*/
	if ((status = parseLabel(&row, &labelFlag, label)) != NORMAL)
		return status;


	/*
	step #5 and #8 on p 28
	if data instruction is found the instructionFlag is set per enum
	*/
	if ((status = parseInstruction(&row, &instructionFlag, &orig_address)) != NORMAL)
		return status;



	/* 	step #6 on p 28 */
	if (labelFlag) {
		if (DATA_INSTRUCTION_FLAG == instructionFlag ||
			STRING_INSTRUCTION_FLAG == instructionFlag)
			return insertLabel(label, DATA_LABEL, orig_address);
		else
		{
			
			/*step #11 on p 28 */
			return insertLabel(label, CODE_LABEL, orig_address);
		}
	}
		

	/* 	step #9 on p 28 */
	if (EXTERN_INSTRUCTION_FLAG == instructionFlag){
		char label[MAX_LABEL_SIZE + 1];
		if (sscanf(row, "%s", label) != 1)
			return  reportError("Error! Illegal external instruction\n", ERROR);
		return insertLabel(label, EXT_LABEL, 0);
	}
		
	
	return NORMAL;

}



/* row level parsing activities for first pass */
int parseRowSecond(const char *row){
	int status, orig_address = 0;
	char label[MAX_LABEL_SIZE + 1];
	int instructionFlag = 0;	 /*  instruction idenfification flag */

	/*TODO remove
	printf("row is <%s>\n", row);
	*/
	if (isCommentOrEmpty(row))
		return NORMAL;


	/*
	step #2 on p 28 - second pass
	*/
	if ((status = parseLabel(&row, &labelFlag, label)) != NORMAL)
		return status;


	/*
	step #5 and #8 on p 28
	if data instruction is found the instructionFlag is set per enum
	*/
	if ((status = parseInstructionSecondPass(&row, &instructionFlag, &orig_address)) != NORMAL)
		return status;





	return NORMAL;

}


/*between first and second passes we increment the addresses of data
labels by the value of IC to have them come right after the code segment */
void updateDataSegemnt(){
	incrementDataLabels(g_IC);
	/* 	step #1 on p 28	*/
	g_IC = INIT_IC;
}

/* flush all buffers to output files*/
int writeOutput(){
	if (NORMAL != initOutput())
		return FATAL;

	if (NORMAL != flushObjFile())
		return FATAL;

	if (NORMAL != flushExtFile())
		return FATAL;

	if (NORMAL != flushEntFile())
		return FATAL;

	
	closeFiles();
	return NORMAL;
}
/*
input file level cleanups and initalizations
*/
void cleanup(){
	free(g_symbolTable);
	free(g_entryTable);
	free(g_externalTable);
	g_IC = INIT_IC;					
	g_DC = INIT_DC;				
	g_symbolTableSize = 0;			
	g_externalTableSize = 0;			
	g_entryTableSize = 0;			
	labelFlag = 0;			

	memset(g_programSegment, 0, MAX_PC);
	memset(g_ICFlag, 0, MAX_PC);
	memset(g_dataSegment, 0, MAX_PC*sizeof(Word_t));
	memset(g_ICWordCount, 0, MAX_PC);


}




/* translate flag values to ascii chars*/
char flagToAscii(int flag){
	switch (flag){
	case EXTERNAL_ADDRESS:
		return 'e';
	case ABSOLUTE_ADDRESS:
		return 'a';
	case RELATIVE_ADDRESS:
		return 'r';
	default:
		return 0;

	}
}

/* flush IC and DC buffers to obj file */
int flushObjFile(){
	int i,j;
	char row[MAX_ROW_SIZE];

	/*write header*/
	sprintf(row, "%d %d", getOctal(g_IC - INIT_IC), getOctal(g_DC));
	writeObjLine(row);

	/*traverse code segment*/
	for (i = INIT_IC; i < g_IC ; i++){
		char flag = flagToAscii(g_ICFlag[i]);
		int dec = mapwordtodecimal(&(g_programSegment[i]));

		/* for debugging bits */
		/* char *bits = get20LSBs(&(g_programSegment[i])); */
		if (!flag)
			return reportError("ERROR! invalid addressing flag\n");
		sprintf(row, "%d %.7d %c", getOctal(i),  getOctal(dec), flag);
		
		writeObjLine(row);
		/*free(bits);*/
	}



	/*traverse data segment*/
	for (j = 0; j < g_DC; j++){
		int dec = mapwordtodecimal(&(g_dataSegment[j]));

		/* for debugging bits */
		/*char *bits = get20LSBs(&(g_dataSegment[j]));*/
		sprintf(row, "%d %.7d", getOctal( i++), getOctal(dec));
		
		writeObjLine(row);
		/*free(bits);*/
	}


	
	return NORMAL;
}

/* flush entry table to entry file */
int flushEntFile(){

	int i;
	char row[MAX_ROW_SIZE];

	/*traverse exytrenal table */
	for (i = 0; i < g_entryTableSize; i++){
		sprintf(row, "%s %d", g_entryTable[i].label, g_entryTable[i].octal);

		writeEntLine(row);

	}

	return NORMAL;
}


/* flush external table to external file */
int flushExtFile(){
	int i;
	char row[MAX_ROW_SIZE];

	/*traverse exytrenal table */
	for (i = 0; i < g_externalTableSize; i++){
		sprintf(row, "%s %d", g_externalTable[i].label, g_externalTable[i].octal);

		writeExtLine(row);
		
	}

	return NORMAL;
}
