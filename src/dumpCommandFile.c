/* dumpCommandFile -- output the command file lines
 *    either all of them or just the ones without modules 
 */

/* 
 *  $Log: dumpCommandFile.c,v $
 *  Revision 1.3  2002/08/08 23:36:58  stanleyl
 *  parameterize verbosity
 *
 *  Revision 1.2  2002/03/22 20:20:15  stanley
 *  shut up about unloved lines unless debug is set
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

static const char rcsid[] = "$Id: dumpCommandFile.c 172 2016-03-25 21:03:51Z stanley $";
 
#include "hotm.h"

void dumpCommandFile( int verbose )
{
struct _commandFileEntry_ *ptr = firstCommandFileLine;

	if( !HVERBOSE(HVERB_COMMANDFILE) ) return;

	printf("Dumping %s command file lines:\n",
			HVERBOSE(HVERB_ALLCOMMANDS)? "all" : "unloved" );
			
	do {
	
		if( HVERBOSE(HVERB_ALLCOMMANDS) ) { 
			printf("   Line: %s\nUsed by: %s\n",
					ptr->line, ptr->module );
			continue;
		}
		if( 0 == ptr->module[0] ) {
			printf("   Line: %s\nUsed by: nobody\n",
					ptr->line );
			continue;
		}
	
	} while( NULL != (ptr = ptr->next) );
			
}

