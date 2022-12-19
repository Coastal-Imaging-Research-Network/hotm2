
/* simple conversion of framerate from code into number */
/* make use of the fact that the rate coding is powers of two */

/* 
 *  $Log: framerateToDouble.c,v $
 *  Revision 1.2  2011/11/10 00:44:33  root
 *  fixed for new dc1394
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

static const char rcsid[] = "$Id: framerateToDouble.c 172 2016-03-25 21:03:51Z stanley $";


#include <dc1394/dc1394.h>


double framerateToDouble( long rate )
{
double out = 240;

	rate = DC1394_FRAMERATE_MAX - rate;

	while( rate-- ) out /= 2;

	return out;

}
