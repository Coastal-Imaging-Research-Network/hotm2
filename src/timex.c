/* timex.c -- the time exposure/std Dev module */

/* 
 *  $Log: timex.c,v $
 *  Revision 1.6  2003/05/01 21:49:05  stanley
 *  changed bottom text line, shorter, better
 *
 *  Revision 1.5  2002/11/06 23:54:56  stanley
 *  added shutter etc to bottom lines
 *
 *  Revision 1.4  2002/08/09 00:37:18  stanleyl
 *  changed verbose paramters
 *
 *  Revision 1.3  2002/03/22 21:41:30  stanley
 *  initialize quality to 80
 *
 *  Revision 1.2  2002/03/16 02:13:07  stanley
 *  fix baseEpoch increment/inserted text time
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

static const char rcsid[] = "$Id: timex.c 172 2016-03-25 21:03:51Z stanley $";


#include "hotm.h"

#define VERBOSE(x) if(me->verbose & x)
#define VERB_INIT 1
#define VERB_PROCESS 2
#define VERB_SAVE 4

#define max(a,b) ((a)>(b) ? (a) : (b))
#define min(a,b) ((a)>(b) ? (b) : (a))
#define stdDev(sum,sq,n) (sqrt((n)*(double)(sq) - (double)(sum)*(double)(sum)) / (n))

struct {
	unsigned long *rsumm;
	unsigned long *gsumm;
	unsigned long *bsumm;
	unsigned long *rsq;
	unsigned long *gsq;
	unsigned long *bsq;
	
	struct _pixel_ *RGB;
	
	unsigned long index;
	unsigned long count;
	unsigned long size;
	int           showTopTextLine;
	int           showBottomTextLine;
	char          timexFilename[128];
	long          timexQuality;
	int 	      doStdDev;	
} timexData;

void _saveResults( struct _processModule_ *me )
{
struct _pixel_ *data, *foo;
unsigned long i;
char line[132];
double maxSDr;
double maxSDg;
double maxSDb;
double temp;

	foo = data = (struct _pixel_ *)malloc( timexData.size * sizeof(struct _pixel_ ));
	
	/* this here is a timex */
	if( timexData.gsumm ) {
		for( i=0; i<timexData.size; i++ ) {
			foo->r = (timexData.rsumm[i] / timexData.count) & 0xff;
			foo->g = (timexData.gsumm[i] / timexData.count) & 0xff;
			foo->b = (timexData.bsumm[i] / timexData.count) & 0xff;
			foo++;
		}
	}
	else {
		for( i=0; i<timexData.size; i++ ) {
			foo->r = 
			foo->g = 
			foo->b = (timexData.rsumm[i] / timexData.count) & 0xff;
			foo++;
		}
	}
	
	
	sprintf( timexData.timexFilename, 
		"%d.c%d.timex.jpg", 
		hotmParams.baseEpoch,
		cameraModule.cameraNumber );
		
	VERBOSE(VERB_SAVE) {
		KIDID;
		printf("%s: creating file %s\n", 
				me->module, 
				timexData.timexFilename );
	}
	
	/* insert text */
	if( timexData.showTopTextLine ) {
		sprintf( line, 
			"%-s %-6s %-10s %s%-7s F: %s",
			hotmParams.programVersion,
			"timex",
			hotmParams.sitename,
			ctime( &hotmParams.baseEpoch ),
			getenv("TZ"),
			timexData.timexFilename );
		
		line[cameraModule.x/8]=0;
	
		insertText( 0, data, line );
	}

	hotmParams.baseEpoch++;
	
	if( timexData.showBottomTextLine ) {
		sprintf( line,
			"C: %d S: %4d P: %.3f Q: %3d Sk: %d G: %d Sh: %d Int: %lf N: %s",
			cameraModule.cameraNumber,
			timexData.count,
			cameraModule.sumDiff/cameraModule.frameCount,
			timexData.timexQuality,
			hotmParams.skip,
			cameraModule.gain,
			cameraModule.shutter,
			cameraModule.intTime,
			cameraModule.module  );
		line[cameraModule.x/8]=0;
		insertText( (cameraModule.y/8-1), data, line );
	}
			
	save24( data, timexData.timexFilename, timexData.timexQuality );
	
	/* now do the stddev, is we do it */
	if( NULL == timexData.rsq ) 
		return;
	
	/* scan for max SD, allow scaling of output */
	maxSDr = maxSDg = maxSDb = 0;	
	for( i=0; i<timexData.size; i++ ) {
		temp = stdDev( timexData.rsumm[i], 
				timexData.rsq[i],
				timexData.count );
		maxSDr = max( temp, maxSDr );
		if( timexData.gsumm ) {
			temp = stdDev( timexData.gsumm[i], 
					timexData.gsq[i],
					timexData.count );
			maxSDg = max( temp, maxSDg );
			temp = stdDev( timexData.bsumm[i], 
					timexData.bsq[i],
					timexData.count );
			maxSDb = max( temp, maxSDb );
		}
		else {
			maxSDb = maxSDg = maxSDr;
		}
	}
	foo = data;
	maxSDr = 255 / maxSDr;
	maxSDg = 255 / maxSDg;
	maxSDb = 255 / maxSDb;
	if( timexData.gsq ) {
		for( i=0; i<timexData.size; i++ ) {
			foo->r = maxSDr * stdDev( timexData.rsumm[i], 
					timexData.rsq[i],
					timexData.count );
			foo->g = maxSDg * stdDev( timexData.gsumm[i], 
					timexData.gsq[i],
					timexData.count );
			foo->b = maxSDb * stdDev( timexData.bsumm[i], 
					timexData.bsq[i],
					timexData.count );
			foo++;
		}
	}
	else {
		for( i=0; i<timexData.size; i++ ) {
			foo->r = 
			foo->g = 
			foo->b = maxSDr * stdDev( timexData.rsumm[i], 
					timexData.rsq[i],
					timexData.count );
			foo++;
		}
	}
	
	
	
	sprintf( timexData.timexFilename, 
		"%d.c%d.var.jpg", 
		hotmParams.baseEpoch,
		cameraModule.cameraNumber );

	VERBOSE(VERB_SAVE) {
		KIDID;
		printf("%s: creating file %s\n", 
				me->module, 
				timexData.timexFilename );
	}
			
	/* insert text */
	if( timexData.showTopTextLine ) {
		sprintf( line, 
			"%-s %-6s %-10s %s%-7s F: %s",
			hotmParams.programVersion,
			"stdDev",
			hotmParams.sitename,
			ctime( &hotmParams.baseEpoch ),
			getenv("TZ"),
			timexData.timexFilename );
		
		line[cameraModule.x/8]=0;
	
		insertText( 0, data, line );
	}

	hotmParams.baseEpoch++;
	
	if( timexData.showBottomTextLine ) {
		sprintf( line,
			"C: %d S: %4d P: %.3f Q: %3d Sc: %.3f/%.3f/%.3f Sk: %d G: %d S: %d Int: %lf N: %s",
			cameraModule.cameraNumber,
			timexData.count,
			cameraModule.sumDiff/cameraModule.frameCount,
			timexData.timexQuality,
			maxSDr, maxSDg, maxSDb,
			hotmParams.skip,
			cameraModule.gain,
			cameraModule.shutter,
			cameraModule.intTime,
			cameraModule.module  );
		line[cameraModule.x/8]=0;
		insertText( (cameraModule.y/8-1), data, line );
	}
			
	save24( data, timexData.timexFilename, timexData.timexQuality );
	
	
	
}


void _processFrame( struct _processModule_ *me, struct _frame_ *frame )
{
struct timeval start;
struct timeval end;
int err;
register unsigned char *ptr;
unsigned long i;
unsigned char r,g,b;
struct _pixel_ pix;

	err = gettimeofday( &start, NULL );
	if( err ) 
		perror( me->module );
	
	ptr = frame->dataPtr;
	
	/* convert to RGB if not already */
	if( timexData.RGB ) {
		whateverToRGB( timexData.RGB, ptr );
		ptr = (unsigned char *)timexData.RGB;
	}
	
	for( i = 0; i < timexData.size; i++ ) {
		r = *ptr++; g=*ptr++; b=*ptr++;
		timexData.rsumm[i] += r;
		if( timexData.gsumm ) {
			timexData.gsumm[i] += g;
			timexData.bsumm[i] += b;
		}
		if( timexData.rsq ) {
			timexData.rsq[i] += r * r;
			if( timexData.gsumm ) {
				timexData.bsq[i] += b * b;
				timexData.gsq[i] += g * g;
			}
		}
	}
	
	me->remaining--;
	timexData.count++;
	
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
char 			*module = "timex/stdDev";
struct _commandFileEntry_ *cmd;

	
	/* problem -- how do I find ME in the process list? */
	/* well, I will be the last member of the list! */
	me = firstProcessModule;
	while( me->next ) 
		me = me->next;
	
	strcpy( me->module, module );
	
	timexData.size = cameraModule.x * cameraModule.y;
	size = timexData.size * sizeof( unsigned long );

	timexData.count = 0;
	timexData.doStdDev = 1;

	getLongParam( &me->remaining, "timexFrames", module );
	getLongParam( &me->verbose, "timexVerbose", module );

	timexData.showTopTextLine =
	timexData.showBottomTextLine = 1;
	
	timexData.timexQuality = 80;

	getBooleanParam( &timexData.showTopTextLine, 
			"timexShowTopTextLine",
			module);
	getBooleanParam( &timexData.showBottomTextLine, 
			"timexShowBottomTextLine",
			module);
	getLongParam( &timexData.timexQuality,
			"timexQuality",
			module );
				
	VERBOSE(VERB_INIT) {
		KIDID; 
		printf("cam %d: %s: _init\n", 
				cameraModule.cameraNumber,
				module );
	}

	/* now some format specific stuff */
	timexData.rsumm = memoryLock( size, "rsumm", module );
	if( cameraModule.format < formatMONO ) {
		timexData.gsumm = memoryLock( size, "gsumm", module );
		timexData.bsumm = memoryLock( size, "bsumm", module );
	}
	getBooleanParam( &timexData.doStdDev, "timexDoStdDev", module );
	if( timexData.doStdDev ) {
		timexData.rsq = memoryLock( size, "rsq", module );
		if( cameraModule.format < formatMONO ) {
			timexData.bsq = memoryLock( size, "bsq", module );
			timexData.gsq = memoryLock( size, "gsq", module );
		}
	}
	
	if( cameraModule.format != formatRGB8 ) 
		timexData.RGB = (struct _pixel_ *)memoryLock( 
					timexData.size * sizeof(struct _pixel_),
					"RGB", 
					module );
					 
	me->processFrame = _processFrame;
	me->processPixel = NULL;
	me->closeFrame = NULL;
	me->saveResults = _saveResults;
	
}
	
	
