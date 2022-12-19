
/* post-process -- load a saved file, do the stuff */

/* this is the conversion routine to convert from raw data
 * saved to disk by the demon process into the final output
 * data types - snap, timex, variance. stacks are done
 * by the demon directly.
 
 * $Log: postproc.c,v $
 * Revision 1.13  2011/11/10 00:56:53  root
 * added split long ago, updates for format 7 handling dc1394
 *
 * Revision 1.12  2010/11/16 18:48:03  stanley
 * added dark code
 *
 * Revision 1.11  2010/09/12 13:03:44  stanley
 * fixed flags for selected products, detecting deBayer
 *
 * Revision 1.10  2010/06/09 20:16:52  stanley
 * changed process to depend on rawOrder instead of knowing all the cameras
 *
 * Revision 1.9  2010/04/02 19:12:53  stanley
 * mostly format 7 (subimage stuffed into full) changes
 *
 * Revision 1.8  2009/02/20 21:58:22  stanley
 * casted data and ptr to remove sign complaints
 *
 * Revision 1.7  2009/02/20 21:17:02  stanley
 * added dobright, command line options
 *
 * Revision 1.6  2007/08/29 01:07:12  stanley
 * changed raw header format to text
 *
 * Revision 1.5  2006/04/14 23:35:51  stanley
 * added scorpion to debayer list
 *
 * Revision 1.4  2005/08/11 00:12:47  stanley
 * added camera number modifier-- masquerade
 *
 * Revision 1.3  2004/12/17 21:08:35  stanley
 * moved debayer test, added text in image
 *
 * Revision 1.2  2004/12/06 21:50:04  stanley
 * added more cameras to the deBayer list
 *
 * Revision 1.1  2004/03/17 00:56:11  stanley
 * Initial revision
 *
 *
 */
 

/* added a flag, -P for "products". -Pb says "do brightest". Default
 * is -Pstvb (all four known types). We're also assuming you always
 * do the stv part, so -Pb is the same as -Pstvb. This may change
 * as this code matures.
 */

 static char * RCSId = "$Id: postproc.c 446 2019-04-05 01:35:43Z stanley $";

#define _MAIN_

#include "demon2.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <png.h>

#define max(a,b) ((a)>(b) ? (a) : (b))
#define min(a,b) ((a)>(b) ? (b) : (a))
#define stdDev(sum,sq,n) (sqrt((n)*(double)(sq) - (double)(sum)*(double)(sum)) / (n))

#define to16bit	for( ii=0; ii<ih.length; ii++ ) u16DataPtr[ii] = data[ii]


double deBayer( struct _pixel_ *, uint16_t *, struct _cameraModule_ * );


unsigned char *readl( int fd )
{
static unsigned char line[1024];
int i=0;

	while( i<sizeof(line)-10 ) {
		read( fd, &line[i], 1 );
		if( '\n' == line[i] ) break;
		i++;
	}
	line[i] = 0;
	return line;
}

void doHelp() {
    printf("Usage: postproc [opts] rawfile\n");
    printf(" Opts: -p == ouput PNG (very slow)\n");
    printf("       -s == split raw into individual images\n");
    printf("       -q == do it quietly\n");
    printf("       -j nn == use jpeg compression quality nn percent \n");
    printf("       -B xxxx == set Bayer pattern xxxx (e.g. RGGB)\n");
    printf("       -c n == output files using camera number n\n");
    printf("       -Px == output only product x, where x is one or more of:\n");
    printf("           s -- snap  t -- timex  v -- variance  b -- bright  d -- dark\n");
}

int main( int argc, char **argv )
{

char outSnap[128];
char outTimex[128];
char outVar[128];
char outBright[128];
char outDark[128];

int doSplit = 0;
int doSnap = 1;
int doTimex = 1;
int doVar = 1;
int doBright = 1;
int doDark = 1;

struct smalltimeval frameTime;
struct _imageHeader_ ih;
long ihsize; /* old is shorter. */

unsigned char *data;
uint16_t      *u16DataPtr;
/*unsigned*/ char *ptr;
struct _pixel_ *RGB;
struct _pixel_ *FullRGB;
unsigned long fullSize;

time_t baseEpoch;
unsigned long *summ;
unsigned long *sq;
unsigned long *summPtr;
unsigned long *sqPtr;
unsigned char *dataPtr;
uint16_t  *dataPtru16;
uint16_t  *brightPtr;
uint16_t  *bright;
uint16_t  *darkPtr;
uint16_t  *dark;

unsigned long inDataBytes;
unsigned long pixCount;
unsigned long pixBytes;
unsigned long i;
unsigned long frameCount = 1;

double lastTime;
double firstTime;
double period;

int j;
int ii;
int index;
int baseIndex;
int in;
int count;

int camnum = -1;
char line[1024];
char fileName[128];
struct timeval xxstart;
struct timeval xxend;
long ltemp;
int doDeBayer = 0;

double maxSD = 0;
int beNoisy = 1;
int isPNG = 0;
char format[32];

int qual = 80;

char rawOrder[5];

rawOrder[0] = rawOrder[4] = 0;

printf( "called as: %s\n", argv[0] );

strcpy( format, "jpg" );

if( !strcmp(argv[0], "postsplit" )) {
	doSplit++;
}

for( j=1; j<argc; j++ ) {

	if( 0 == argv[j] ) { doHelp(); exit; }

	if( !strcmp( argv[j], "-q" ) ) {
		beNoisy = 0;
		continue;
	}

	if( !strcmp( argv[j], "-s" ) ) {
		doSplit++;
		printf("changing to split mode...\n");
		continue;
	}

	if( !strncmp( argv[j], "-j", 2 ) ) {
		qual = atoi( argv[j]+2 );
		printf( "qual changed to %d\n", qual );
		continue;
	}

	if( !strncmp( argv[j], "-c", 2 ) ) {
		camnum = atoi( argv[j]+2 );
		printf( "using camera number %d\n", camnum );
		continue;
	}

	if( !strncmp( argv[j], "-p", 2 ) ) {
        isPNG++;
        strcpy( format, "png" );
		printf( "will save PNG\n" );
		continue;
	}

	if( !strncmp( argv[j], "-h", 2 ) ) {
        doHelp();
		continue;
	}

	if( !strncmp( argv[j], "-B", 2 ) ) {
		strncpy( rawOrder, argv[j]+2, 4 );
		printf( "using raw order %s\n", rawOrder );
		continue;
	}

	if( !strncmp( argv[j], "-P", 2 ) ) {
		ptr = argv[j]+2;
		doDark = doBright = doVar = doSnap = doTimex = 0;
		while( *ptr ) {

			switch( *ptr ) {

				case 'd':
					if( beNoisy )
						printf("doDark turned on\n");
					doDark++;
					break;
				case 'b':
					if( beNoisy )
						printf("doBright turned on\n");
					doBright++;
					break;
				case 's':
					if( beNoisy )
						printf("doSnap turned on\n");
					doSnap++;
					break;
				case 't':
					if( beNoisy )
						printf("doTimex turned on\n");
					doTimex++;
					break;
				case 'v':
					if( beNoisy )
						printf("doVar turned on\n");
					doVar++;
					break;
				default: 
					if( beNoisy )
						printf("unknown product code: %c\n",
							*ptr );
			}
			ptr++;
		}
		continue;
	}

	/* if we are splitting, ignore other product flags */
	if( doSplit ) {
		doTimex = doBright = doDark = doVar = 0;
	}

	xxstart.tv_sec = xxstart.tv_usec = 0;
	xxend.tv_sec = xxend.tv_usec = 0; 

	printf("loading file %s\n", argv[j] );

	in = open( argv[j], O_RDONLY );
	if( in == -1 ) {
		perror("open input");
		continue;
	}
	
	count = read( in, &ltemp, sizeof(ltemp) );
	if( count != sizeof(ltemp) ) {
		printf("error reading hotmparams length\n");
		continue;
	}

	/* set ih size here */
	ihsize = sizeof( ih ) - 2 * sizeof(unsigned long);

	if( ltemp < 100000 ) {  /* should be old format */

		   count = read( in, &hotmParams, ltemp );
		   if( count != ltemp ) {
			   printf("error reading hotmParams from input\n");
			   continue;
		   }

		   count = read( in, &ltemp, sizeof(ltemp) );
		   if( count != sizeof(ltemp) ) {
			   printf("error reading cameraModule length\n");
			   continue;
		   }
		   count = read( in, &cameraModule, ltemp );
		   if( count != ltemp ) {
			   printf("error reading cameraModule from input\n" );
			   continue;
		   }

	  	/* set ih size here */
		ihsize = sizeof( ih ) - 2 * sizeof(unsigned long);
		ih.encoding = 0;
		ih.length = cameraModule.bufferLength;

	}
	else {  /* assume new format, ascii header */

#define ifis(x) if( ptr=(char *)&data[strlen(x)+2], \
		!strncasecmp( (char *)data, x, strlen(x) ) ) \
		
		memset( &cameraModule, 0, sizeof(cameraModule) );
		memset( &hotmParams, 0, sizeof(hotmParams) );	

		while( data = readl( in ) ) {

			ifis("end") break;
			ifis("bufferlength") {
				cameraModule.bufferLength = atol(ptr);
			}
			ifis("cameraformat") {
				cameraModule.format = atol(ptr); 
			}
			ifis("cameraname") {
				strcpy( cameraModule.module, ptr); 
			}
			ifis("rawOrder") {
				if( !rawOrder[0] ) 
					strcpy( cameraModule.rawOrder, ptr );
			}
			ifis("cameraX") {
				cameraModule.x = atol(ptr);
			}
			ifis("cameraY") {
				cameraModule.y = atol(ptr);
			}
			ifis("cameraTop") {
				cameraModule.top = atol(ptr);
			}
			ifis("cameraLeft") {
				cameraModule.left = atol(ptr);
			}
			ifis("cameraWidth") {
				cameraModule.cameraWidth = atol(ptr);
			}
			ifis("cameraHeight") {
				cameraModule.cameraHeight = atol(ptr);
			}
			ifis("maxY") {
				cameraModule.maxY = atol(ptr);
			}
			ifis("maxX") {
				cameraModule.maxX = atol(ptr);
			}
			ifis("cameraNumber") {
				cameraModule.cameraNumber = atol(ptr);
			}
			ifis("sitename") {
				strcpy( hotmParams.sitename, ptr );
			}
			ifis("programversion") {
				strcpy( hotmParams.programVersion, ptr );
			}
			ifis("frameCount") {
				cameraModule.frameCount = atol(ptr);
			}
			ifis("gain") {
				cameraModule.realGain = atof(ptr);
			}
			ifis("intTime") {
				cameraModule.intTime = atof(ptr);
			}
			ifis("cameraShutter") {
				cameraModule.shutter = atol(ptr);
			}
			ifis("cameraGain") {
				cameraModule.gain = atol(ptr);
			}

		}

		if( cameraModule.x == 0 ) cameraModule.x = cameraModule.cameraWidth;
		if( cameraModule.y == 0 ) cameraModule.y = cameraModule.cameraHeight;
	  	/* set ih size here */
		ihsize = sizeof( ih );

	}

	if( camnum > -1 ) 
		cameraModule.cameraNumber = camnum;
        
        printf("%ld %ld %ld %ld %ld %ld %ld %ld %ld\n", 
               cameraModule.x,
               cameraModule.y,
               cameraModule.top,
               cameraModule.left,
               cameraModule.cameraWidth,
               cameraModule.cameraHeight,
               cameraModule.maxX,
               cameraModule.maxY,
		cameraModule.bufferLength);

	if( rawOrder[0] ) strncpy( cameraModule.rawOrder, rawOrder, 4 );

	FullRGB = NULL;
	/* format 7, image area smaller than normal, insert */
	if( (cameraModule.x < cameraModule.cameraWidth)
	  || (cameraModule.y < cameraModule.cameraHeight) )  {
		fullSize = cameraModule.cameraWidth 
			* cameraModule.cameraHeight;
		FullRGB = calloc( fullSize, sizeof( struct _pixel_ ) );
	}
	
	pixCount = cameraModule.x * cameraModule.y;
	inDataBytes = cameraModule.bufferLength;	

	data = malloc( inDataBytes );
	u16DataPtr = malloc( 2*inDataBytes ); /* convert byte data to 18 bit that 
		                                 deBayer wants. */
	RGB = malloc( (pixCount) * sizeof(struct _pixel_) );
	if( doTimex || doVar ) {
		summ = malloc( inDataBytes * sizeof( unsigned long ) );
		sq = malloc( inDataBytes * sizeof( unsigned long ) );
	}
	if( doBright ) 
		bright = malloc( inDataBytes*2 );
	if( doDark ) {
		dark = malloc( inDataBytes*2 );
		if( NULL == dark ) {
			printf("failed to mallog dark\n:" );
			exit;
		}
	}	


TOPSPLIT:

	read( in, &ih, ihsize );
	frameTime = ih.when;

	/* encoded data? No! */
	if( ih.encoding ) {
		printf("cannot deal with encoded data!\n" );
		exit;
	}
	
	baseEpoch = frameTime.tv_sec;
	firstTime = 1.0 * frameTime.tv_sec + frameTime.tv_usec/1000000.0;
	count = read( in, data, ih.length );
	to16bit;

	/* special conversion, deBayer the frame  */
#ifdef OLDWAY
	if( !strcmp( cameraModule.module, "MicroPix" )
	 || !strcmp( cameraModule.module, "1394" ) 
	 || !strcmp( cameraModule.module, "Flea" )
	 || !strcmp( cameraModule.module, "Dragonfly" )
	 || !strcmp( cameraModule.module, "Dragonfly2 DR2-HICOL" )
	 || !strncmp( cameraModule.module, "Scorpion", strlen("Scorpion") ) )
#else
       if( strcmp( cameraModule.rawOrder, "YYYY" ) )
#endif
		doDeBayer = 1;
	
	/* special conversion, deBayer the frame for snap */
	if( doDeBayer ) 
		deBayer( RGB, u16DataPtr, &cameraModule );
	else
	    /* special conversion, for snap */
	    whateverToRGB( RGB, data );
	
	if( FullRGB ) {
             printf("stuffing into full\n");
             baseIndex = 0;
		for( i=cameraModule.top; i<cameraModule.y+cameraModule.top; i++ ) {
			index = i*cameraModule.cameraWidth + cameraModule.left - 1;
			for( j=cameraModule.left; j<cameraModule.x+cameraModule.left; j++ ) { 
				FullRGB[index++] = RGB[baseIndex++];
			}
		}
	}
				

	if( doSplit ) {
	     sprintf( fileName, "%u.%03u.c%ld.snap.%s", 
			frameTime.tv_sec, frameTime.tv_usec/1000,
			cameraModule.cameraNumber, format );
	} else {
	     sprintf( fileName, "%ld.c%ld.snap.%s", 
			baseEpoch,
			cameraModule.cameraNumber, format );
	}
			
	sprintf( line, 
			"%-s %-6s %-10s %s%-7s F: %s                     ",
			hotmParams.programVersion,
			"snap",
			hotmParams.sitename,
			ctime( &baseEpoch ),
			getenv("TZ"),
			fileName );
			
	baseEpoch++;
		
	line[cameraModule.cameraWidth/8]=0;
	
	if( FullRGB ) 
		insertText( 0, FullRGB, line );
	else
		insertText( 0, RGB, line );

	sprintf( line,
			"C: %ld S: %4d Qual: %3d Gain: %ld (%.2fdB) Shutter: %ld IntTime: %lf Name: %s                ",
			cameraModule.cameraNumber,
			1,
			80,
			cameraModule.gain,
			cameraModule.realGain,
			cameraModule.shutter,
			cameraModule.intTime,
			cameraModule.module );
	line[cameraModule.cameraWidth/8]=0;

	if( FullRGB ) {
		insertText( (cameraModule.cameraHeight/8-1), FullRGB, line );
		if( doSnap ) 
            isPNG ? (void) savePNG( FullRGB, fileName, 8 ) 
                  : save24( FullRGB, fileName, qual );
	}
	else {
		insertText( (cameraModule.y/8-1), RGB, line );
		if( doSnap ) 
            isPNG ? (void) savePNG( RGB, fileName, 8 )
                  : save24( RGB, fileName, qual );
	}
	
	if( doSplit && cameraModule.frameCount-- ) {
		printf("\r%05ld %u.%06u...", 
			frameCount, 
			frameTime.tv_sec, 
			frameTime.tv_usec );
		fflush(NULL);
		frameCount++;
		goto TOPSPLIT;
	}
	if( doSplit ) return(0);

	if( doSnap ) printf("%.6f (%u.%06u) snap saved %s\n", 
			firstTime,
			frameTime.tv_sec, 
			frameTime.tv_usec,
			fileName);
	
	if( cameraModule.frameCount == 1 ) {
		free(data); data = NULL;
		free(u16DataPtr); u16DataPtr = NULL;
		free(RGB); RGB = NULL;
		free(summ); summ = NULL;
		free(sq); sq = NULL;
		free(FullRGB); FullRGB = NULL; 
		continue;
	}

	if( gettimeofday( &xxstart, NULL ) ) 
		perror("get time start");

	if( doTimex || doVar ) {
		summPtr = summ; sqPtr = sq; dataPtr = data;
	}
	if( doBright ) 
		brightPtr = bright;
	if( doDark ) 
		darkPtr = dark;
	
	for( i=0; i<inDataBytes; i++ ) {
		if( doTimex || doVar ) {
			*sqPtr++ = *dataPtr * *dataPtr;
			*summPtr++ = *dataPtr;
		}
		if( doBright ) 
			*brightPtr++ = *dataPtr;
		if( doDark ) 
			*darkPtr++ = *dataPtr;

		dataPtr++;
	}
	
	while( read( in, &ih, ihsize ) ) {
		frameTime = ih.when;
		read( in, data, ih.length );
		summPtr = summ; sqPtr = sq; 
		dataPtr = data; brightPtr = bright;
		darkPtr = dark;
		for( i=0; i<inDataBytes; i++ ) {
			if( doTimex || doVar ) {
				*sqPtr++ += *dataPtr * *dataPtr;
				*summPtr++ += *dataPtr;
			}
			if( doBright ) {
				if (*dataPtr > *brightPtr)
					*brightPtr = *dataPtr;
				brightPtr++; 
			}
			if( doDark ) {
				if (*dataPtr < *darkPtr)
					*darkPtr = *dataPtr;
				darkPtr++; 
			}
			dataPtr++; 
		}
		frameCount++;
		if(beNoisy) { 
			printf("\r%05ld %u.%06u...", 
				frameCount, 
				frameTime.tv_sec, 
				frameTime.tv_usec );
			fflush(NULL);
		}
	}
	
	/*printf("\nread %d frames from file, should have %d\n", 
		frameCount, cameraModule.frameCount ); */

	lastTime = 1.0 * frameTime.tv_sec + frameTime.tv_usec/1000000.0;
	period = (lastTime - firstTime) / (frameCount - 1);
	summPtr = summ; sqPtr = sq; dataPtru16 = u16DataPtr;	
	for( i=0; i<inDataBytes; i++ ) {
		*dataPtru16++ = *summPtr++ / frameCount;
	}
	/* special conversion, deBayer the frame  */
	if( doDeBayer ) 
		deBayer( RGB, u16DataPtr, &cameraModule );
	else
	    /* special conversion */
		whateverToRGB( RGB, data );

	if( FullRGB ) {
		baseIndex = 0;
		for( i=cameraModule.top; i<cameraModule.y+cameraModule.top; i++ ) {
			index = i*cameraModule.cameraWidth + cameraModule.left - 1;
			for( j=cameraModule.left; j<cameraModule.x+cameraModule.left; j++ ) { 
				FullRGB[index++] = RGB[baseIndex++];
			}
		}
	}
				
		
	sprintf( fileName, "%ld.c%ld.timex.%s", 
			baseEpoch,
			cameraModule.cameraNumber, format );
			
	sprintf( line, 
			"%-s %-6s %-10s %s%-7s F: %s                       ",
			hotmParams.programVersion,
			"timex",
			hotmParams.sitename,
			ctime( &baseEpoch ),
			getenv("TZ"),
			fileName );
	baseEpoch++;
		
	line[cameraModule.cameraWidth/8]=0;

	if( FullRGB )
		insertText( 0, FullRGB, line );
	else	
		insertText( 0, RGB, line );

	sprintf( line,
			"C: %ld S: %4ld Q: %3d Per: %.3f G: %ld (%.2fdB) Sh: %ld Int: %lf N: %s                 ",
			cameraModule.cameraNumber,
			cameraModule.frameCount,
			qual,
			period,
			cameraModule.gain,
			cameraModule.realGain,
			cameraModule.shutter,
			cameraModule.intTime,
			cameraModule.module  );
	line[cameraModule.cameraWidth/8]=0;


	if( FullRGB ) {
		insertText( (cameraModule.cameraHeight/8-1), FullRGB, line );
		if( doTimex ) 
            isPNG ? (void) savePNG( FullRGB, fileName, 8 )
                  : save24( FullRGB, fileName, qual );
	}
	else {
		insertText( (cameraModule.cameraHeight/8-1), RGB, line );
		if( doTimex ) 
            isPNG ? (void) savePNG( RGB, fileName, 8 )
                  : save24( RGB, fileName, qual );
	}

	printf( "%.6f (%u.%06u) timex saved %s\n", 
			firstTime,
			frameTime.tv_sec, 
			frameTime.tv_usec,
			fileName);

	
	/* start variance image processing */
	summPtr = summ; sqPtr = sq; dataPtru16 = u16DataPtr;	
	for( i=0; i<inDataBytes; i++ ) {
		maxSD = max( maxSD, stdDev( *summPtr, *sqPtr, frameCount ) ) ;
		summPtr++; sqPtr++;
	}
	maxSD = 255 / maxSD;
	summPtr = summ; sqPtr = sq; dataPtru16 = u16DataPtr;	
	for( i=0; i<inDataBytes; i++ ) {
		*dataPtru16++ = maxSD * stdDev( *summPtr, *sqPtr, frameCount );
		summPtr++; sqPtr++;
	}
	if( cameraModule.format == formatYUV422 ) {
		for( dataPtru16=u16DataPtr, i=0; i<inDataBytes; i+=4 ) {
			*dataPtru16 = *dataPtru16/2 + 128; dataPtru16++; dataPtru16++;
			*dataPtru16 = *dataPtru16/2 + 128; dataPtru16++; dataPtru16++;
		}
	}

	/* special conversion, deBayer the frame  */
	if( doDeBayer ) 
		deBayer( RGB, u16DataPtr, &cameraModule );
	else
	    /* special conversion */
		whateverToRGB( RGB, data );

	if( FullRGB ) {
		baseIndex = 0;
		for( i=cameraModule.top; i<cameraModule.y+cameraModule.top; i++ ) {
			index = i*cameraModule.cameraWidth + cameraModule.left - 1;
			for( j=cameraModule.left; j<cameraModule.x+cameraModule.left; j++ ) { 
				FullRGB[index++] = RGB[baseIndex++];
			}
		}
	}
				
	
	sprintf( fileName, "%ld.c%ld.var.%s", 
			baseEpoch,
			cameraModule.cameraNumber, format );
			
	sprintf( line, 
			"%-s %-6s %-10s %s%-7s F: %s                       ",
			hotmParams.programVersion,
			"stdDev",
			hotmParams.sitename,
			ctime( &baseEpoch ),
			getenv("TZ"),
			fileName );
		
	line[cameraModule.cameraWidth/8]=0;
	
	if( FullRGB ) {
		insertText( 0, FullRGB, line );
	}
	else {
		insertText( 0, RGB, line );
	}


	sprintf( line,
			"C: %ld S: %4ld Q: %3d Per: %.3f Sc: %.3f G: %ld (%.2fdB) S: %ld Int: %lf N: %s                    ",
			cameraModule.cameraNumber,
			frameCount,
			qual,
			period,
			maxSD,
			cameraModule.gain,
			cameraModule.realGain,
			cameraModule.shutter,
			cameraModule.intTime,
			cameraModule.module  );

	line[cameraModule.cameraWidth/8]=0;

	if( FullRGB ) {
		insertText( (cameraModule.cameraHeight/8-1), FullRGB, line );
		if( doVar ) 
            isPNG ? (void) savePNG( FullRGB, fileName, 8 )
                  : save24( FullRGB, fileName, qual );
	}
	else {
		insertText( (cameraModule.cameraHeight/8-1), RGB, line );
		if( doVar ) 
            isPNG ? (void) savePNG( RGB, fileName, 8 )
                  : save24( RGB, fileName, qual );
	}


	printf( "%.6f (%u.%06u) var saved %s\n", 
			firstTime,
			frameTime.tv_sec, 
			frameTime.tv_usec,
			fileName);


	baseEpoch++;	

	/* bright */

if( doBright ) {

	if( doDeBayer ) 
		deBayer( RGB, bright, &cameraModule );
	else
	    /* special conversion */
		whateverToRGB( RGB, (unsigned char *)bright );

	if( FullRGB ) {
		baseIndex = 0;
		for( i=cameraModule.top; i<cameraModule.y+cameraModule.top; i++ ) {
			index = i*cameraModule.cameraWidth + cameraModule.left - 1;
			for( j=cameraModule.left; j<cameraModule.x+cameraModule.left; j++ ) { 
				FullRGB[index++] = RGB[baseIndex++];
			}
		}
	}
				
	
	sprintf( fileName, "%ld.c%ld.bright.%s", 
			baseEpoch,
			cameraModule.cameraNumber, format );
			
	sprintf( line, 
			"%-s %-6s %-10s %s%-7s F: %s                   ",
			hotmParams.programVersion,
			"bright",
			hotmParams.sitename,
			ctime( &baseEpoch ),
			getenv("TZ"),
			fileName );
		
	line[cameraModule.cameraWidth/8]=0;
	
	if( FullRGB ) {
		insertText( 0, FullRGB, line );
	}
	else {
		insertText( 0, RGB, line );
	}

	sprintf( line,
			"C: %ld S: %4ld Q: %3d Per: %.3f          G: %ld (%.2fdB) S: %ld Int: %lf N: %s                   ",
			cameraModule.cameraNumber,
			frameCount,
			qual,
			period,
			cameraModule.gain,
			cameraModule.realGain,
			cameraModule.shutter,
			cameraModule.intTime,
			cameraModule.module  );
	line[cameraModule.cameraWidth/8]=0;

	if( FullRGB ) {
		insertText( (cameraModule.cameraHeight/8-1), FullRGB, line );
		isPNG ? (void) savePNG( FullRGB, fileName, 8 )
              : save24( FullRGB, fileName, qual );
	}
	else {
		insertText( (cameraModule.cameraHeight/8-1), RGB, line );
		isPNG ? (void) savePNG( RGB, fileName, 8 )
              : save24( RGB, fileName, qual );
	}

	printf( "%.6f (%u.%06u) bright saved %s\n", 
			firstTime,
			frameTime.tv_sec, 
			frameTime.tv_usec,
			fileName);
	
	baseEpoch++;	
}

if( doDark ) {

	if( doDeBayer ) 
		deBayer( RGB, dark, &cameraModule );
	else
	    /* special conversion */
		whateverToRGB( RGB, (unsigned char *) dark );

	if( FullRGB ) {
		baseIndex = 0;
		for( i=cameraModule.top; i<cameraModule.y+cameraModule.top; i++ ) {
			index = i*cameraModule.cameraWidth + cameraModule.left - 1;
			for( j=cameraModule.left; j<cameraModule.x+cameraModule.left; j++ ) { 
				FullRGB[index++] = RGB[baseIndex++];
			}
		}
	}
				
	
	sprintf( fileName, "%ld.c%ld.dark.%s", 
			baseEpoch,
			cameraModule.cameraNumber, format );
			
	sprintf( line, 
			"%-s %-6s %-10s %s%-7s F: %s                   ",
			hotmParams.programVersion,
			"dark",
			hotmParams.sitename,
			ctime( &baseEpoch ),
			getenv("TZ"),
			fileName );
		
	line[cameraModule.cameraWidth/8]=0;
	
	if( FullRGB ) {
		insertText( 0, FullRGB, line );
	}
	else {
		insertText( 0, RGB, line );
	}

	sprintf( line,
			"C: %ld S: %4ld Q: %3d Per: %.3f          G: %ld (%.2fdB) S: %ld Int: %lf N: %s                   ",
			cameraModule.cameraNumber,
			frameCount,
			qual,
			period,
			cameraModule.gain,
			cameraModule.realGain,
			cameraModule.shutter,
			cameraModule.intTime,
			cameraModule.module  );
	line[cameraModule.cameraWidth/8]=0;

	if( FullRGB ) {
		insertText( (cameraModule.cameraHeight/8-1), FullRGB, line );
		isPNG ? (void) savePNG( FullRGB, fileName, 8 ) 
              : save24( FullRGB, fileName, qual );
	}
	else {
		insertText( (cameraModule.cameraHeight/8-1), RGB, line );
		isPNG ? (void) savePNG( RGB, fileName, 8 )
              : save24( RGB, fileName, qual );
	}

	printf( "%.6f (%u.%06u) dark saved %s\n", 
			firstTime,
			frameTime.tv_sec, 
			frameTime.tv_usec,
			fileName);
	
}

	if( gettimeofday( &xxend, NULL ) ) 
		perror("get time end");
	
	printf( "%ld frames over %f seconds ",
		frameCount,
		lastTime-firstTime );
		
	firstTime = 1.0 * xxstart.tv_sec + (xxstart.tv_usec/1000000.0);		
	lastTime = 1.0 * xxend.tv_sec + (xxend.tv_usec/1000000.0);		
		
	printf( "processed in %f seconds\n",
		lastTime-firstTime );			

	free(data); data = NULL;
	free(u16DataPtr); u16DataPtr = NULL;
	free(RGB); RGB = NULL;
	free(summ); summ = NULL;
	free(sq); sq = NULL;
	free(FullRGB); FullRGB = NULL;
	fflush(NULL);
}

}
