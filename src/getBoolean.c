/*  getBoolean -- return 1 for true/on/1, 0 for false/off/0
 *
 *   from a command line pointer passed in
 *
 */

/* 
 *  $Log: getBoolean.c,v $
 *  Revision 1.3  2002/08/14 21:52:16  stanleyl
 *   add listcommands verbosity
 *
 *  Revision 1.2  2002/08/09 00:06:52  stanleyl
 *  change from space test to 'isspace' -- allow tabs
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

static const char rcsid[] = "$Id: getBoolean.c 172 2016-03-25 21:03:51Z stanley $";


#include "hotm.h"
 
int getBoolean( struct _commandFileEntry_ *command )
{

int 	val;
char 	*ptr;
char	module[] = "getBoolean";
	
	/* assume NULL command is FALSE */
	if( NULL == command ) 
		return 0;
		
	/* find first : */
	ptr = index( command->line, ':' );
	if( NULL == ptr ) {
		fprintf( stderr, 
				"%s: no colon in line, assuming FALSE\n",
				module );
		fprintf( stderr,
				"%s: line was \"%s\"\n",
				module,
				command->line );
		return 0;
	}
	
	/* look from :+1 for token */
	ptr++;
	while( (*ptr) && (isspace(*ptr)) ) 
		ptr++;
	
	if( 0 == *ptr ) {
		fprintf(stderr,
				"%s: end of line reached!\n%s: line: %s",
				module,
				module,
				command->line );
		return 0;
	}
	
	switch (*ptr++) {
	
		case '1':
		case 't':
		case 'T':
			val = 1;
			break;
			
		case '0':
		case 'f':
		case 'F':
			val = 0;
			break;
			
		case 'o':
		case 'O':
			if( ('n' == *ptr) || ('N' == *ptr) )
				val = 1;
			else
				val = 0;
			break;
			
		default:
			fprintf(stderr,
					"%s: cannot decode line, assuming FALSE\n",
					module );
			fprintf(stderr,
					"%s: line: %s\n",
					module,
					command->line );
			val = 0;
			
	}
	
	return val;
	
}	

void getBooleanParam( int *out, char *command, char *module )
{
struct _commandFileEntry_ *ptr;

	if( HVERBOSE(HVERB_LISTCOMMANDS) ) 
		printf("command: \"%s\" type boolean from module \"%s\"\n",
				command,
				module );

	ptr = findCommand( command );
	tagCommand( ptr, module );
	if( NULL != ptr ) *out = getBoolean(ptr);

}
