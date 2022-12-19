/* snapshot module */

/* 
 *  $Log: snap.c,v $
 *  Revision 1.7  2003/05/01 20:47:53  stanley
 *  changed bottom text line, simpler
 *
 *  Revision 1.6  2002/11/06 23:54:35  stanley
 *  added shutter etc to bottom line
 *
 *  Revision 1.5  2002/08/09 00:27:04  stanleyl
 *  changed init message
 *
 *  Revision 1.4  2002/08/08 23:31:12  stanleyl
 *  prettier init message
 *
 *  Revision 1.3  2002/04/29 16:56:53  stanley
 *  change verbose codes
 *
 *  Revision 1.2  2002/03/16 02:11:56  stanley
 *  fix baseEpoch increment/inserted text line
 *
 *  Revision 1.1  2002/03/15 21:57:00  stanley
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

static const char rcsid[] = "$Id: snap.c 172 2016-03-25 21:03:51Z stanley $";

#include "hotm.h"

#define VERBOSE(x) if(me->verbose & x)
#define VERB_SAVE 1
#define VERB_INIT 2

struct {
	int           showTopTextLine;
	int           showBottomTextLine;
	char          snapFilename[128];
	long          snapQuality;
	long          size;
	unsigned char *snap;
} snapData;

void _saveResults( struct _processModule_ *me )
{
unsigned long i;
char line[132];
struct _pixel_ *rgb;

	rgb = malloc( cameraModule.x * cameraModule.y * sizeof(struct _pixel_));
	if( NULL == rgb ) {
		printf("snap: cannot malloc rgb buffer!\n");
		return;
	}
	
	whateverToRGB( rgb, snapData.snap );

	sprintf( snapData.snapFilename, 
		"%d.c%d.snap.jpg", 
		hotmParams.baseEpoch,
		cameraModule.cameraNumber );
		
	VERBOSE(VERB_SAVE) {
		KIDID;
		printf("%s: creating file %s\n", "snap", snapData.snapFilename );
	}
	
	/* insert text */
	if( snapData.showTopTextLine ) {
		sprintf( line, 
			"%-s %-6s %-10s %s%-7s F: %s",
			hotmParams.programVersion,
			"snap",
			hotmParams.sitename,
			ctime( &hotmParams.baseEpoch ),
			getenv("TZ"),
			snapData.snapFilename );
		
		line[cameraModule.x/8]=0;
	
		insertText( 0, rgb, line );
	}

	hotmParams.baseEpoch++;
	
	if( snapData.showBottomTextLine ) {
		sprintf( line,
			"C: %d S: %4d Qual: %3d Gain: %d Shutter: %d IntTime: %lf Name: %s",
			cameraModule.cameraNumber,
			1,
			snapData.snapQuality,
			cameraModule.gain,
			cameraModule.shutter,
			cameraModule.intTime,
			cameraModule.module );
		line[cameraModule.x/8]=0;
		insertText( (cameraModule.y/8-1), rgb, line );
	}
			
	save24( rgb, snapData.snapFilename, snapData.snapQuality );
	
	
}


void _processFrame( struct _processModule_ *me, struct _frame_ *frame )
{
struct timeval start;
struct timeval end;
int err;

	err = gettimeofday( &start, NULL );
	if( err ) 
		perror( me->module );
	
	memcpy( snapData.snap, frame->dataPtr, cameraModule.bufferLength );
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
char 			*module = "snap";
struct _commandFileEntry_ *cmd;

	/* problem -- how do I find ME in the process list? */
	/* well, I will be the last member of the list! */
	me = firstProcessModule;
	while( me->next )
		me = me->next;
	
	strcpy( me->module, module );
	
	snapData.size = cameraModule.x * cameraModule.y;
	size = cameraModule.bufferLength;
	
	getLongParam( &me->verbose, "snapVerbose", module );

	snapData.showTopTextLine =
	snapData.showBottomTextLine = 1;

	getBooleanParam( &snapData.showTopTextLine, 
			"snapShowTopTextLine",
			module);
	getBooleanParam( &snapData.showBottomTextLine, 
			"snapShowBottomTextLine",
			module);
	getLongParam( &snapData.snapQuality,
			"snapQuality",
			module );
				
	VERBOSE(VERB_INIT) { 
		KIDID;
		printf("cam %d: %s: _init\n", 
			cameraModule.cameraNumber, module );
	}

	snapData.snap = (unsigned char *)memoryLock( size, "snap", module );

	me->processFrame = _processFrame;
	me->processPixel = NULL;
	me->closeFrame = NULL;
	me->saveResults = _saveResults;
	
	me->remaining = 1;
	
}
	
	
