/* convert the MODE of the camera into the format for the data */
/* as specified by my enum list. The MODE is a string, this pulls */
/* the specific bits out and returns a number */

/* 
 *  $Log: modeToFormat.c,v $
 *  Revision 1.4  2010/04/02 19:11:45  stanley
 *  fix format 7, no longer MODE_FORMAT_7, there's a color coding
 *
 *  Revision 1.3  2002/10/31 01:33:28  stanley
 *  added test for null input
 *
 *  Revision 1.2  2002/08/05 23:21:27  stanleyl
 *  added FORMAT7 data modes
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

static const char rcsid[] = "$Id: modeToFormat.c 172 2016-03-25 21:03:51Z stanley $";


/* mode is the string like MODE_640x480_YUV422 as in the command file */

#include "hotm.h"

int modeToFormat( char *mode )
{

	/* watch out, maybe no mode to parse! */
	if( NULL == mode )
		return -1;

	/* look for most restrictive strings first */
	/* not RGB, because there is also RGB16 */

	/* look for format7 stuff ... */
	/* no, not anymore. MODE_FORMAT7_0 isn't a color coding,
	   it's a binning/ROI format. We'll be looking for
	   COLOR_FORMAT7_RGB8, etc when we know we're 7  */
#ifdef NEVER
	if( strstr( mode, "FORMAT7_0" ) ) return formatMONO8;
	if( strstr( mode, "FORMAT7_1" ) ) return -1;
	if( strstr( mode, "FORMAT7_2" ) ) return formatYUV422;
	if( strstr( mode, "FORMAT7_3" ) ) return formatYUV444;
	if( strstr( mode, "FORMAT7_4" ) ) return formatRGB8;
	if( strstr( mode, "FORMAT7_5" ) ) return formatMONO16;
	if( strstr( mode, "FORMAT7_6" ) ) return formatRGB16;
#endif

	if( strstr( mode, "RGB16" ) ) return formatRGB16;
	if( strstr( mode, "MONO16") ) return formatMONO16;
	if( strstr( mode, "RAW16") ) return formatMONO16;
	if( strstr( mode, "YUV422") ) return formatYUV422;
	if( strstr( mode, "YUV444") ) return formatYUV444;
	
	/* now shorter versions */
	if( strstr( mode, "MONO" ) ) return formatMONO8;
	if( strstr( mode, "RGB"  ) ) return formatRGB8;
	if( strstr( mode, "RAW"  ) ) return formatMONO8;
	
	/* no valid mode found. return -1 */
	return -1;
	
}
	
