
/* deBayerRGGB.c -- deBayer a RGGB array (as in Flea) */

/* 
 * this is going to be called by deBayer, which will decide if I
 * am the right routine, or is my brother deBayerBGGR. I'm just
 * passed all the same things. 
 *
 * Here's the sweet trick that allows me to use the same code:
 *   I SWAP the R and B output bytes in the pixel struct!
 *   The deBayer algorithm doesn't care if the non-G bytes
 *   are actually R or B, it only knows that one goes into the
 *   R output and the other the B. Swap the two, use the same
 *   code, because the two are swapped implicitely in the input.
 *   Ain't that sweet?
 *
 */

/* 
 *  $Log: deBayerRGGB.c,v $
 *  Revision 1.2  2016/03/24 23:06:16  root
 *  16 bit data
 *
 *  Revision 1.1  2004/12/17 21:27:12  stanley
 *  Initial revision
 *
 *
 */
 

#define BAYER_RGGB
#include "hotm.h"

static char *RCSid = "$Id: deBayerRGGB.c 172 2016-03-25 21:03:51Z stanley $";
 
double deBayerRGGB( struct _pixel_ *out, 
 		    uint16_t *in, 
		    struct _cameraModule_ *c )
{

/* all code to do the work lives in this file */
#include "deBayerCode.h"

}
