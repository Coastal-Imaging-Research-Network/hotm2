/* demon.c -- write data to disk when asked
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

static const char rcsid[] = "$Id: offsetTest.c 172 2016-03-25 21:03:51Z stanley $";


#include "hotm.h"

extern int errno;


void _saveResults( struct _processModule_ *me )
{

}


void _processFrame( struct _processModule_ *me, struct _frame_ *frame )
{
struct timeval start;
struct timeval end;
unsigned char *ptr;
int err;
long i;
double now;
static double previous;
long x,y;
double count, summ, summsq;
double var;
static double lastVar;
	
	err = gettimeofday( &start, NULL );
	if( err ) 
		perror( me->module );
		
	ptr = frame->dataPtr;
	now = 1.0 * frame->when.tv_sec + (frame->when.tv_usec/1000000.0);

	if( previous == 0 )
		previous = now;

	/* code to do variance */		
	ptr=frame->dataPtr;
	
	summ = summsq = count = 0;
	for( x=496; x<529; x++ ) {
		for( y=368; y<400; y++ ) {
			i = (y*1024 + x) * 2 + 1;
			summ += ptr[i];
			summsq += (ptr[i] * ptr[i]);
			count++;
		}
	}
			
	var = summsq / count - (summ/count) * (summ/count);
	printf( "var: %10.4lf %c\n", var, 
		lastVar>var? '-' : '+' );
	lastVar = var;
		
	previous = now;
	/*me->remaining--;*/

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
char 			*module = "disk";
struct _commandFileEntry_ *cmd;
int i;
char line[128];
	
	/* problem -- how do I find ME in the process list? */
	/* well, I will be the last member of the list! */
	me = firstProcessModule;
	while( me->next ) 
		me = me->next;
	
	strcpy( me->module, module );
	
	me->remaining = 1;
	getLongParam( &me->verbose, "diskVerbose", module );

	
	me->processFrame = _processFrame;
	me->processPixel = NULL;
	me->closeFrame = NULL;
	me->saveResults = _saveResults;
	
}
	
	
