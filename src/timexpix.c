/* timex.c -- the time exposure/std Dev module */

#include "hotm.h"

struct {
	unsigned long *rsumm;
	unsigned long *gsumm;
	unsigned long *bsumm;
	unsigned long *rsq;
	unsigned long *gsq;
	unsigned long *bsq;
	
	unsigned long index;
	unsigned long count;
	unsigned long size;
} timexPixData;

void _processPixel( struct _processModule_ *me, 
			struct _pixel_ pix, 
			unsigned long ptr )
{
struct timeval start;
struct timeval end;
int err;

	err = gettimeofday( &start, NULL );
	if( err ) 
		perror( me->module );
	
		timexPixData.rsumm[ptr] += pix.data.r;
		timexPixData.gsumm[ptr] += pix.data.g;
		timexPixData.bsumm[ptr] += pix.data.b;
		timexPixData.rsq[ptr] += pix.data.r * pix.data.r;
		timexPixData.bsq[ptr] += pix.data.b * pix.data.b;
		timexPixData.gsq[ptr] += pix.data.g * pix.data.g;
	
	err = gettimeofday( &end, NULL );
	if( err ) {
		perror( me->module );
		me->processMicros = 9999999;
		return;
	}
	
	me->processMicros += (end.tv_sec - start.tv_sec) * 1000000
		+ (end.tv_usec - start.tv_usec );
		
	return;
}

void _processFrame( struct _processModule_ *me, struct _frame_ *pix )
{
	return;
}

void _closeFrame( struct _processModule_ *me )
{
	me->remaining--;
	return;
}

void _init()
{
unsigned long 		size;
struct _processModule_ 	*me;
char 			*module = "timex/stdDev pix";
struct _commandFileEntry_ *cmd;

	/* problem -- how do I find ME in the process list? */
	/* well, I will be the last member of the list! */
	me = firstProcessModule;
	while( me->next )
		me = me->next;
	
	strcpy( me->module, module );
	
	timexPixData.size = cameraModule.x * cameraModule.y;
	size = timexPixData.size * sizeof( unsigned long );
	
	timexPixData.count = 0;

	cmd = findCommand("timexFrames");
	me->remaining = getLong( cmd );
	tagCommand( cmd, module );
	
	cmd = findCommand("timexVerbose");
	me->verbose = getBoolean(cmd);
	tagCommand( cmd, module );

	timexPixData.rsumm = memoryLock( size, "rsumm", module );
	timexPixData.gsumm = memoryLock( size, "gsumm", module );
	timexPixData.bsumm = memoryLock( size, "bsumm", module );
	timexPixData.rsq = memoryLock( size, "rsq", module );
	timexPixData.bsq = memoryLock( size, "bsq", module );
	timexPixData.gsq = memoryLock( size, "gsq", module );
	
	me->processFrame = NULL;
	me->processPixel = _processPixel;
	me->closeFrame = _closeFrame;
	
}
	
	
