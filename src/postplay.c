
/* postplay -- load a saved file, play on screen */

/*  postplay xxx.raw 
 *
 * $Log: postplay.c,v $
 * Revision 1.2  2011/11/10 01:00:05  root
 * added sys/time to deal with time functions
 *
 * Revision 1.1  2010/06/09 20:16:30  stanley
 * Initial revision
 *
 *
 */
 

 static char * RCSId = "$Id: postplay.c 236 2016-10-06 22:58:42Z stanley $";

#define _MAIN_

#include "demon.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <SDL/SDL.h>

SDL_Event event;
                                                                                
struct {
        SDL_Surface     *screen;
        SDL_Surface     *image;
        SDL_Color        colors[256];
        int              modulo;
        int              isRGB;
} showData;

double deBayer( struct _pixel_ *, unsigned char *, struct _cameraModule_ * );


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

int main( int argc, char **argv )
{

struct timeval frameTime;
struct _imageHeader_ ih;
long ihsize; /* old is shorter. */

unsigned char *data;
/*unsigned*/ char *ptr;
struct _pixel_ *RGB;
struct _pixel_ *FullRGB;
unsigned long fullSize;

time_t baseEpoch;

unsigned long inDataBytes;
unsigned long pixCount;
unsigned long pixBytes;
unsigned long i;
unsigned long frameCount = 1;

double lastTime;
double firstTime;
double period;

int j;
int index;
int baseIndex;
int in;
int count;

int camnum = -1;
char line[128];
char fileName[128];
struct timeval now;
struct timeval then;
struct timeval lastFrame;
struct timeval frameDiff;
struct timeval elapsed;
unsigned long sleepus;
long ltemp;
int doDeBayer = 0;

double maxSD = 0;
int beNoisy = 1;

int qual = 80;

for( j=1; j<argc; j++ ) {

	if( 0 == argv[j] ) exit;

	if( !strcmp( argv[j], "-q" ) ) {
		beNoisy = 0;
		continue;
	}

	timerclear(&lastFrame);

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
			ifis("cameraname") {
				strcpy( cameraModule.module, ptr); 
			}
			ifis("rawOrder") {
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

        if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
                fprintf( stderr,
                                "%s: cannot init SDL: %s\n",
                                "postshow", SDL_GetError() );
                return -1;
        }
                                                                                
        showData.screen = SDL_SetVideoMode( cameraModule.cameraWidth,
                                        cameraModule.cameraHeight,
                                        0,
                                        SDL_SWSURFACE );
                                                                                
        for( i=0; i<256; i++ )
                showData.colors[i].r =
                showData.colors[i].g =
                showData.colors[i].b = i;
                                                                                
        if( NULL == showData.screen ) {
                fprintf( stderr, "%s: cannot video mode SDL\n", "postplay" );
		return -1;
        }

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
	RGB = malloc( pixCount * sizeof(struct _pixel_) );

TOP:
	/* read the image header */
	count = read( in, &ih, ihsize );
	/* next file if EOF */
	if( count < ihsize ) continue;

	/* save this frame's time */
	frameTime = ih.when;

	/* get 'now' */
	gettimeofday( &now, NULL );

	/* if we've seen a 'last frame', i.e. been here once */
	if( lastFrame.tv_sec > 0 ) {
		timersub( &now, &then, &elapsed );
		timersub( &frameTime, &lastFrame, &frameDiff );
		if( timercmp( &elapsed, &frameDiff, < ) ) {
			sleepus = frameDiff.tv_sec*1000000+frameDiff.tv_usec;
			sleepus = sleepus - elapsed.tv_sec*1000000 - elapsed.tv_usec;
			usleep( sleepus );
			gettimeofday( &now, NULL );
		}
	}
	lastFrame = frameTime;		
	then = now;
		

	/* encoded data? No! */
	if( ih.encoding ) {
		printf("cannot deal with encoded data!\n" );
		exit;
	}
	
	baseEpoch = frameTime.tv_sec;
	firstTime = 1.0 * frameTime.tv_sec + frameTime.tv_usec/1000000.0;
	count = read( in, data, ih.length );

	/* special conversion, deBayer the frame  */
	if( !strcmp( cameraModule.module, "MicroPix" )
	 || !strcmp( cameraModule.module, "1394" ) 
	 || !strcmp( cameraModule.module, "Flea" )
	 || !strcmp( cameraModule.module, "Dragonfly" )
	 || !strcmp( cameraModule.module, "Dragonfly2 DR2-HICOL" )
	 || !strncmp( cameraModule.module, "Scorpion", strlen("Scorpion") ) )
		doDeBayer = 1;
	
	/* special conversion, deBayer the frame for snap */
	if( doDeBayer ) 
		deBayer( RGB, data, &cameraModule );
	else
	    /* special conversion, for snap */
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
	}
	else {
		insertText( (cameraModule.y/8-1), RGB, line );
	}

	if( beNoisy ) {

	showData.image = SDL_CreateRGBSurfaceFrom( 
						FullRGB? FullRGB : RGB,
                                                cameraModule.cameraWidth,
                                                cameraModule.cameraHeight,
                                                24,
				3*(cameraModule.cameraWidth),
                                                0xff,
                                                0xff00,
                                                0xff0000,
                                                0 );

        SDL_BlitSurface( showData.image, NULL, showData.screen, NULL );
        SDL_UpdateRect( showData.screen,
                                        0,
                                        0,
                                        showData.image->w,
                                        showData.image->h );

        sprintf( line, "Cam %ld: %13ld.%06ld",
                        cameraModule.cameraNumber,
                        frameTime.tv_sec,
                        frameTime.tv_usec);
        SDL_WM_SetCaption( line, line );
                                                                                

	}
	
	goto TOP;
}

}
