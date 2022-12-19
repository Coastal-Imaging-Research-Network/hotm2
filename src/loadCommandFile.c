
/* load the hotm command file -- the list of parameters */

/* 
 *  $Log: loadCommandFile.c,v $
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

static const char rcsid[] = "$Id: loadCommandFile.c 351 2018-08-15 23:41:44Z stanley $";


#include "hotm.h"

void loadCommandFile()
{

char line[256];
struct _commandFileEntry_ *new;
struct _commandFileEntry_ *last = NULL;

FILE *in;

char *module = "loadCommandFile";

	/* open the list of commands */
	in = fopen( hotmParams.commandFile, "r" );
	/* failed! */
	if( NULL == in ) {
		fprintf(stderr, "%s: error opening command file %s\n",
			module, 
			hotmParams.commandFile );
		perror( module );
		exit(-1);
	}
	
	/* ok, read it line by line */
	while( fgets( line, sizeof(line), in ) ) {
	
		/* ignore comments and blank lines */
		if( (line[0] == '#') || (strlen(line) == 0) )
			continue;
		if( NULL == index( line, ':' ) )
			continue; 
	
		/* malloc space to keep the line and associated data */
		new = calloc( 1, sizeof( struct _commandFileEntry_ ) );
		if( NULL == new ) {
			fprintf(stderr, "%s: failure in malloc\n",
				module );
			perror( module );
		}
		
		/* remove the newline */
		line[strlen(line)-1] = 0;
		/* copy the line into the malloc'd space */
		strcpy( new->line, line );
		/* clear the module list */
		new->module[0] = 0;
		/* clear the "next" pointer */
		new->next = NULL;
		
		/* now, link this into the list */
		appendList( &firstCommandFileLine, new );
		 		
		if( hotmParams.debug ) 
			printf("%s: line @ %08lx: %s\n", 
				module, (unsigned long) new, new->line);
				
	
	}
}
