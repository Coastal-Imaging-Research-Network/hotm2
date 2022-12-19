
/* running diff */

/* 
 *  $Log: runDiff.c,v $
 *  Revision 1.3  2011/11/10 01:25:46  root
 *  made module signed, fixed warning
 *
 *  Revision 1.2  2011/11/10 01:17:29  root
 *  fixed int underflow in difference
 *
 *  Revision 1.1  2011/11/10 01:03:35  root
 *  Initial revision
 *
 *  Revision 1.5  2009/02/20 21:18:05  stanley
 *  modulo, SDL fixes
 *
 *  Revision 1.3  2002/08/09 00:38:06  stanleyl
 *  stupid programmer cannot do bit patterns right!
 *
 *  Revision 1.2  2002/08/09 00:22:25  stanleyl
 *  changed verbose processing, changed init message
 *
 *  Revision 1.1  2002/08/08 21:19:18  stanleyl
 *  Initial revision
 *
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

static const char rcsid[] = "$Id: runDiff.c 231 2016-10-06 22:54:05Z stanley $";


#include "hotm.h"
#include <SDL/SDL.h>

#define VERBOSE(x) if(me->verbose & x)
#define VERB_INIT 1
#define VERB_PROCESS 2
#define VERB_SAVE 4

unsigned char * prevPtr;
unsigned char * result;
unsigned long prevLen;

struct {
	SDL_Surface     *screen;
	SDL_Surface	*image;
	SDL_Color	 colors[256];
	int 		 modulo;
	int		 isRGB;
} rdiffData;

SDL_Event event;

/* nothing to do for saving results other than killing SDL */

void _saveResults( struct _processModule_ *me )
{

	SDL_Quit;
	
}


void _processFrame( struct _processModule_ *me, struct _frame_ *frame )
{
struct timeval start;
struct timeval end;
int err;
register unsigned char *ptr;
unsigned long i;
int modulo;
char title[256];

	err = gettimeofday( &start, NULL );
	if( err ) 
		perror( me->module );
	
	ptr = frame->dataPtr;
	modulo = rdiffData.modulo;

	for( i=0; i<prevLen; i++ ) {
		result[i] = prevPtr[i]>ptr[i] ? 
				prevPtr[i] - ptr[i] :
				ptr[i]-prevPtr[i];
		prevPtr[i] = ptr[i];
	}

	if( rdiffData.isRGB )
		rdiffData.image = SDL_CreateRGBSurfaceFrom( ptr, 
						cameraModule.x,
						cameraModule.y, 
						24, 
						cameraModule.x*3, 
						0xff, 
						0xff00, 
						0xff0000, 
						0 );
	else {
		rdiffData.image = SDL_CreateRGBSurfaceFrom( result, 
						cameraModule.x/modulo,
						cameraModule.y/modulo, 
						8*modulo, 
						cameraModule.x*modulo, 
						0xff, 
						0xff, 
						0xff, 
						0 );
		SDL_SetPalette( rdiffData.image, 
				SDL_LOGPAL, 
				rdiffData.colors, 
				0, 256 );
	}
						
	SDL_BlitSurface( rdiffData.image, NULL, rdiffData.screen, NULL );
	SDL_UpdateRect( rdiffData.screen, 
					0, 
					0, 
					rdiffData.image->w, 
					rdiffData.image->h );
					
	err = gettimeofday( &end, NULL );
	if( err ) {
		perror( me->module );
		me->processMicros = 9999999;
		return;
	}
	
	me->processMicros = (end.tv_sec - start.tv_sec) * 1000000
		+ (end.tv_usec - start.tv_usec );


	sprintf( title, "Rdiff Cam %ld: %13ld.%06ld (%ld)", 
			cameraModule.cameraNumber,
			frame->when.tv_sec,
			frame->when.tv_usec,
			me->processMicros );
	SDL_WM_SetCaption( title, title );

	while( SDL_PollEvent( &event ) ) {

		switch( event.type ) {

			case SDL_VIDEORESIZE:
				rdiffData.screen = SDL_SetVideoMode( 
					event.resize.w,
					event.resize.h,
					0,
					SDL_SWSURFACE | SDL_RESIZABLE );
				fprintf(stderr, "resize event: %dx%d\n", 
					event.resize.w, event.resize.h);
				break;
		}
	}
		
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
char 			*module = "runDiff";
struct _commandFileEntry_ *cmd;
int i;
long  modulo;
long testing = 0;

	
	/* problem -- how do I find ME in the process list? */
	/* well, I will be the last member of the list! */
	me = firstProcessModule;
	while( me->next ) 
		me = me->next;
	
	strcpy( me->module, module );
	modulo = 2;
	
	getLongParam( &me->verbose, "rdiffverbose", module );
	getLongParam( &modulo, "rdiffmodulo", module );
	getLongParam( &testing, "rdifftesting", module );
	rdiffData.isRGB = (cameraModule.format == formatRGB8);

	if(rdiffData.isRGB & (modulo>1) ) {
		modulo = 1;
		fprintf( stderr,
				"cannot modulo RGB camera!" );
	}

	if( modulo > 4 ) modulo = 4;
	me->remaining = -1;
	rdiffData.modulo = modulo;
	
	size = cameraModule.x * cameraModule.y / modulo / modulo;

	prevLen = (cameraModule.x * cameraModule.y) 
		* (rdiffData.isRGB? 4 : 1 );
	prevPtr = malloc( prevLen );
	result  = malloc( prevLen );

	VERBOSE(VERB_INIT) {
		KIDID; 
		printf("cam %ld: %s: _init\n", 
				cameraModule.cameraNumber,
				module );
	}

	if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
		fprintf( stderr, 
				"%s: cannot init SDL: %s\n", 
				module, SDL_GetError() );
		return;
	}
	
	if( !testing ) 
	    rdiffData.screen = SDL_SetVideoMode( cameraModule.x/modulo,
					cameraModule.y/modulo,
					0,
					SDL_SWSURFACE );
	
	else
	    rdiffData.screen = SDL_SetVideoMode( 1024,
					768,
					0,
					SDL_SWSURFACE );
	

	if( NULL == rdiffData.screen ) {
		fprintf( stderr, "%s: cannot set video mode for SDL\n", module );
	}
	 					 
	for( i=0; i<256; i++ ) 
		rdiffData.colors[i].r = 
		rdiffData.colors[i].g = 
		rdiffData.colors[i].b = i;
		
	me->processFrame = _processFrame;
	me->processPixel = NULL;
	me->closeFrame = NULL;
	me->saveResults = _saveResults;
	
}
	
	
