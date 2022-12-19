
/* map camera name to the data for that camera */

/* 
 *  $Log: mapCameraNameToData.c,v $
 *  Revision 1.3  2011/11/10 00:51:11  root
 *  changed for dc1394
 *
 *  Revision 1.2  2006/04/14 23:37:09  stanley
 *  changed from space to colon seps in data file
 *
 *  Revision 1.1  2002/11/05 23:21:52  stanley
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

static const char rcsid[] = "$Id: mapCameraNameToData.c 214 2016-10-04 00:46:24Z stanley $";

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hotm.h"
#include "camera.h"

/* uses cameraData.info and sends back to typeData */

void mapCameraNameToData( int x, char* name )
{
char line[256];
FILE *in;
long this;
char *ptr;
char *strtokPtr;

/* default camera mapping file */
char mapFile[] = "cameraData";

	/* empty typeData */
	memset( &typeData, 0, sizeof(typeData) );
	
	/* look for an environment variable to tell us */
	if( getenv("CAMERADATAFILE") )
		strcpy( line, getenv("CAMERADATAFILE"));
	else
		strcpy( line, mapFile );
		
	/* or maybe a cameraMapping option in the command file */
	getStringParam( line, "cameradatafile", "mapCameraNameToData" );
		
	in = fopen( line, "r" );
	if( NULL == in ) {
		fprintf(stderr, 
			"mapCameraNameToData: cannot open datafile: %s\n",
			line );
		exit(-1);
	}
	
	/* loop through lines of file look for me */
	ptr = NULL;
	while( fgets( line, sizeof(line), in ) ) {
		line[strlen(line)-1] = 0;
		ptr = strtok_r( line, ":", &strtokPtr );

		/* allow comments */
		if( '#' == *ptr ) continue;
		
		/* look for match */
		if( !strncasecmp( ptr, name, strlen(ptr) ) )
			break; /* found it! */

		/* keep "no match" flag */
		ptr = NULL;
	}
	
	if( ptr == NULL ) 
		/* no match! just return with empty typeData*/
		return;
		
	
	strcpy( typeData.name, ptr );	/* save name */
	strcpy( typeData.defaultFramerate, strtok_r( NULL, ":", &strtokPtr ));
        strcpy( typeData.defaultColorCoding, strtok_r( NULL, ":", &strtokPtr ));
        
	typeData.X               = atol( strtok_r( NULL, ":", &strtokPtr ) );
	typeData.Y               = atol( strtok_r( NULL, ":", &strtokPtr ) );
	typeData.left            = atol( strtok_r( NULL, ":", &strtokPtr ) );
	typeData.top             = atol( strtok_r( NULL, ":", &strtokPtr ) );
        typeData.referenceWidth  = atol( strtok_r( NULL, ":", &strtokPtr ) );
        typeData.referenceHeight = atol( strtok_r( NULL, ":", &strtokPtr ) );
	
	return;
	
}

 
