
/* deBayerBGGR.c -- deBayer a BGGR array (as in MicroPix and DragonFly) */

/* 
 * this is going to be called by deBayer, which will decide if I
 * am the right routine, or is my brother deBayerRGGB. I'm just
 * passed all the same things. 
 *
 */

/* 
 *  $Log: deBayerBGGR.c,v $
 *  Revision 1.2  2016/03/24 23:04:05  root
 *  16 bit data
 *
 *  Revision 1.1  2004/12/17 21:27:47  stanley
 *  Initial revision
 *
 *
 */
 

#define BAYER_BGGR
#include "hotm.h"

static char *RCSid = "$Id: deBayerBGGR.c 172 2016-03-25 21:03:51Z stanley $";
 
double deBayerBGGR( struct _pixel_ *out, 
 		    uint16_t *in, 
		    struct _cameraModule_ *c )
{

/* all code to do the work lives in this file */
#include "deBayerCode.h"

}
