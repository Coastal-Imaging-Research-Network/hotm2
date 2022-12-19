/* a simple function to decode the DC1394 control strings */
/* that one normally uses in C source as enums, so they can */
/* appear in hotm command files as camera parameters. */

/* 
 *  $Log: dc1394Decode.c,v $
 *  Revision 1.4  2011/11/10 00:33:56  root
 *  added encode, bigger parameter buffer
 *
 *  Revision 1.3  2010/04/02 19:09:05  stanley
 *  pass along a NULL finding
 *
 *  Revision 1.2  2009/02/20 21:14:30  stanley
 *  remove one print about paramfile
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

static const char rcsid[] = "$Id: dc1394Decode.c 172 2016-03-25 21:03:51Z stanley $";


#define NUMPARAMS 256
#define MAXPARAMLENGTH 128
#define DEFAULTPARAMENV "DC1394PARAMLIST"
#define DEFAULTPARAMFILE "dc1394ParamList"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct {
	char	param[MAXPARAMLENGTH];
	long	value;
} dc1394[NUMPARAMS];

void dc1394LoadParams()
{
FILE * fid;
long l;
char line[128];
int i;

	if( getenv( DEFAULTPARAMENV ) ) 
		fid = fopen( getenv( DEFAULTPARAMENV ), "r" );
	else
		fid = fopen( DEFAULTPARAMFILE, "r" );
		
	if( NULL == fid ) {
		fprintf(stderr, 
			"dc1394LoadParams: cannot find dc1394 encoding file\n");
		fprintf(stderr,
			"dc1394LoadParams: hope you don't need to use any!\n");
		return;
	}
	
	i=0;
	while( fgets( line, 128, fid) ) {
		/*printf("paramfile: %s\n", line );*/
		strcpy( dc1394[i].param, strtok(line, " " ));
		dc1394[i++].value = atol(strtok(NULL," "));
	}
	  
	
}



long dc1394Decode( char *param )
{
int i;
char dc1394Param[256];

	/* if NULL coming in, pass it along! */
	if( NULL == param )
             return (long)NULL;

	/* if not loaded yet, load me now! load me now! */
	if( 0 == dc1394[0].param[0] )
		dc1394LoadParams();
        
        /* create a second version that is prepended with 'DC1394_' */
        /* changed from version 1 of libdc1394, accept both */
        strcpy( dc1394Param, "DC1394_" );
        strcat( dc1394Param, param );

	/* after the loading, look for me now! */
        for( i=0; i<NUMPARAMS; i++ ) {
		if( !strcasecmp( param, dc1394[i].param ) ) 
			return dc1394[i].value;
                if( !strcasecmp( dc1394Param, dc1394[i].param ) ) 
                     return dc1394[i].value;
        }
			
	/* if I get here, I wasn't found. Report the problem */
	fprintf(stderr,
		"decodeDC1394: cannot find value for param %s\n",
		param );
		
	return 0;
}

char *dc1394Encode( long value )
{
     int i;
     
     /* if not loaded yet, do so now, dufus */
     if( 0 == dc1394[0].param[0] )
          dc1394LoadParams();
     
     for( i=0; i<NUMPARAMS; i++ )
          if( value == dc1394[i].value )
               return dc1394[i].param;
     
     /* nope. not here */
     return "dc1394Encode: cannot find value\n";
     
}

     
     
