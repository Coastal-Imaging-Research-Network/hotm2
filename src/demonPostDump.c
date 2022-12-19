
/* post-process -- load a saved file, do the stuff */
/* dump raw data and spawn post proc for gigE cams */

/* this is the conversion routine to convert from raw data
 * saved to disk by the demon process into the final output
 * data types - snap, timex, variance. stacks are done
 * by the demon directly.
 
 * $Log: demonPostProc.c,v $
 * Revision 1.2  2016/03/25 00:03:36  stanley
 * longer lines of text for wider camera! runningk fixes to text
 *
 * Revision 1.13  2011/11/10 00:56:53  root
 * added split long ago, updates for format 7 handling dc1394
 *
 * Revision 1.12  2010/11/16 18:48:03  stanley
 * added dark code
 *
 * Revision 1.11  2010/09/12 13:03:44  stanley
 * fixed flags for selected products, detecting deBayer
 *
 * Revision 1.10  2010/06/09 20:16:52  stanley
 * changed process to depend on rawOrder instead of knowing all the cameras
 *
 * Revision 1.9  2010/04/02 19:12:53  stanley
 * mostly format 7 (subimage stuffed into full) changes
 *
 * Revision 1.8  2009/02/20 21:58:22  stanley
 * casted data and ptr to remove sign complaints
 *
 * Revision 1.7  2009/02/20 21:17:02  stanley
 * added dobright, command line options
 *
 * Revision 1.6  2007/08/29 01:07:12  stanley
 * changed raw header format to text
 *
 * Revision 1.5  2006/04/14 23:35:51  stanley
 * added scorpion to debayer list
 *
 * Revision 1.4  2005/08/11 00:12:47  stanley
 * added camera number modifier-- masquerade
 *
 * Revision 1.3  2004/12/17 21:08:35  stanley
 * moved debayer test, added text in image
 *
 * Revision 1.2  2004/12/06 21:50:04  stanley
 * added more cameras to the deBayer list
 *
 * Revision 1.1  2004/03/17 00:56:11  stanley
 * Initial revision
 *
 *
 */


static char *RCSId = "$Id: demonPostDump.c 251 2016-12-22 00:56:46Z stanley $";

#include "demon2.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define GPRINT if(cameraModule.verbose & HVERB_PROCESSTIMES)

#define TEST16BIT  \
     if( (cameraModule.format == formatMONO16)\
        || (cameraModule.format == formatRGB16) )\
                is16bit = 1;\
     else is16bit = 0



int
demonPostDump( struct _diskdata_ *p )
{

     FILE *out;
     char outFile[128];

	unsigned long didWrite;
     unsigned long fullSize;
     time_t baseEpoch;

     TEST16BIT;                 /* probably redundant */

     baseEpoch = 1 * p->begin;

     sprintf( outFile, "%s/%ld.c%ld.dump", 
		p->name,
		baseEpoch,
              cameraModule.cameraNumber );

     out = fopen( outFile, "w" );

     /* base data, what do I need? */
     didWrite = fwrite( p, sizeof( struct _diskdata_), 1, out );
	GPRINT printf("wrote %lu diskdata structs\n", didWrite);

     didWrite = fwrite( &cameraModule, sizeof( struct _cameraModule_), 1, out );
	GPRINT printf("wrote %lu cameraModule structs\n", didWrite);

     didWrite = fwrite( &hotmParams, sizeof( hotmParams ), 1, out );
	GPRINT printf("wrote %lu hotmParams structs\n", didWrite);

     //didWrite = fwrite( &cameraData, sizeof( cameraData ), 1, out );
        //printf("wrote %lu cameraData structs\n", didWrite);

     /* now all the data */
     didWrite = fwrite( p->snapPtr, sizeof(uint16_t), p->numPix, out );
	GPRINT printf("wrote %lu snap pixels\n", didWrite);
     didWrite = fwrite( p->sumPtr, sizeof(uint32_t), p->numPix, out );
	GPRINT printf("wrote %lu sum pixels\n", didWrite);
     didWrite = fwrite( p->sumsqPtr, sizeof(uint64_t), p->numPix, out );
	GPRINT printf("wrote %lu sumsq pixels\n", didWrite);
     didWrite = fwrite( p->brightPtr, sizeof(uint16_t), p->numPix, out );
	GPRINT printf("wrote %lu bright pixels\n", didWrite);
     didWrite = fwrite( p->darkPtr, sizeof(uint16_t), p->numPix, out );
	GPRINT printf("wrote %lu dark pixels\n", didWrite);
     didWrite = fwrite( p->runDarkPtr, sizeof(double), p->numPix, out );
	GPRINT printf("wrote %lu rundark pixels\n", didWrite);

     fclose(out);

}
