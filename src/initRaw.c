/* initRaw -- initialize raw file */

/* write a text-based header into the raw file that can be used
 * later for processing.
 *
 * keyword value pairs. colon separator means value is numeric.
 *   semicolon means string. (Intended for matlab loading)
 * 
 * Mandatory keywords:
 *   bufferLength -- length in 8-bit bytes of each image frame
 
 * Optional but very nice keywords:
 *   cameraWidth, cameraHeight -- width and height of final image
 *   top, left -- one-based index of data in height/width final image
 *   x,y -- width of sub-image
 *   siteName -- Argus site (e.g. argus00)
 *   frameCount -- number of frames
 *   begin -- reference epoch time of first frame
 *   cameraFormat -- camera format 
 *   cameraMode -- camera mode
 *   cameraName -- name reported by camera (string)
 *   filename -- name of rawfile (as recorded)
 *   cameraGain -- camera-encoded gain value
 *   gain -- real gain in dB
 *   cameraShutter -- camera-encoded shutter value
 *   intTime -- integration time in seconds
 *   rawOrder -- pixel ordering in camera RGGB, BGGR, etc...


 * Mandatory non-keywords
 *  AIII rawfile -- starts every file
 *  end  -- end of header. Raw data starts after the next newline.

/*  
 * John Stanley
 * Coastal Imaging Lab
 * College of Oceanic and Atmospheric Sciences
 * Oregon State University
 * Corvallis, OR 97331
 *
 * $Copyright 2006 Argus Users Group$
 *
 * $Log: initRaw.c,v $
 * Revision 1.5  2016/03/24 23:11:58  stanley
 * change to demon2.h
 *
 * Revision 1.4  2011/11/10 00:46:27  root
 * format written from correct place now
 *
 * Revision 1.3  2010/04/02 19:10:27  stanley
 * add values needed for format 7 processing
 *
 * Revision 1.2  2009/02/20 21:15:46  stanley
 * changed version to 2.0, text header params
 *
 * Revision 1.1  2006/06/26 21:34:51  stanley
 * Initial revision
 *
 *
 */

static const char rcsid[] = "$Id: initRaw.c 359 2018-08-15 23:44:22Z stanley $";

#include "demon2.h"
#define doIt write( myfd, line, strlen(line) )

/* input: fd to write to, diskdata struct. impicit hotmParams struct
 *  and cameraModule struct */

/* output: int. ok: 0  error: 1 */

int initRaw( int myfd, struct _diskdata_ *myp )
{
char line[256];

	sprintf( line, "AIII rawfile 2.0\n" );
	doIt;

	sprintf( line, "bufferLength: %ld\n", cameraModule.bufferLength );
	doIt;

	sprintf( line, "frameCount: %ld\n", cameraModule.frameCount );
	doIt;

	sprintf( line, "programVersion; %s\n", hotmParams.programVersion );
	doIt;

	sprintf( line, "sitename; %s\n", hotmParams.sitename );
	doIt;

	sprintf( line, "begin: %lf\n", myp->begin );
	doIt;

	sprintf( line, "cameraName; %s\n", cameraModule.module);
	doIt;

	sprintf( line, "cameraFormat: %d\n", cameraModule.format );
	doIt;

	sprintf( line, "cameraMode: %ld\n", cameraData.mode );
 	doIt;

	sprintf( line, "filename; %s\n", myp->name );
	doIt;

	sprintf( line, "cameraX: %ld\n", cameraModule.x );
	doIt;

	sprintf( line, "cameraY: %ld\n", cameraModule.y );
	doIt;

	sprintf( line, "cameraTop: %ld\n", cameraModule.top );
	doIt;

	sprintf( line, "cameraLeft: %ld\n", cameraModule.left );
	doIt;

	sprintf( line, "maxY: %ld\n", cameraModule.maxY );
	doIt;

	sprintf( line, "maxX: %ld\n", cameraModule.maxX );
	doIt;

	sprintf( line, "cameraWidth: %ld\n", cameraModule.cameraWidth );
	doIt;

	sprintf( line, "cameraHeight: %ld\n", cameraModule.cameraHeight );
	doIt;

	sprintf( line, "cameraNumber: %ld\n", cameraModule.cameraNumber );
	doIt;

	sprintf( line, "cameraShutter: %ld\n", cameraModule.shutter );
	doIt;

	sprintf( line, "cameraGain: %ld\n", cameraModule.gain );
	doIt;
	
	sprintf( line, "gain: %lf\n", cameraModule.realGain );
	doIt;

	sprintf( line, "intTime: %lf\n", cameraModule.intTime );
	doIt;

	sprintf( line, "rawOrder; %s\n", cameraModule.rawOrder );
	doIt;


	sprintf( line, "end\n" );
	doIt;
	
	return 0;

}

