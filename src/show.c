/* show.c -- show an image */

/* 
 *  $Log: show.c,v $
 *  Revision 1.7  2016/03/24 23:43:03  root
 *  16 bit
 *
 *  Revision 1.6  2011/11/10 00:58:18  root
 *  some old fixes, never committed
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

static const char rcsid[] = "$Id: show.c 230 2016-10-06 22:53:58Z stanley $";


#include "hotm.h"
#include <SDL/SDL.h>

#define VERBOSE(x) if(me->verbose & x)
#define VERB_INIT 1
#define VERB_PROCESS 2
#define VERB_SAVE 4

struct {
	SDL_Surface     *screen;
	SDL_Surface	*image;
	SDL_Color	 colors[256];
	int 		 modulo;
	int		 isRGB;
} showData;

SDL_Event event;

unsigned char *obuff;
struct timeval last;

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
int is16bit = 0;
unsigned long  diff;

unsigned short *iptr;
unsigned char  *ucptr;
unsigned char *optr;
int maxv = 0;
int minv = 65000;

	err = gettimeofday( &start, NULL );
	if( err ) 
		perror( me->module );

	if( cameraModule.format == formatMONO16 ) {
		is16bit = 1;
		iptr = (unsigned short *)frame->dataPtr;
		for( i=0; i<cameraModule.x*cameraModule.y; i++ ) {
			if( *iptr > maxv ) maxv = *iptr;
			if( *iptr < minv ) minv = *iptr;
			iptr++;
		}
		maxv = 1 + (maxv-minv) / 256;
	}
	
	iptr = (unsigned short *)frame->dataPtr;
	ucptr = (unsigned char *)frame->dataPtr;
	optr = obuff;
	for( i=0; i<cameraModule.x*cameraModule.y; i++ )
		if(is16bit)
			*optr++ = ((*iptr++)-minv)/maxv;
		else
			*optr++ = *ucptr++;
	optr = obuff;

	modulo = showData.modulo;

	if( showData.isRGB )
		showData.image = SDL_CreateRGBSurfaceFrom( ptr, 
						cameraModule.x,
						cameraModule.y, 
						24, 
						cameraModule.x*3, 
						0xff, 
						0xff00, 
						0xff0000, 
						0 );
	else {
		showData.image = SDL_CreateRGBSurfaceFrom( optr, 
						cameraModule.x/modulo,
						cameraModule.y/modulo, 
						8*modulo, 
						cameraModule.x*modulo, 
						0xff, 
						0xff, 
						0xff, 
						0 );
		SDL_SetPalette( showData.image, 
				SDL_LOGPAL, 
				showData.colors, 
				0, 256 );
	}
						
	SDL_BlitSurface( showData.image, NULL, showData.screen, NULL );
	SDL_UpdateRect( showData.screen, 
					0, 
					0, 
					showData.image->w, 
					showData.image->h );
					
	err = gettimeofday( &end, NULL );
	if( err ) {
		perror( me->module );
		me->processMicros = 9999999;
		return;
	}
	
	me->processMicros = (end.tv_sec - start.tv_sec) * 1000000
		+ (end.tv_usec - start.tv_usec );


	diff = (frame->when.tv_sec - last.tv_sec) * 1000000 ;
	diff +=	 (frame->when.tv_usec - last.tv_usec);
	last.tv_sec = frame->when.tv_sec;
	last.tv_usec = frame->when.tv_usec;

	sprintf( title, "Cam %ld: %13ld.%06ld (%06ld) %d", 
			cameraModule.cameraNumber,
			frame->when.tv_sec,
			frame->when.tv_usec,
			diff,
			me->processMicros );
	SDL_WM_SetCaption( title, title );

	while( SDL_PollEvent( &event ) ) {

		switch( event.type ) {

			case SDL_VIDEORESIZE:
				showData.screen = SDL_SetVideoMode( 
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
char 			*module = "show";
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
	
	getLongParam( &me->verbose, "showVerbose", module );
	getLongParam( &modulo, "showmodulo", module );
	getLongParam( &testing, "showtesting", module );
	showData.isRGB = (cameraModule.format == formatRGB8);

	if(showData.isRGB & (modulo>1) ) {
		modulo = 1;
		fprintf( stderr,
				"cannot modulo RGB camera!" );
	}

	if( modulo > 4 ) modulo = 4;
	me->remaining = -1;
	showData.modulo = modulo;
	
	size = cameraModule.x * cameraModule.y / modulo / modulo;

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
	    showData.screen = SDL_SetVideoMode( cameraModule.x/modulo,
					cameraModule.y/modulo,
					0,
					SDL_SWSURFACE );
	
	else
	    showData.screen = SDL_SetVideoMode( 1024,
					768,
					0,
					SDL_SWSURFACE );
	

	if( NULL == showData.screen ) {
		fprintf( stderr, "%s: cannot set video mode for SDL\n", module );
	}
	 					 
	for( i=0; i<256; i++ ) 
		showData.colors[i].r = 
		showData.colors[i].g = 
		showData.colors[i].b = i;
		
	me->processFrame = _processFrame;
	me->processPixel = NULL;
	me->closeFrame = NULL;
	me->saveResults = _saveResults;

	obuff = calloc( cameraModule.bufferLength, 1 );
	
}
	
	
