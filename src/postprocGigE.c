/* post processing for the GigE cameras */
/* cannot do internal to demon, takes too long and
   I lose track of the cameras. Must be separate process */

#define _MAIN_  /* so we actually allocate structs */

#include "demon2.h"

#include <stdio.h>
#include <errno.h>

#define PRINTME if(1) printf

#define testMalloc(x) if( NULL == x ) { printf("failed to malloc x\n"); exit(-2);}

/* KLUDGE! */
#define RUNNINGK (3.0)

int is16bit;

#define TEST16BIT  \
     if( (cameraModule.format == formatMONO16)\
        || (cameraModule.format == formatRGB16) )\
                is16bit = 1;\
     else is16bit = 0

int writePNG;

FILE *logFD;

#define freeAndClear(x) if(x) { free(x); x = NULL; }

int main( int argc, char **argv )
{

struct _diskdata_ p;
FILE *input;
unsigned long didRead;

	/* logFD = fopen( "/dev/tty", "w" ); */ 
	/* USE STDOUT for output -- so cron can catch it all */
	logFD = stdout;

	if( argc < 2 ) {
		printf("demonPostProcGigE failed: no input file name\n");
		exit(-1);
	}

	input = fopen( argv[1], "r" );
	if( NULL == input ) {
		printf("demonPostProcGigE file open failed: %s\n", argv[0] );
		perror("here is why: ");
		exit(-1);
	}

	/* read in all the data. First, meta data */
	didRead = fread( &p, sizeof(p), 1, input );
	PRINTME( "did read %lu diskdata structs\n", didRead );

	didRead = fread( &cameraModule, sizeof(cameraModule), 1, input );
	PRINTME( "did read %lu cameraModule structs\n", didRead );

	didRead = fread( &hotmParams, sizeof(hotmParams), 1, input );
	PRINTME( "did read %lu hotmParams structs\n", didRead );

	writePNG = hotmParams.writePNG;

	/* test output */
	PRINTME( "name of camera: %s\n", cameraModule.module );
	PRINTME( "name of module: %s\n", cameraModule.library );
	PRINTME( "name of files: %s\n", p.name );
	PRINTME( "frame interval: %.3f\n", p.frameInterval );
	PRINTME( "camera number: %ld\nx: %ld y: %ld\n", 
			cameraModule.cameraNumber, 
			cameraModule.x, cameraModule.y );
	PRINTME( "bytes in frame: %u pixels: %u\n", p.size, p.numPix );
	
	/* now must malloc memory for incoming data */
	p.snapPtr    = malloc( p.numPix * sizeof(uint16_t) );
	testMalloc( p.snapPtr );
	didRead = fread( p.snapPtr, sizeof(uint16_t), p.numPix, input );
	PRINTME( "didRead %lu snap pixels\n", didRead );

	p.sumPtr     = malloc( p.numPix * sizeof(uint32_t) );
	testMalloc( p.sumPtr );
	didRead = fread( p.sumPtr, sizeof(uint32_t), p.numPix, input );
	PRINTME( "didRead %lu sum pixels\n", didRead );

	p.sumsqPtr   = malloc( p.numPix * sizeof(uint64_t) );
	testMalloc( p.sumsqPtr );
	didRead = fread( p.sumsqPtr, sizeof(uint64_t), p.numPix, input );
	PRINTME( "didRead %lu sumsq pixels\n", didRead );

	p.brightPtr  = malloc( p.numPix * sizeof(uint16_t) );
	testMalloc( p.brightPtr );
	didRead = fread( p.brightPtr, sizeof(uint16_t), p.numPix, input );
	PRINTME( "didRead %lu bright pixels\n", didRead );

	p.darkPtr    = malloc( p.numPix * sizeof(uint16_t) );
	testMalloc( p.darkPtr );
	didRead = fread( p.darkPtr, sizeof(uint16_t), p.numPix, input );
	PRINTME( "didRead %lu dark pixels\n", didRead );

	p.runDarkPtr = malloc( p.numPix * sizeof(double) );
	testMalloc( p.runDarkPtr );
	didRead = fread( p.runDarkPtr, sizeof(double), p.numPix, input );
	PRINTME( "didRead %lu runDark pixels\n", didRead );

	demonPostProc( &p );


}
