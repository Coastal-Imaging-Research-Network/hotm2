
/* commonCommand -- get common command file entries */

/* 
 *  $Log: commonCommand.c,v $
 *  Revision 1.3  2003/01/07 21:29:48  stanley
 *  coerce baseEpoch to shut intel compiler up
 *
 *  Revision 1.2  2002/03/22 19:59:40  stanley
 *  added timelimit
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

static const char rcsid[] = "$Id: commonCommand.c 172 2016-03-25 21:03:51Z stanley $";


#include "hotm.h"


void commonCommand( void )
{

	struct _commandFileEntry_ *ptr;
	char module[] = "commonCommand (internal)";

	/* a few special command file entries */
	getStringParam( &hotmParams.programVersion[0], "programVersion", module );
	getStringParam( &hotmParams.sitename[0], "sitename", module );
	getLongParam( &hotmParams.discard, "discard", module );
	getLongParam( (long *)&hotmParams.baseEpoch, "baseEpoch", module );
	getStringParam( &hotmParams.baseName[0], "baseName", module );
	getLongParam( &hotmParams.skip, "skip", module );
	getLongParam( &hotmParams.timeLimit, "timelimit", module );
	
}

 
	
