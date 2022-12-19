/* stack.c -- the stack module */

/* 
 *  $Log: stack.c,v $
 *  Revision 1.3  2003/01/07 22:02:30  stanley
 *  cleanups to make intel compiler happy
 *
 *  Revision 1.2  2002/08/09 00:32:40  stanleyl
 *  change verbose parameterization
 *
 *  Revision 1.1  2002/03/22 19:50:26  stanley
 *  Initial revision
 *
 */

/*  
 * John Stanley
 * Coastal Imaging Lab
 * College of Oceanic and Atmospheric Sciences
 * Oregon State University
 * Corvallis, OR 97331
 *
 * $Copyright 2002 Argus Users Group$
 *
 */

static const char rcsid[] = "$Id: stack.c 172 2016-03-25 21:03:51Z stanley $";


#include "hotm.h"
#include <sys/stat.h>
#include <fcntl.h>
#include "rasterfile.h"

#define VERBOSE(x) if(me->verbose & x)
#define VERB_INIT 1
#define VERB_PROCESS 2
#define VERB_SAVE 4
#define VERB_AOI 8

struct {
	unsigned long index;
	unsigned long count;
	unsigned long width;
	/* note to self, pixList will be 1 based UV, 0 based pixel */
        unsigned long *pixList;
	long	      pixCount;
	char          stackFilename[128];
	char          aoiFilename[128];
	int           stackFD;
	unsigned char *outputLine;
	unsigned long startSec;
} stackData;

void _saveResults( struct _processModule_ *me )
{

	/* nothing to do but close the file! */
	close(stackData.stackFD);

}


void _processFrame( struct _processModule_ *me, struct _frame_ *frame )
{
struct timeval start;
struct timeval end;
struct timeval now;
int err;
register unsigned char *ptr;
register unsigned char *data;
unsigned long i;
unsigned char *outPtr;
unsigned char r,g,b;
unsigned long temp;
struct _pixel_ pix;

	err = gettimeofday( &start, NULL );
	if( err ) 
		perror( me->module );
	
	outPtr = (unsigned char *) stackData.outputLine;
	ptr = frame->dataPtr;
	now = frame->when;

	/* milliseconds since starting this stack */
	temp = (now.tv_usec / 1000) + ((now.tv_sec-stackData.startSec) * 1000);
	temp = htonl(temp);
	write( stackData.stackFD, &temp, 4 );
	temp = htonl(stackData.count);
	write( stackData.stackFD, &temp, 4 );

	/* ok, here we grab the stack data */
	/* in a marvelous twist of irony, I want it in YUV! */
	/* but I do not want to convert it all to YUV to save time */
	/* so, I'll do my own math to get the right bytes from the */
	/* char * ptr. */

	for( i=0; i<stackData.pixCount; i++ ) {

		switch( cameraModule.format ) {
	
			case formatYUV422:
				data = ptr + (stackData.pixList[i] * 2) + 1;
				*outPtr++ = *data;
				break;

			case formatYUV444:
				data = ptr + (stackData.pixList[i] * 3) + 1;
				*outPtr++ = *data;
				break;

			case formatRGB8:
				/* point to R */
				data = ptr + (stackData.pixList[i] * 3);
				*outPtr++ = 16 + ((*data++) *  66 
					           +  (*data++) * 129 
					           +  (*data)   *  25) / 256;
				break;

			case formatMONO8:
				data = ptr + (stackData.pixList[i]);
				*outPtr++ = *data;
				break;

			default:
				/* not implemented yet */
				break;
		}
	}
	
	write( stackData.stackFD, stackData.outputLine, stackData.pixCount );

	me->remaining--;
	stackData.count++;
	
	err = gettimeofday( &end, NULL );
	if( err ) {
		perror( me->module );
		me->processMicros = 9999999;
		return;
	}
	
	me->processMicros = (end.tv_sec - start.tv_sec) * 1000000
		+ (end.tv_usec - start.tv_usec );
		
	return;
}

void _processPixel( struct _processModule_ *me, struct _pixel_ *pix )
{
	return;
}

void _closeFrame( struct _processModule_ *me )
{
	return;
}

void _init()
{
unsigned long 		size;
struct _processModule_ 	*me;
char 			*module = "stack";
struct _commandFileEntry_ *cmd;
FILE *in;
char line[128];
double U, V;
unsigned long temp;
int iu, iv;
int i;
struct timeval now;
struct rasterfile r;
char cmap[256];
unsigned long *UV;
long fakeUST;

struct {
	unsigned long when;
	char cam;
	char gain;
	char offset;
	char version;
} line1;

struct {
	long            undef;
	char            hz;
	char            color;
	unsigned short  count;
} line2;

struct {
	char            name[8];
} line34;

	
	/* problem -- how do I find ME in the process list? */
	/* well, I will be the last member of the list! */
	me = firstProcessModule;
	while( me->next ) 
		me = me->next;
	
	strcpy( me->module, module );
	
	stackData.width = cameraModule.x;

	stackData.count = stackData.pixCount = 0;

	getLongParam( &me->remaining, "stackSamples", module );
	getLongParam( &me->verbose, "stackVerbose", module );
	getBooleanParam( &me->debug, "stackDebug", module );

	VERBOSE(VERB_INIT) {
		KIDID; 
		printf("%s: _init (%d)\n", module, me->debug );
	}
	
	/* get aoi filename */
	stackData.aoiFilename[0] = 0;
	getStringParam( &stackData.aoiFilename[0], "stackAOIFile", module );
	if( strlen(stackData.aoiFilename) == 0 ) {
		printf("%s: no pixlist provided!\n", module );
		exit(-1);
	}

	VERBOSE(VERB_INIT) {
		KIDID;
		printf("%s: aoi file: %s\n", 
				module, 
				stackData.aoiFilename );
	}
	
	/* open it */
	in = fopen( stackData.aoiFilename, "r" );
	if( NULL == in ) {
		printf("%s: cannot open aoifile %s\n", 
			module,
			stackData.aoiFilename );
		exit(-1);
	}

	/* one pass to figure out how many entries it has */
	while( fgets( line, sizeof(line), in ) ) {
		/* do scanf, any failure is not counted */
		if( me->debug ) printf("%s: aoi line: %s", module, line );
		if( 2 != sscanf( line, "%lf %lf", &U, &V ) ) {
			VERBOSE(VERB_AOI) {
				KIDID;
				printf("%s: ignoring aoi line: %s",
						module, line );
			}
		}
		else {
			stackData.pixCount++;
		}
	}
	
	VERBOSE(VERB_INIT) {
		KIDID; 
		printf("%s: found %d pixels\n", module, stackData.pixCount );
	}
	
	/* deal with Sun rasterfile even length requirement */
	if( stackData.pixCount % 2 ) stackData.pixCount++;

	rewind( in );

	/* malloc the space for data */
	stackData.pixList = calloc( stackData.pixCount, sizeof(unsigned long) );
	if( NULL == stackData.pixList ) {
		printf("%s: alloc of pixlist failed!\n", module );
		exit(-1);
	}
	stackData.outputLine = 
			calloc( stackData.pixCount + 8, sizeof(unsigned char) );
	if( NULL == stackData.outputLine ) {
		printf("%s: alloc of outputLine failed!\n", module );
		exit(-1);
	}
	
	/* and a temporary list of UV's for the stack header later */
	UV = calloc( stackData.pixCount, sizeof(unsigned long) );

	/* start over, */
	i = 0;
	while( fgets( line, sizeof(line), in ) ) {
		if( 2 == sscanf( line, "%lf %lf", &U, &V ) ) {
			iu = U; iv = V;
			if( (iu >= cameraModule.left ) 
			 && (iv >= cameraModule.top )
			 && (iu < (cameraModule.x + cameraModule.left) ) 
			 && (iv < (cameraModule.y + cameraModule.top) ) ) {

				UV[i] = (iu<<16) | (iv);

				iu -= cameraModule.left;
				iv -= cameraModule.top;
				stackData.pixList[i] = iv * cameraModule.x + iu;

				if( me->debug ) 
					printf("%s: U: %f V: %f Index: %d\n",
						module,
						U, V,
						stackData.pixList[i] );

				i++;
			}
			else
				printf("%s: UV out of limits: %f/%f ignored\n", module, U, V );
		}

	}
	fclose(in);

	/* stupid Sun */
	if( i%2 ) i++;

	/* check for lost lines */
	if( i != stackData.pixCount ) {
		stackData.pixCount = i;
		printf("%s: mismatch pixCount != count, using %d\n", 
			module,
			stackData.pixCount );
	}

	/* save data about start of stack */
	gettimeofday( &now, NULL );
	stackData.startSec = now.tv_sec;
	
    sprintf( stackData.stackFilename, 
             "%d.c%d.stack.ras", 
             hotmParams.baseEpoch++,
             cameraModule.cameraNumber );
			 
	VERBOSE(VERB_INIT) {
		KIDID; 
		printf("%s: output file: %s\n", 
				module,
				stackData.stackFilename );
	}
	
	stackData.stackFD = open( stackData.stackFilename,
							O_WRONLY | O_CREAT,
							0644 );
	if( -1 == stackData.stackFD ) {
			printf("%s: cannot open output: %s\n", 
					module, stackData.stackFilename );
			perror(module);
			exit(-1);
	}

	/* ain't life fun? we're on a good-endian system, but since */
	/* rasterfiles are based on Suns -- bad endian -- everyone */
	/* treats the header as wrong-endian and so must I */	
    /* magic numbers */
    r.ras_magic =     htonl(RAS_MAGIC);
	r.ras_width =     htonl(stackData.pixCount + 8);
	r.ras_height =    htonl(me->remaining + 8);
    r.ras_length =    htonl(r.ras_width * r.ras_height);
    r.ras_depth =     htonl(8);
    r.ras_type =      htonl(RT_STANDARD);
    r.ras_maptype =   htonl(RMT_EQUAL_RGB);
    r.ras_maplength = htonl(3 * 256);

    /* put in a color map */
    for (i = 0; i < 256; cmap[i] = i++);

    write(stackData.stackFD, &r, 32);
    write(stackData.stackFD, cmap, 256);
    write(stackData.stackFD, cmap, 256);
    write(stackData.stackFD, cmap, 256);

	/* THANK GOODNESS WE ARE BACK ON A SYSTEM WITH THE RIGHT ENDIAN */
	line1.when = now.tv_sec;
	line1.cam  = cameraModule.cameraNumber;
	line1.gain =
	line1.offset = 0;
	line1.version = 4;

	for( i=0; i<stackData.pixCount; i++ )
		stackData.outputLine[i] = (UV[i] & 0x00ff0000) >> 16;
		
	write(stackData.stackFD, &line1, sizeof(line1) );
	write(stackData.stackFD, &stackData.outputLine[0], stackData.pixCount );
	
	line2.undef = 0;
	line2.hz = 1;
	line2.color = 0;
	line2.count = me->remaining;
	 
	for( i=0; i<stackData.pixCount; i++ )
		stackData.outputLine[i] = (UV[i] & 0xff000000) >> 24;
		
	write(stackData.stackFD, &line2, sizeof(line2) );
	write(stackData.stackFD, &stackData.outputLine[0], stackData.pixCount );
	
	strncpy( &line34.name[0], hotmParams.sitename, 8 );

	for( i=0; i<stackData.pixCount; i++ )
		stackData.outputLine[i] = (UV[i] & 0x000000ff);
		
	write(stackData.stackFD, &line34, sizeof(line34) );
	write(stackData.stackFD, &stackData.outputLine[0], stackData.pixCount );
	
	strncpy( &line34.name[0], stackData.aoiFilename, 8 );

	for( i=0; i<stackData.pixCount; i++ )
		stackData.outputLine[i] = (UV[i] & 0x0000ff00) >> 8;
		
	write(stackData.stackFD, &line34, sizeof(line34) );
	write(stackData.stackFD, &stackData.outputLine[0], stackData.pixCount );
	
	free(UV);
	
	/* fake UST -- set to fraction of second -- pretend we just rebooted */
	fakeUST = 0;
	write(stackData.stackFD, &fakeUST, 4);
	fakeUST = htonl(now.tv_usec * 1000);
	write(stackData.stackFD, &fakeUST, 4);
	strncpy( stackData.outputLine, 
				stackData.aoiFilename,
				stackData.pixCount ); 
	write(stackData.stackFD, &stackData.outputLine[0], stackData.pixCount );

	/* line 6 */	
	now.tv_sec = htonl(now.tv_sec);
	now.tv_usec = htonl(now.tv_usec); 
	write( stackData.stackFD, &now, 8);
	write(stackData.stackFD, &stackData.outputLine[0], stackData.pixCount );

	/* 7 and 8 are unused, reserved for future expansion */
	write( stackData.stackFD, &now, 8);
	write(stackData.stackFD, &stackData.outputLine[0], stackData.pixCount );
	write( stackData.stackFD, &now, 8);
	write(stackData.stackFD, &stackData.outputLine[0], stackData.pixCount );
	
	me->processFrame = _processFrame;
	me->processPixel = NULL;
	me->closeFrame = NULL;
	me->saveResults = _saveResults;
	
}
	
	
