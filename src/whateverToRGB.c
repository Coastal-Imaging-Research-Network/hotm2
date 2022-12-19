
/* convert whatever the input is to RGB output */
/* input must match the cameraModule.format enum */


#include "hotm.h"

/* 
 *  $Log: whateverToRGB.c,v $
 *  Revision 1.4  2016/03/24 23:45:08  root
 *  64 bit os, 16 bit data
 *
 *  Revision 1.3  2011/11/10 00:27:35  root
 *  change YUV2RGB for libdc1394 conflict
 *
 *  Revision 1.2  2003/01/07 21:32:22  stanley
 *  change return to void, add return to default
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

static const char rcsid[] = "$Id: whateverToRGB.c 172 2016-03-25 21:03:51Z stanley $";

#define myYUV2RGB(y, u, v, r, g, b)\
  r = ( 76312L * y              + 104597L * v ) >> 16;  \
  g = ( 76312L * y + -25674L * u + -53279L * v ) >> 16;  \
  b = ( 76312L * y + 132201L * u              ) >> 16;  \
  r = r < 0 ? 0 : r;\
  g = g < 0 ? 0 : g;\
  b = b < 0 ? 0 : b;\
  r = r > 255 ? 255 : r;\
  g = g > 255 ? 255 : g;\
  b = b > 255 ? 255 : b

void whateverToRGB( struct _pixel_ *RGB, unsigned char *in )
{
unsigned long i;
int y, u, v;
long r, g, b;
uint16_t *intPtr;
uint16_t maxv = 0;
uint16_t minv = 65000;

	switch( cameraModule.format ) {
	
		/* simple stuff first */
		case formatRGB8:
			memcpy( RGB, in, cameraModule.bufferLength );
			return;
			
		case formatYUV444:

			for( i=0; i<cameraModule.x*cameraModule.y; i++ ) {
				u = (unsigned char)*in++ - 128; 
				y = (unsigned char)*in++ - 16; 
				v = (unsigned char)*in++ - 128;
				myYUV2RGB( y, u, v, r, g, b );
				RGB->r = r; RGB->g = g; RGB->b = b;
				RGB++;
			}
			return;
					
		case formatYUV422:
			
			for( i=0; i<cameraModule.x*cameraModule.y; i++ ) {
				u = (unsigned char)*in++ - 128; 
				y = (unsigned char)*in++ - 16; 
				v = (unsigned char)*in++ - 128;
				myYUV2RGB( y, u, v, r, g, b );
				RGB->r = r; RGB->g = g; RGB->b = b;
				RGB++;
				
				y = (unsigned char)*in++ - 16;
				myYUV2RGB( y, u, v, r, g, b );
				RGB->r = r; RGB->g = g; RGB->b = b;
				RGB++;
				
				i++;			
			}
			return;
			
		case formatMONO8:

			for( i=0; i<cameraModule.x*cameraModule.y; i++ ) {
				RGB->r = RGB->g = RGB->b = *in++;
				RGB++;
			}
			return;
			
		case formatMONO16:
			//printf("Conversion from 16 to 8 bit not tested\n" );
			intPtr = (uint16_t *)in;
				for( i=0; i<cameraModule.x*cameraModule.y; i++) {
					if (*intPtr>maxv) maxv = *intPtr;
					if (*intPtr<minv) minv = *intPtr;
					intPtr++;
				}
				intPtr = (uint16_t *) in;
				maxv = 1 + (maxv-minv)/256;
			for( i=0; i<cameraModule.x*cameraModule.y; i++ ) {
				RGB->r = RGB->g = RGB->b = ((*intPtr++)-minv)/maxv;
				RGB++;
			}
			return;
			
		case formatRGB16:
			printf("Conversion from 16 to 8 bit not tested\n" );
			intPtr = (uint16_t *)in;
			for( i=0; i<cameraModule.x*cameraModule.y; i++ ) {
				RGB->r = (*intPtr++) >> 8;
				RGB->g = (*intPtr++) >> 8;
				RGB->b = (*intPtr++) >> 8;
				RGB++;
			}
			return;			
		
		default:
			return;
		
	}
	
}
