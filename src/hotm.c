
/* 
 *  $Log: hotm.c,v $
 *  Revision 1.9  2016/03/24 23:11:31  root
 *  added stdout, stderr redirection to specific place
 *
 *  Revision 1.8  2010/06/09 20:13:34  stanley
 *  replaced stop with stop ???
 *
 *  Revision 1.7  2004/12/09 18:15:48  stanley
 *  added version, removed LD-LIBRARY
 *
 *  Revision 1.6  2002/08/14 21:28:08  stanleyl
 *  add timer module loading before camera
 *  fixup momma messages
 *
 *  Revision 1.5  2002/08/08 23:22:52  stanleyl
 *  parameterize verbose, moved kid messages inside id>=0 tests
 *
 *  Revision 1.4  2002/03/23 00:23:57  stanley
 *  message passing code
 *
 *  Revision 1.3  2002/03/22 20:10:54  stanley
 *  check for no command file on command line, do default
 *
 *  Revision 1.2  2002/03/22 20:03:28  stanley
 *  moved loadCommandFile to parseCommandLine
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

static const char rcsid[] = "$Id: hotm.c 389 2018-09-20 21:22:20Z stanley $";


#define _MAIN_
#include "hotm.h"
#include <signal.h>

/* an empty signal handler, all I need is to return */
void sighandle(int a) { }

/* what to do atexit. */
void exitHandler() 
{

	/* if we have a positive kid id, must tell parent to go */
	/* on about its life without me, it may be waiting for me to */
	/* finish setting up before it continues. */
	if( hotmParams.kidid >= 0 ) {
		printf("Reporting death to hotmomma\n");
		AssertState (hotmParams.mqid, hotmParams.kidid, MOMMA, stateExit);
	}

	/* stop the camera */
	if( cameraModule.stopCamera ) 
		(*cameraModule.stopCamera)(&cameraModule); 

}

/* the main banana */
int main( int argc, char **argv )
{

	cameraModule.version = 2;

	/* must make sure I have a good LD_LIBRARY_PATH */
	setenv( "LD_LIBRARY_PATH", "/usr/local/lib:.", 1 ); 

	/* take care of stdout and stderr before anything else */
	if( getenv("HOTM_STDOUT") ) {
		if( NULL == freopen( getenv("HOTM_STDOUT"), "w", stdout ) ) {
			perror("redirecting stdout");
			exit;
		}
	}
	if( getenv("HOTM_STDERR") ) {
		if( NULL == freopen( getenv("HOTM_STDERR"), "w", stderr ) ) {
			perror("redirecting stderr");
			exit;
		}
	}

	/* register a cleanup routine */
	atexit( exitHandler );
	
	/* parse the command line I got */
	parseCommandLine( argc, argv );

	/* load the command file I've been told to */
	/* now done in parseCommandLine, if on command line */
	if( hotmParams.commandFile[0] == 0 ) {
		strcpy( hotmParams.commandFile, "default.command" );
		loadCommandFile(); 
	}
	
	/* get common parameters like debug, etc */
	commonCommand();

	/* load any timer modules to control timing */
	loadTimer();
	
	/* load my camera module */
	loadCameraModule();
	
	if( HVERBOSE(HVERB_CAMERA) ) {
		printf("Camera module file: %s\n", cameraModule.library );
		printf("Camera module name: %s\n", cameraModule.module );
		printf("Camera size: %ld x %ld\n", 
			cameraModule.x, 
			cameraModule.y );
	}

	/* load any data collection modules */
	loadProcessModules();

	/* dump the command file data */
	dumpCommandFile( hotmParams.verbose );
	
	/* this is "wait for start", pause until momma says to go */
	if( hotmParams.kidid >= 0 ) {
		AssertState (hotmParams.mqid, hotmParams.kidid, MOMMA, stateReady);
		if( HVERBOSE(HVERB_MOMMA) ) {
			KIDID;
			printf("Waiting for hotmomma to start me\n");
		}
		ExpectState (hotmParams.mqid, MOMMA, hotmParams.kidid, stateStart);
		if( HVERBOSE(HVERB_MOMMA) ) {
			KIDID;
			printf ("is collecting images\n");
		}
	}
					
	/* start the camera */
	(*cameraModule.startCamera)(&cameraModule);
	
	/* do all the processing */
	processLoop();
	
	/* stop the camera */
	(*cameraModule.stopCamera)(&cameraModule);

	/* tell momma We are done */
	if( hotmParams.kidid >= 0 ) {
		if( HVERBOSE(HVERB_MOMMA) ) {
			KIDID;
			printf ("done collecting images\n");
		}
		AssertState (hotmParams.mqid, hotmParams.kidid, MOMMA, stateDone);
		ExpectState (hotmParams.mqid, MOMMA, hotmParams.kidid, stateFinish);
		if( HVERBOSE(HVERB_MOMMA) ) {
			KIDID;
			printf ("saving results\n");
		}
	}
	
	/* and save the results! */
	saveResults();
	
	/* I've ended successfully, remove atexit conditions */
	if( (hotmParams.kidid >= 0) && (HVERBOSE(HVERB_MOMMA)) ) {
		KIDID;
		printf ("is terminating\n");
	}
	
	hotmParams.kidid = -1;
	cameraModule.stopCamera = NULL;
	
}

