
/* post-process -- load a saved file, do the stuff */

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


static char *RCSId = "$Id: demonPostProc.c 303 2018-06-19 00:50:30Z stanley $";

#include "demon2.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define max(a,b) ((a)>(b) ? (a) : (b))
#define min(a,b) ((a)>(b) ? (b) : (a))
/*#define stdDev(sum,sq,n) (sqrt((n)*(double)(sq) - (double)(sum)*(double)(sum)) / (n))*/
#define stdDev(sum,sq,n) (sqrt(1.0*(n)*(sq) - 1.0*(sum)*(sum)) / (n))

double deBayer( struct _pixel_ *, uint16_t *, struct _cameraModule_ * );
void savePNG();
void insertText16();

int
demonPostProc( struct _diskdata_ *p )
{

     char outSnap[128];
     char outTimex[128];
     char outVar[128];
     char outBright[128];
     char outDark[128];

     int doSplit = 0;
     double frameInterval;

     struct timeval frameTime;
     struct _imageHeader_ ih;
     long ihsize;               /* old is shorter. */

     unsigned char *data;
     uint16_t *ptr;
     uint32_t *l1ptr;
     uint64_t *l2ptr;
     struct _pixel_ *RGB;
     struct _pixel_ *FullRGB;
     unsigned long fullSize;

     time_t baseEpoch;
     struct tm *myTmPtr;
     char myTz[32];
	extern char* tzname[2];

     unsigned long inDataBytes;
     unsigned long pixCount;
     unsigned long pixBytes;
     unsigned long i;
     unsigned long frameCount = p->nFrames;

     double lastTime;
     double firstTime;
     double period;

	double xxt;

     int j;
     int index;
     int baseIndex;
     int in;
     int count;

     int camnum = -1;
     char line1[2024];
     char line2[2024];
	char lineTemp[1024];
     int bottomLine;
     char fullFileName[256];
     char *thisName;
     struct timeval xxstart;
     struct timeval xxend;
     long ltemp;
     int doDeBayer = 0;

     double maxSD = 0;
     int qual = 80;

     char rawOrder[5];

     char fileExt[8];

     TEST16BIT;                 /* probably redundant */

     pixCount = p->numPix;
     inDataBytes = p->size;
     frameInterval = p->frameInterval;

     strcpy( fullFileName, p->name );
     strcat( fullFileName, "/" );
     thisName = &fullFileName[strlen( fullFileName )];

     if( writePNG )
          strcpy( fileExt, "png" );
     else
          strcpy( fileExt, "jpg" );

     RGB = malloc( pixCount * sizeof( struct _pixel_ ) + 10000 );
	if( NULL == RGB ) {
		printf("failed malloc of RGB");
	}
     FullRGB = NULL;
     /* format 7, image area smaller than normal, insert */
     if( ( cameraModule.x < cameraModule.cameraWidth )
         || ( cameraModule.y < cameraModule.cameraHeight ) ) {
          fullSize = cameraModule.cameraWidth * cameraModule.cameraHeight;
          FullRGB = calloc( fullSize, sizeof( struct _pixel_ ) );
     }

     baseEpoch = 1 * p->begin;
     frameTime.tv_sec = p->begin;
     frameTime.tv_usec = 0;
     firstTime = p->begin;

     bottomLine = cameraModule.cameraHeight / 8 - 1;


     /* if we have 16 bit data but are writing a png, do nothing */
     if( is16bit && writePNG ) {
     }
     /* otherwise we always convert into RGB */
     else {
          deBayer( RGB, p->snapPtr, &cameraModule );

          if( FullRGB ) {
               fprintf( logFD, "stuffing into full\n" );
               baseIndex = 0;
               for( i = cameraModule.top;
                    i < cameraModule.y + cameraModule.top; i++ ) {
                    index =
                         i * cameraModule.cameraWidth + cameraModule.left - 1;
                    for( j = cameraModule.left;
                         j < cameraModule.x + cameraModule.left; j++ ) {
                         FullRGB[index++] = RGB[baseIndex++];
                    }
               }
          }
     }

     sprintf( thisName, "%ld.c%ld.snap.%s", baseEpoch,
              cameraModule.cameraNumber, fileExt );

     /* get time zone name */
     myTmPtr = localtime( &baseEpoch );
     if( myTmPtr->tm_isdst == 0 ) 
	strcpy( myTz, tzname[0] );
     else if( myTmPtr->tm_isdst > 0 ) 
	strcpy( myTz, tzname[1] );
     else ;

     sprintf( line1,
              "%-s %-6s %-10s %s%-7s F: %s                     ",
              hotmParams.programVersion,
              "snap",
              hotmParams.sitename, ctime( &baseEpoch ), myTz,
              thisName );

     line1[cameraModule.cameraWidth / 8] = 0;

     sprintf( line2,
              "C: %ld S: %4d Qual: %3d Gain: %ld (%.2fdB) Shutter: %ld IntTime: %lf Name: %s                ",
              cameraModule.cameraNumber,
              1,
              80,
              cameraModule.gain,
              cameraModule.realGain,
              cameraModule.shutter, cameraModule.intTime,
              cameraModule.module );
     line2[cameraModule.cameraWidth / 8] = 0;

     if( is16bit && writePNG ) {        /* only time we have data in snapPtr */
          insertText16( 0, p->snapPtr, line1 );
          insertText16( bottomLine, p->snapPtr, line2 );
          savePNG( p->snapPtr, fullFileName, 16 );
     }
     else if( FullRGB ) {
          insertText( 0, FullRGB, line1 );
          insertText( bottomLine, FullRGB, line2 );
          if( writePNG )
               savePNG( FullRGB, fullFileName, 8 );
          else
               save24( FullRGB, fullFileName, qual );
     }
     else {
          insertText( 0, RGB, line1 );
          insertText( bottomLine, RGB, line2 );
          if( writePNG )
               savePNG( RGB, fullFileName, 8 );
          else
               save24( RGB, fullFileName, qual );
     }

     fprintf( logFD, "%.6f (%ld.%06ld) snap saved %s\n",
              firstTime, frameTime.tv_sec, frameTime.tv_usec, fullFileName );

     if( p->nFrames == 1 ) {
          freeAndClear( RGB );
          freeAndClear( FullRGB );
          //printf( "done with one frames\n" );
          return ( 1 );
     }

     baseEpoch++;

     if( gettimeofday( &xxstart, NULL ) )
          perror( "get time start" );

     lastTime = 1.0 * frameTime.tv_sec + frameTime.tv_usec / 1000000.0;
     period = ( lastTime - firstTime ) / ( frameCount - 1 );

     /* timex. */

     ptr = p->snapPtr;
     l1ptr = p->sumPtr;

     /* copy sum over into snap as we divide */
     for( i = 1; i < pixCount; i++ ) {
          *ptr++ = *l1ptr++ / frameCount;
     }

     /* if we have 16 bit data and are writing png, do nothing */
     if( is16bit && writePNG ) {
     }
     else {
          /* special conversion, deBayer the frame  */
          deBayer( RGB, p->snapPtr, &cameraModule );
          if( FullRGB ) {
               baseIndex = 0;
               for( i = cameraModule.top;
                    i < cameraModule.y + cameraModule.top; i++ ) {
                    index =
                         i * cameraModule.cameraWidth + cameraModule.left - 1;
                    for( j = cameraModule.left;
                         j < cameraModule.x + cameraModule.left; j++ ) {
                         FullRGB[index++] = RGB[baseIndex++];
                    }
               }
          }
     }

     sprintf( thisName, "%ld.c%ld.timex.%s",
              baseEpoch, cameraModule.cameraNumber, fileExt );

     sprintf( line1,
              "%-s %-6s %-10s %s%-7s F: %s                       ",
              hotmParams.programVersion,
              "timex",
              hotmParams.sitename, ctime( &baseEpoch ), myTz,
              thisName );

     baseEpoch++;

     line1[cameraModule.cameraWidth / 8] = 0;

     sprintf( line2,
              "C: %ld S: %4lu Q: %3d Per: %.3f G: %ld (%.2fdB) Sh: %ld Int: %f N: %s                 ",
              cameraModule.cameraNumber,
              frameCount,
              qual,
              frameInterval,
              cameraModule.gain,
              cameraModule.realGain,
              cameraModule.shutter, cameraModule.intTime,
              cameraModule.module );
     line2[cameraModule.cameraWidth / 8] = 0;


     if( is16bit && writePNG ) {        /* data is in snapPtr */
          insertText16( 0, p->snapPtr, line1 );
          insertText16( bottomLine, p->snapPtr, line2 );
          savePNG( p->snapPtr, fullFileName, 16 );
     }
     else if( FullRGB ) {
          insertText( 0, FullRGB, line1 );
          insertText( bottomLine, FullRGB, line2 );
          if( writePNG )
               savePNG( FullRGB, fullFileName, 8 );
          else
               save24( FullRGB, fullFileName, qual );
     }
     else {
          insertText( 0, RGB, line1 );
          insertText( bottomLine, RGB, line2 );
          if( writePNG )
               savePNG( RGB, fullFileName, 8 );
          else
               save24( RGB, fullFileName, qual );
     }

     fprintf( logFD, "%.6f (%ld.%06ld) timex saved %s\n",
              firstTime, frameTime.tv_sec, frameTime.tv_usec, fullFileName );


     /* start variance image processing */
     ptr = p->snapPtr;
     l1ptr = p->sumPtr;
     l2ptr = p->sumsqPtr;

     /* pass 1 through data, find max SD for later scaling */
     for( i = 1; i < pixCount; i++ ) {
          maxSD = max( maxSD, stdDev( *l1ptr, *l2ptr, frameCount ) );
          l1ptr++;
          l2ptr++;
     }

     maxSD = ( is16bit ? 65535 : 255 ) / maxSD;

     ptr = p->snapPtr;
     l1ptr = p->sumPtr;
     l2ptr = p->sumsqPtr;

     for( i = 1; i < pixCount; i++ ) {
          *ptr++ = maxSD * stdDev( *l1ptr, *l2ptr, frameCount );
          l1ptr++;
          l2ptr++;
     }

     if( is16bit && writePNG ) {        /* do nothing here */
     }
     else {
          deBayer( RGB, p->snapPtr, &cameraModule );
          if( FullRGB ) {
               baseIndex = 0;
               for( i = cameraModule.top;
                    i < cameraModule.y + cameraModule.top; i++ ) {
                    index =
                         i * cameraModule.cameraWidth + cameraModule.left - 1;
                    for( j = cameraModule.left;
                         j < cameraModule.x + cameraModule.left; j++ ) {
                         FullRGB[index++] = RGB[baseIndex++];
                    }
               }
          }
     }


     sprintf( thisName, "%ld.c%ld.var.%s",
              baseEpoch, cameraModule.cameraNumber, fileExt );

     sprintf( line1,
              "%-s %-6s %-10s %s%-7s F: %s                       ",
              hotmParams.programVersion,
              "stdDev",
              hotmParams.sitename, ctime( &baseEpoch ), myTz,
              thisName );

     line1[cameraModule.cameraWidth / 8] = 0;

     sprintf( line2,
              "C: %ld S: %4lu Q: %3d Per: %.3f Sc: %.3f G: %ld (%.2fdB) S: %ld Int: %f N: %s                    ",
              cameraModule.cameraNumber,
              frameCount,
              qual,
              frameInterval,
              maxSD,
              cameraModule.gain,
              cameraModule.realGain,
              cameraModule.shutter, cameraModule.intTime,
              cameraModule.module );

     line2[cameraModule.cameraWidth / 8] = 0;

     if( is16bit && writePNG ) {
          insertText16( 0, p->snapPtr, line1 );
          insertText16( bottomLine, p->snapPtr, line2 );
          savePNG( p->snapPtr, fullFileName, 16 );
     }
     else if( FullRGB ) {
          insertText( 0, FullRGB, line1 );
          insertText( bottomLine, FullRGB, line2 );
          if( writePNG )
               savePNG( FullRGB, fullFileName, 8 );
          else
               save24( FullRGB, fullFileName, qual );
     }
     else {
          insertText( 0, RGB, line1 );
          insertText( bottomLine, RGB, line2 );
          if( writePNG )
               savePNG( RGB, fullFileName, 8 );
          else
               save24( RGB, fullFileName, qual );
     }


     fprintf( logFD, "%.6f (%ld.%06ld) var saved %s\n",
              firstTime, frameTime.tv_sec, frameTime.tv_usec, fullFileName );


     baseEpoch++;

     /* bright */


     if( is16bit && writePNG ) {
     }
     else {
          deBayer( RGB, p->brightPtr, &cameraModule );

          if( FullRGB ) {
               baseIndex = 0;
               for( i = cameraModule.top;
                    i < cameraModule.y + cameraModule.top; i++ ) {
                    index =
                         i * cameraModule.cameraWidth + cameraModule.left - 1;
                    for( j = cameraModule.left;
                         j < cameraModule.x + cameraModule.left; j++ ) {
                         FullRGB[index++] = RGB[baseIndex++];
                    }
               }
          }
     }


     sprintf( thisName, "%ld.c%ld.bright.%s",
              baseEpoch, cameraModule.cameraNumber, fileExt );

     sprintf( line1,
              "%-s %-6s %-10s %s%-7s F: %s                   ",
              hotmParams.programVersion,
              "bright",
              hotmParams.sitename, ctime( &baseEpoch ), myTz,
              thisName );

     line1[cameraModule.cameraWidth / 8] = 0;

     sprintf( line2,
              "C: %ld S: %4lu Q: %3d Per: %.3f          G: %ld (%.2f dB) S: %ld Int: %lf N: %s                   ",
              cameraModule.cameraNumber,
              frameCount,
              qual,
              frameInterval,
              cameraModule.gain,
              cameraModule.realGain,
              cameraModule.shutter, cameraModule.intTime,
              cameraModule.module );
     line2[cameraModule.cameraWidth / 8] = 0;

	fprintf( logFD, "%f: %s\n", cameraModule.realGain, line2 );

     if( is16bit && writePNG ) {
          insertText16( 0, p->brightPtr, line1 );
          insertText16( bottomLine, p->brightPtr, line2 );
          savePNG( p->brightPtr, fullFileName, 16 );
     }
     else if( FullRGB ) {
          insertText( 0, FullRGB, line1 );
          insertText( bottomLine, FullRGB, line2 );
          if( writePNG )
               savePNG( FullRGB, fullFileName, 8 );
          else
               save24( FullRGB, fullFileName, qual );
     }
     else {
          insertText( 0, RGB, line1 );
          insertText( bottomLine, RGB, line2 );
          if( writePNG )
               savePNG( RGB, fullFileName, 8 );
          else
               save24( RGB, fullFileName, qual );
     }

     fprintf( logFD, "%.6f (%ld.%06ld) bright saved %s\n",
              firstTime, frameTime.tv_sec, frameTime.tv_usec, fullFileName );

     baseEpoch++;


     if( is16bit && writePNG ) {
     }
     else {
          deBayer( RGB, p->darkPtr, &cameraModule );

          if( FullRGB ) {
               baseIndex = 0;
               for( i = cameraModule.top;
                    i < cameraModule.y + cameraModule.top; i++ ) {
                    index =
                         i * cameraModule.cameraWidth + cameraModule.left - 1;
                    for( j = cameraModule.left;
                         j < cameraModule.x + cameraModule.left; j++ ) {
                         FullRGB[index++] = RGB[baseIndex++];
                    }
               }
          }
     }


     sprintf( thisName, "%ld.c%ld.dark.%s",
              baseEpoch, cameraModule.cameraNumber, fileExt );

     sprintf( line1,
              "%-s %-6s %-10s %s%-7s F: %s                   ",
              hotmParams.programVersion,
              "dark",
              hotmParams.sitename, ctime( &baseEpoch ), myTz,
              thisName );

     line1[cameraModule.cameraWidth / 8] = 0;

     sprintf( line2,
              "C: %ld S: %4lu Q: %3d Per: %.3f          G: %ld (%.2fdB) S: %ld Int: %lf N: %s                   ",
              cameraModule.cameraNumber,
              frameCount,
              qual,
              frameInterval,
              cameraModule.gain,
              cameraModule.realGain,
              cameraModule.shutter, cameraModule.intTime,
              cameraModule.module );
     line2[cameraModule.cameraWidth / 8] = 0;

	fprintf( logFD, "%f: %s\n", cameraModule.realGain, line2 );

     if( is16bit && writePNG ) {
          insertText16( 0, p->darkPtr, line1 );
          insertText16( bottomLine, p->darkPtr, line2 );
          savePNG( p->darkPtr, fullFileName, 16 );
     }
     else if( FullRGB ) {
          insertText( 0, FullRGB, line1 );
          insertText( bottomLine, FullRGB, line2 );
          if( writePNG )
               savePNG( FullRGB, fullFileName, 8 );
          else
               save24( FullRGB, fullFileName, qual );
     }
     else {
          insertText( 0, RGB, line1 );
          insertText( bottomLine, RGB, line2 );
          if( writePNG )
               savePNG( RGB, fullFileName, 8 );
          else
               save24( RGB, fullFileName, qual );
     }

     fprintf( logFD, "%.6f (%ld.%06ld) dark saved %s\n",
              firstTime, frameTime.tv_sec, frameTime.tv_usec, fullFileName );

     /* new running dark */
     /* convert double to 8 bit first, might as well use darkPtr */
     { long ii;
	for( ii=0; ii<pixCount; ii++ ) 
		(p->darkPtr)[ii] = (p->runDarkPtr)[ii];
     }
     /* now image is in darkPtr */

	baseEpoch++;

     if( is16bit && writePNG ) {
     }
     else {
          deBayer( RGB, p->darkPtr, &cameraModule );

          if( FullRGB ) {
               baseIndex = 0;
               for( i = cameraModule.top;
                    i < cameraModule.y + cameraModule.top; i++ ) {
                    index =
                         i * cameraModule.cameraWidth + cameraModule.left - 1;
                    for( j = cameraModule.left;
                         j < cameraModule.x + cameraModule.left; j++ ) {
                         FullRGB[index++] = RGB[baseIndex++];
                    }
               }
          }
     }


     sprintf( thisName, "%ld.c%ld.rundark.%s",
              baseEpoch, cameraModule.cameraNumber, fileExt );

     sprintf( line1,
              "%-s %-6s %-10s %s%-7s F: %s                   ",
              hotmParams.programVersion,
              "rundark",
              hotmParams.sitename, ctime( &baseEpoch ), myTz,
              thisName );

     line1[cameraModule.cameraWidth / 8] = 0;

	/* for some reason I cannot imagine, other than compiler bug, if I try
 		writing RUNNINGK out as a %f in the next line2 sprintf, either it
		or the realGain is printed as -nan! odd */
     sprintf( lineTemp, "K: %.2f", (double)RUNNINGK );

	xxt = (double)RUNNINGK;

     sprintf( line2,
              "C: %ld S: %4lu Q: %3d Per: %.3f          G: %ld (%.2fdB) S: %ld Int: %f K: %f N: %s                ",
              cameraModule.cameraNumber,
              frameCount,
              qual,
              frameInterval,
              cameraModule.gain,
              (double)cameraModule.realGain,
              cameraModule.shutter, cameraModule.intTime,
		xxt+0.0,
              cameraModule.module);
     line2[cameraModule.cameraWidth / 8] = 0;

	fprintf( logFD, "%f: %s\n", cameraModule.realGain, line2 );

     if( is16bit && writePNG ) {
          insertText16( 0, p->darkPtr, line1 );
          insertText16( bottomLine, p->darkPtr, line2 );
          savePNG( p->darkPtr, fullFileName, 16 );
     }
     else if( FullRGB ) {
          insertText( 0, FullRGB, line1 ); insertText( bottomLine, FullRGB, line2 );
          if( writePNG )
               savePNG( FullRGB, fullFileName, 8 );
          else
               save24( FullRGB, fullFileName, qual );
     }
     else {
          insertText( 0, RGB, line1 );
          insertText( bottomLine, RGB, line2 );
          if( writePNG )
               savePNG( RGB, fullFileName, 8 );
          else
               save24( RGB, fullFileName, qual );
     }

     fprintf( logFD, "%.6f (%ld.%06ld) rundark saved %s\n",
              firstTime, frameTime.tv_sec, frameTime.tv_usec, fullFileName );


     if( gettimeofday( &xxend, NULL ) )
          perror( "get time end" );

     fprintf( logFD, "%lu frames over %f seconds ", frameCount,
              p->lastFrameProcessedTime - firstTime );

     firstTime = 1.0 * xxstart.tv_sec + ( xxstart.tv_usec / 1000000.0 );
     lastTime = 1.0 * xxend.tv_sec + ( xxend.tv_usec / 1000000.0 );

     fprintf( logFD, "processed in %f seconds\n", lastTime - firstTime );

     freeAndClear( RGB );
     freeAndClear( FullRGB );
     fflush( NULL );
}
