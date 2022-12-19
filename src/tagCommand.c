/* tagCommand -- tag the command line entry with my
 *    module name
 */

/* 
 *  $Log: tagCommand.c,v $
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

static const char rcsid[] = "$Id: tagCommand.c 172 2016-03-25 21:03:51Z stanley $";
 
#include "hotm.h"

void tagCommand( struct _commandFileEntry_ *command, char *module )
{

	/* allow NULL command -- simply ignore this */
	if( NULL == command ) 
		return;
		
	if( (sizeof(command->module) - strlen(command->module)) 
		< strlen(module) ) {
		fprintf(stderr, "tagCommand: overflow!\n");
		return;
	}
	
	strcat( command->module, module );
	strcat( command->module, " " );
	
}

