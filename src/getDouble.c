/* getDouble -- get a Double value from command line */

/* 
 *  $Log: getDouble.c,v $
 *  Revision 1.1  2011/11/10 00:45:09  root
 *  Initial revision
 *
 *  Revision 1.5  2003/01/07 21:46:47  stanley
 *  unremembered change
 *
 *  Revision 1.4  2002/08/14 21:51:55  stanleyl
 *  add listcommands verbosity
 *
 *  Revision 1.3  2002/08/10 00:25:40  stanleyl
 *  change to use sscanf %i so numbers can be hex or octal
 *
 *  Revision 1.2  2002/08/09 00:08:06  stanleyl
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

static const char rcsid[] = "$Id: getDouble.c 172 2016-03-25 21:03:51Z stanley $";


#include "hotm.h"

double getDouble( struct _commandFileEntry_ *command )
{
double 	val;
long	lval;
char 	*ptr;
char	module[] = "getDouble";

	/* assume NULL command is 0 */
	if( NULL == command ) 
		return 0;
		
	/* find first : */
	ptr = index( command->line, ':' );
	if( NULL == ptr ) {
		fprintf( stderr, 
				"%s: no colon in line, assuming 0\n",
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

	if( *ptr < 64 ) {
		sscanf( ptr, "%lf", &val );
	}
	else {
		lval = dc1394Decode( ptr );
		val = lval;
	}
	return val;
	
}

void getDoubleParam( double *out, char *command, char *module )
{
struct _commandFileEntry_ *ptr;

	if( HVERBOSE(HVERB_LISTCOMMANDS) ) 
		printf("command: \"%s\" type long from module \"%s\"\n",
				command,
				module );

	ptr = findCommand( command );
	tagCommand( ptr, module );
	if( NULL != ptr ) *out = getDouble(ptr);	
}
