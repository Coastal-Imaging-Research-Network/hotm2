
/*
	loadTimer
	
	find the command "timerModule", load the shared library in
	that file. Get the symbols we need from it
	
*/

/* 
 *  $Log: loadTimer.c,v $
 *  Revision 1.3  2003/10/05 23:04:52  stanley
 *  fix comparison
 *
 *  Revision 1.2  2003/01/07 21:41:36  stanley
 *  removed & on file in getString
 *
 *  Revision 1.1  2002/08/14 21:29:28  stanleyl
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

static const char rcsid[] = "$Id: loadTimer.c 172 2016-03-25 21:03:51Z stanley $";


#include "hotm.h"

void loadTimer()
{
char *module = "loadTimer";
char file[256];
void *handle;

	/* NO default timer */
	strcpy( file, "" );
	
	getStringParam( file, "timermodule", module );
		
	if( 0 == file[0] ) return;
	
	strcat( file, ".so" );
	handle = dlopen( file, RTLD_NOW );
	if( NULL == handle ) {
		fprintf(stderr, "%s: failed to find timer module: %s\n",
			module, file );
		fprintf(stderr, "%s: %s\n", module, dlerror() );
		exit(-1);
	}
	
}
	
	
