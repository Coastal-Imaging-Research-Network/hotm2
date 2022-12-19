/* average.c -- calculate average intensity */


/* 
 *  $Log: average.c,v $
 *  Revision 1.1  2009/02/20 21:13:47  stanley
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

static const char rcsid[] = "$Id: average.c 243 2016-10-06 23:33:27Z stanley $";


#include "hotm.h"

#define VERBOSE(x) if(me->verbose & x)
#define VERB_INIT 1
#define VERB_PROCESS 2
#define VERB_SAVE 4

/* nothing to do for saving results */

void _saveResults( struct _processModule_ *me )
{

	
}


void _processFrame( struct _processModule_ *me, struct _frame_ *frame )
{
struct timeval start;
struct timeval end;
int err;
register unsigned char *ptr;
unsigned long i;
unsigned long summ; 
double avg;

	err = gettimeofday( &start, NULL );
	if( err ) 
		perror( me->module );
	
	ptr = frame->dataPtr;

	summ = 0; i = cameraModule.bufferLength;
	while( i-- ) 
		summ += *ptr++;

	avg = (1.0 * summ) / (1.0 * cameraModule.bufferLength);

	printf("Average Value camera %ld: %lf at frame %ld\n",
		cameraModule.cameraNumber,
		avg,
		cameraModule.frameCount);
	

					
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
char 			*module = "average";
struct _commandFileEntry_ *cmd;
int i;
unsigned long  modulo;
	
	/* problem -- how do I find ME in the process list? */
	/* well, I will be the last member of the list! */
	me = firstProcessModule;
	while( me->next ) 
		me = me->next;
	
	strcpy( me->module, module );
	modulo = 2;
	
	getLongParam( &me->verbose, "averageVerbose", module );

	me->remaining = -1;
	
	VERBOSE(VERB_INIT) {
		KIDID; 
		printf("cam %ld: %s: _init\n", 
				cameraModule.cameraNumber,
				module );
	}

	me->processFrame = _processFrame;
	me->processPixel = NULL;
	me->closeFrame = NULL;
	me->saveResults = _saveResults;
	
}
	
	
