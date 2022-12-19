
/*
	loadCameraModule 
	
	find the command "cameraModule", load the shared library in
	that file. Get the symbols we need from it, put into camera
	data struct 
	
*/

/* 
 *  $Log: loadCameraModule.c,v $
 *  Revision 1.4  2003/01/07 21:19:14  stanley
 *  removed & from getstring parameter
 *
 *  Revision 1.3  2002/08/14 21:28:37  stanleyl
 *  cleaned up extraneous variables from last fix
 *
 *  Revision 1.2  2002/08/10 00:25:19  stanleyl
 *  fix so it actually loads something besides default.camera
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

static const char rcsid[] = "$Id: loadCameraModule.c 444 2019-04-05 01:34:59Z stanley $";


#include "hotm.h"
#include "camera.h"

void loadCameraModule()
{
char *module = "loadCameraModule";
char file[256];
void *handle;

	/* first, flag the cameraData structure with how big it is */
	/* because the MODULE may (will) define it with added things */
	memset( &cameraData, 0, sizeof(cameraData) );
	cameraData.mySize = sizeof(cameraData);

	strcpy( file, "default.camera" );
	
	getStringParam( file, "cameramodule", module );
		
	strcat( file, ".so" );
	strcpy( cameraModule.library, file );
	// handle = dlopen( file, RTLD_NOW | RTLD_DEEPBIND );
	handle = dlopen( file, RTLD_NOW  );
	if( NULL == handle ) {
		KIDID;
		fprintf(stderr, "%s: failed to find camera module: %s\n",
			module, file );
		KIDID;
		fprintf(stderr, "%s: %s\n", module, dlerror() );
		exit(-1);
	}

	/* kludge -- check for valid initCamera AND valid getCamera.
	   if I find init but not get, then I know I've loaded a stub
	   camera demon that only returns the linked in camera module.
	   Sheesh, Spinnaker must be linked instead of loaded, so
	   this is a kludge JUST FOR THAT. */

	//printf("got to loadCamera module, loaded %s\n", file );

	if( cameraModule.initCamera ) 
		return;

	cameraModule.initCamera = dlsym( handle, "x_init" );
	/* has my init function been set? */
	if( cameraModule.initCamera ) {
		/* if getCamera set, then a true _init has been called */
		/* otherwise, must call the init */
		if( cameraModule.getCamera == NULL ) 
			(*cameraModule.initCamera)();
		/* if stopCamera is set, add it to atexit. */
		/* problem in ubuntu 18.04 cannot dynamic link an atexit */
		if( cameraModule.stopCamera ) 
			atexit((void *)cameraModule.stopCamera);
		return;
	}

	printf("failed to init camera -- no _init function available.\n");
	exit;
	
}
	
