/* disk.c -- write data to disk
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

static const char rcsid[] = "$Id: disk.c 172 2016-03-25 21:03:51Z stanley $";

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hotm.h"

#define VERBOSE(x) if(me->verbose & x)
#define VERB_INIT 1
#define VERB_PROCESS 2
#define VERB_SAVE 4

struct _diskdata_ {
	int fd;
	unsigned long size;
} diskdata;

void _saveResults( struct _processModule_ *me )
{

	close(diskdata.fd);	
	
}


void _processFrame( struct _processModule_ *me, struct _frame_ *frame )
{
struct timeval start;
struct timeval end;
unsigned char *ptr;
int err;

	err = gettimeofday( &start, NULL );
	if( err ) 
		perror( me->module );
	
	ptr = frame->dataPtr;
	
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );

	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );

	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );

	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );
	write( diskdata.fd, frame->dataPtr, diskdata.size );

	me->remaining--;
	
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
	
	/* problem -- how do I find ME in the process list? */
	/* well, I will be the last member of the list! */
	me = firstProcessModule;
	while( me->next ) 
		me = me->next;
	
	strcpy( me->module, module );
	
	diskdata.size = cameraModule.x * cameraModule.y;
	size = diskdata.size * 2;

	getLongParam( &me->remaining, "diskFrames", module );
	getLongParam( &me->verbose, "diskVerbose", module );

	VERBOSE(VERB_INIT) {
		KIDID; 
		printf("cam %d: %s: _init\n", 
				cameraModule.cameraNumber,
				module );
	}

	me->processFrame = _processFrame;
	me->processPixel = NULL;
	me->closeFrame = NULL;
	me->saveResults = _saveResults;
	
	diskdata.fd = open( "testfile.dat", O_WRONLY|O_CREAT, S_IRWXU );
	if( diskdata.fd == -1 ) {
		perror("init disk");
		exit(-1);
	}
	
}
	
	
