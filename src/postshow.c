

/* postshow -- read data from a raw stream produced by hotm/demon and
   display */

/* started with postproc.c, v1.8, and uses the show.so module */

 static char * RCSId = "$Id: postshow.c 232 2016-10-06 22:57:50Z stanley $";

#define _MAIN_

#include "demon2.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <SDL/SDL.h>
#include <sys/time.h>

SDL_Event event;
                                                                                
struct {
        SDL_Surface     *screen;
        SDL_Surface     *image;
        SDL_Color        colors[256];
        int              modulo;
        int              isRGB;
} showData;

int is16bit;                    /* is camera producing 16 bit data? */

#define TEST16BIT  \
     if( (cameraModule.format == formatMONO16)\
        || (cameraModule.format == formatRGB16) )\
                is16bit = 1;\
     else is16bit = 0


double deBayer( struct _pixel_ *, uint16_t *, struct _cameraModule_ * );

void goaway( int j ) {
	printf("exiting at user request\n");
	exit(-1);
}

int main( int argc, char **argv ) {

struct smalltimeval frameTime;
struct _imageHeader_ ih;
long ihsize; /* old is shorter. */

struct {
	key_t key;
	int shmid;
	struct ihPTR {
		int sem;
		struct _imageHeader_ ih;
	} * ih;
} ihSHM;

struct {
	key_t key;
	int shmid;
	struct dataPTR {
		int sem;
		uint16_t data;
	} * data;
} dataSHM;

struct {
	key_t key;
	int shmid;
	struct _cameraModule_ *cm;
} cmSHM;


         char *ptr;
struct _pixel_ *RGB;
struct _pixel_ *RGB2;

time_t baseEpoch;
unsigned char *dataPtr;

uint16_t *tmpDataPtr;

unsigned long inDataBytes;
unsigned long pixCount;
unsigned long pixBytes;
unsigned long i;
unsigned long frameCount = 1;

double lastTime;
double firstTime;
double period;

int j;
int in;
int count;
int camnum = -1;
char line[128];
char fileName[128];
struct timeval xxstart;
struct timeval xxend;
long ltemp;
int doDeBayer = 0;
int modulo = 1;

double maxSD = 0;
int beNoisy = 1;

signal( SIGINT, &goaway );

for( j=0; ; j++ ) {

	if( 0 == argv[j] ) break;

	if( !strcmp( argv[j], "-q" ) ) {
		beNoisy = 0;
		continue;
	}

	if( !strncmp( argv[j], "-m", 2 ) ) {
		modulo = atoi( argv[j+1] );
		printf( "using modulo %d\n", modulo );
		j++;
		continue;
	}
	if( !strncmp( argv[j], "-c", 2 ) ) {
		camnum = atoi( argv[j+1] );
		printf( "using camera number %d\n", camnum );
		j++;
		continue;
	}

}

	xxstart.tv_sec = xxstart.tv_usec = 0;
	xxend.tv_sec = xxend.tv_usec = 0; 

	ihSHM.key = 0x100 + camnum;
	dataSHM.key = 0x200 + camnum;
	cmSHM.key = 0x300 + camnum;

	ihSHM.shmid = shmget( ihSHM.key, sizeof(struct ihPTR), 0644 ); 
	if( ihSHM.shmid == -1 ) {
		printf("Error getting shared memory segment. Did you\n");
		printf("send an openSHM to the camera?\n");
		perror("postshow: shmget header");
		exit(0);
	}
	ihSHM.ih = (struct ihPTR *) shmat( ihSHM.shmid, 0, 0 );

	cmSHM.shmid = shmget( cmSHM.key, sizeof(struct _cameraModule_), 0644 );
	cmSHM.cm = (struct _cameraModule_ *)shmat( cmSHM.shmid, 0, 0 );
	memcpy( &cameraModule, cmSHM.cm, sizeof(cameraModule));

	dataSHM.shmid = shmget( dataSHM.key, cameraModule.bufferLength, 0644 );
	dataSHM.data = (struct dataPTR *)shmat( dataSHM.shmid, 0, 0 );

	showData.isRGB = ( cameraModule.format == formatRGB8 );
	
	pixCount = cameraModule.x * cameraModule.y;
	inDataBytes = cameraModule.bufferLength;	
	
	/* if NOT input is 16 bits, must copy to 16 bits for debayer */
	/* sigh. stinkin' IR cameras */
	TEST16BIT;
	if( !is16bit ) 
		tmpDataPtr = (uint16_t *)calloc( pixCount * 2, 1 );

	RGB = malloc( pixCount * sizeof(struct _pixel_) );

        if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
                fprintf( stderr,
                                "%s: cannot init SDL: %s\n",
                                "postshow", SDL_GetError() );
                return -1;
        }
                                                                                
        showData.screen = SDL_SetVideoMode( cameraModule.x/modulo,
                                        cameraModule.y/modulo,
                                        0,
                                        SDL_SWSURFACE );
                                                                                
        for( i=0; i<256; i++ )
                showData.colors[i].r =
                showData.colors[i].g =
                showData.colors[i].b = i;
                                                                                
        if( NULL == showData.screen ) {
                fprintf( stderr, "%s: cannot video mode SDL\n", "postshow" );
		return -1;
        }
                                                                                
TOP:
	/* data sem is set last by demon, check first here. */
	/* if set, then ih is valid, too */
	while( 0 == dataSHM.data->sem ) usleep(5000);
	memcpy( &cameraModule, cmSHM.cm, sizeof(cameraModule));
	
	memcpy( &ih, &ihSHM.ih->ih, sizeof(ih));

	frameTime = ih.when;

	/* encoded data? No! */
	if( ih.encoding ) {
		printf("cannot deal with encoded data!\n" );
		return -1;
	}
	
	baseEpoch = frameTime.tv_sec;

	//printf("postshow: got epoch: %d size: %d\n",
	//	baseEpoch, ih.length );

	firstTime = 1.0 * frameTime.tv_sec + frameTime.tv_usec/1000000.0;

	/* NOT YYYY */
	if( strcmp( cameraModule.rawOrder, "YYYY" ) )
		doDeBayer = 1;
	
	/* special conversion, deBayer the frame for snap */
	if( doDeBayer )  {
		if( !is16bit ) {
			dataPtr = (uint8_t *)&dataSHM.data->data;
			for(i=0; i<pixCount; i++) 
				tmpDataPtr[i] = dataPtr[i];
			deBayer( RGB, tmpDataPtr, &cameraModule );
		}
		else
			deBayer( RGB, &dataSHM.data->data, &cameraModule );
	}
	else
	    /* special conversion, for snap */
	    whateverToRGB( RGB, (uint8_t *)&dataSHM.data->data );

	/* subsample */
	if( modulo>1 ) {

		RGB2 = RGB;
		for( i=0;i<cameraModule.y; i+=modulo ) 
			for( j=0; j<cameraModule.x-(modulo-1); j+=modulo ) 
				*RGB2++ = RGB[i*cameraModule.x+j];
	}
	
	sprintf( line,
			"C: %ld Gain: %.2fdB IntTime: %lf",
			cameraModule.cameraNumber,
			ih.gain, ih.shutter );
	line[cameraModule.x/8]=0;
	if( modulo < 5 ) {
		i = cameraModule.cameraWidth; cameraModule.cameraWidth /= modulo;
		insertText( 0, RGB, line );
		cameraModule.cameraWidth = i;
	}

	/* at this point, image is ready in RGB, now display */

	showData.image = SDL_CreateRGBSurfaceFrom( RGB,
                                                cameraModule.x/modulo,
                                                cameraModule.y/modulo,
                                                24,
				3*(cameraModule.x/modulo),
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

        sprintf( line, "Cam %ld: %13d.%06d",
                        cameraModule.cameraNumber,
                        frameTime.tv_sec,
                        frameTime.tv_usec);
        SDL_WM_SetCaption( line, line );
                                                                                
	dataSHM.data->sem = 0;
	ihSHM.ih->sem = 0;
                                                                                
	/* done displaying, do the next */
	goto TOP;

}
