/* getString -- return a string pointer for the command
*/

/* 
 *  $Log: getString.c,v $
 *  Revision 1.4  2002/10/31 01:37:51  stanley
 *  added test for param failure in get1394
 *
 *  Revision 1.3  2002/08/14 21:52:28  stanleyl
 *   add listcommands verbosity
 *
 *  Revision 1.2  2002/08/09 00:08:46  stanleyl
 *  change space to isspace -- allow tabs
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

static const char rcsid[] = "$Id: getString.c 172 2016-03-25 21:03:51Z stanley $";


#include "hotm.h"

char *getString( struct _commandFileEntry_ * command )
{
char *ptr;

	/* return NULL if input is NULL */
	if( NULL == command )
		return NULL;
		
	/* find : */
	ptr = index( command->line, ':' );
	if( NULL == ptr ) 
		return NULL;
		
	/* first non space */
	ptr++;
	while( (*ptr) && (isspace(*ptr)) ) 
		ptr++;
		
	return ptr;
	
}

/* get a string, store it, tag the command line */
void getStringParam( char *out, char *command, char *module )
{
struct _commandFileEntry_ *ptr;
char *tmp;

	if( HVERBOSE(HVERB_LISTCOMMANDS) ) 
		printf("command: \"%s\" type string from module \"%s\"\n",
				command,
				module );

	ptr = findCommand( command );
	tagCommand( ptr, module );
	tmp = getString( ptr );
	if( NULL != tmp ) strcpy( out, tmp );
}

/* just like getStringParam, except I decode 1394 parameter strings */
void get1394StringParam( long *out, char *command, char *module )
{
char line[128];
long value;

	line[0] = 0;
	getStringParam( line, command, module );
	if( line[0] == 0 )
		return;
	value = dc1394Decode( line );
	if( value )
		*out = value;
	return;
}
