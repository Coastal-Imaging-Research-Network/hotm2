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

static const char rcsid[] = "$Id: focus.c 346 2018-07-20 22:51:22Z stanley $";


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
static double maxVar = 0;
int pc;
static int ii = 0;
	
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
	for( x=cameraModule.x/2-50; x<cameraModule.x/2+50; x+=2 ) {
		for( y=cameraModule.y/2-50; y<cameraModule.y/2+50; y+=2 ) {
			i = (y*cameraModule.x + x) + 1;
			summ += ptr[i];
			summsq += (ptr[i] * ptr[i]);
			count++;
		}
	}
			
	var = summsq / count - (summ/count) * (summ/count);
	var = (2.0*lastVar + var)/3.0;
	/*var = var * 1000 / summ;*/
	if( ii++ > 100 ) { maxVar = var; ii=0; }
	if( var>maxVar ) maxVar=var;
	pc = 40.0 * ( var/maxVar );
	printf( "var: %3d %10.4lf %10.4lf %c ", ii, var, maxVar, 
		lastVar>var? '-' : '+' );
	for( x=0; x<pc; x++ ) 
		printf("*");
	printf("\n");
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
	
	
