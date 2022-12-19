
/* map camera Number to the UID from that camera */

/* 
 *  $Log: mapCameraToUID.c,v $
 *  Revision 1.2  2003/01/07 21:40:47  stanley
 *  changed to hotm.h
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

static const char rcsid[] = "$Id: mapCameraToUID.c 299 2018-06-19 00:45:25Z stanley $";

#include "hotm.h"
char mc2uline[128];

char *mapCameraToUID( long number )
{
FILE *in;
long this;

/* default camera mapping file */
char mapFile[] = "cameraMapping";

	/* look for an environment variable to tell us */
	if( getenv("CAMERAMAPFILE") )
		strcpy( mc2uline, getenv("CAMERAMAPFILE"));
	else
		strcpy( mc2uline, mapFile );
		
	/* or maybe a cameraMapping option in the command file */
	getStringParam( mc2uline, "cameramapfile", "mapCameraToUID" );

	in = fopen( mc2uline, "r" );
	if( NULL == in ) {
		fprintf(stderr, 
			"mapCameraToUID: cannot open mapfile: %s\n",
			mc2uline );
		exit(-1);
	}
	
	while( fgets( mc2uline, sizeof(mc2uline), in ) ) {
		mc2uline[strlen(mc2uline)-1] = 0;
		this = atol(strtok( mc2uline, " " ));
		if( this == number ) 
			return strtok( NULL, " " );
	}
	
	return NULL;
	
}

 
