
/*
	findCommand -- locate the requested command in the 
	command file list. return pointer to that struct.
	
*/

/* 
 *  $Log: findCommand.c,v $
 *  Revision 1.2  2002/08/09 00:01:06  stanleyl
 *  paramterize verbosity
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

static const char rcsid[] = "$Id: findCommand.c 172 2016-03-25 21:03:51Z stanley $";

#include "hotm.h"
#define LVERB if(HVERBOSE(HVERB_FINDCOMMAND))

struct _commandFileEntry_ *findCommand( char *command )
{
	return findCommandNext( command, firstCommandFileLine );
}

struct _commandFileEntry_ *findCommandNext( 
		char *command, 
		struct _commandFileEntry_ *ptr )
{
char *colon;
int len;

	if( ptr == NULL )
		return NULL;
		
	do {

		LVERB printf("exam: %s\n", ptr->line );
			
		colon = index( ptr->line, ':' );
		if( NULL == colon ) 
			continue;
			
		LVERB printf("rest: %s\n", colon );
			
		len = colon - ptr->line;
		LVERB printf("len: %d\n", len );
		
		if( strlen(command) != len )
			continue;
			
		if( strncasecmp( ptr->line, command, len ) )
			continue;
			
		/* FOUND A MATCH */
		return ptr;
		
	} while( NULL != (ptr = ptr->next) );
	
	return NULL;
}
