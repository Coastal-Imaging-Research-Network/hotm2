
/* deBayer.c -- unpack a Bayer pattern CCD image into RGB */

/* 
 *  deBayer( out, in, cameraModule )  
 *    -- where out is a struct _pixel_ *,
 *    and in is an unsigned char * of either 8 or 10 bit
 *    monochrome data as reported by a MicroPix (dragonfly)
 *    camera. usually 8 bit, so we will write that first.
 *
 *  other requirements: a cameraModule struct with
 *   the info about the input data format. width, height, format
 *   etc. 
 *
 *  this will take work to be as optimized as possible. sigh.
 *
 *  $Log: deBayer.c,v $
 *  Revision 1.5  2016/03/24 23:05:34  root
 *  16 bit, and GBRG format
 *
 *  Revision 1.4  2010/04/02 19:09:52  stanley
 *  remove specific camera dependencies, rely on "rawOrder" value
 *
 *  Revision 1.3  2006/04/14 23:35:25  stanley
 *  added scorpion to list
 *
 *  Revision 1.2  2004/12/17 21:26:30  stanley
 *  convert to switch based on camera model: BGGR or RGGB
 *
 *  Revision 1.1  2004/12/06 21:45:33  stanley
 *  Initial revision
 *
 *
 */
 
#include "hotm.h"
 
static char *RCSid = "$Id: deBayer.c 362 2018-08-15 23:45:44Z stanley $";

double deBayerRGGB( struct _pixel_ *out,
		uint16_t *in,
		struct _cameraModule_ *c );
double deBayerBGGR( struct _pixel_ *out,
		uint16_t *in,
		struct _cameraModule_ *c );
 
double deBayer( struct _pixel_ *out, 
 		 uint16_t *in, 
		 struct _cameraModule_ *c )
{

//printf("type %s processed\n", cameraModule.rawOrder );

if( !strcmp( cameraModule.rawOrder, "RGGB" ) ) 
	deBayerRGGB( out, in, c );
else if( !strcmp( cameraModule.rawOrder, "BGGR" ) )
	deBayerBGGR( out, in, c );
else if( !strcmp( cameraModule.rawOrder, "GRBG" ) )
	deBayerRGGB( ++out, ++in, c );
else if( !strcmp( cameraModule.rawOrder, "GBRG" ) )
	deBayerBGGR( ++out, ++in, c );
else  
	whateverToRGB( out, (unsigned char *) in );
} 
