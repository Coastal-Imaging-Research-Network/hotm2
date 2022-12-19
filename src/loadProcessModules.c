
/* loadProcessModules -- scan commands for "module", load what we
     find. */
 
/* 
 *  $Log: loadProcessModules.c,v $
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

static const char rcsid[] = "$Id: loadProcessModules.c 172 2016-03-25 21:03:51Z stanley $";
    
#include "hotm.h"

void loadProcessModules()
{
struct _commandFileEntry_ *cmd = firstCommandFileLine;
char *module = "loadProcessModules (internal)";
char file[256];
struct _processModule_ *pmod;
void *handle;

	/* find the next module command */
	while( cmd = findCommandNext( "module", cmd ) ) {
	
		strcpy(file, getString( cmd ));
		tagCommand( cmd, module );
		
		pmod = (struct _processModule_ *)
			calloc( 1, sizeof( struct _processModule_ ) );
		if( pmod == NULL ) {
			fprintf(stderr, 
				"%s: failed to malloc proc module slot\n",
				module );
			exit(-1);
		}
		
		appendList( &firstProcessModule, pmod );
		
		strcat( file, ".so" );
		strcpy( pmod->library, file );
		
		handle = dlopen( file, RTLD_NOW );
		if( NULL == handle ) {
			fprintf(stderr,
				"%s: failed to find module %s: %s\n",
				module, 
				file,
				dlerror());
			exit(-1);
		}
		
		cmd = cmd->next;
	}
}
