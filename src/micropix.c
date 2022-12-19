/*
	micropix.c -- the camera module for the micropix color cameras
                      also DragonFly and Flea
	
	copied from sony.c, v.1.6

	loaded dynamically by hotm. contains the following functions:
	
	_init -- called by dlopen by default, must set up camera
	_startCamera -- turn on collection using previously determined
		parameters
	_stopCamera  -- turn camera off
	_getFrame    -- get the next frame please
	
*/

/* 
 *  Copied from xcdsx900.c
 *
 *  $Log: micropix.c,v $
 *  Revision 1.12  2016/03/25 00:01:23  stanley
 *  add node and generation printout
 *
 *  Revision 1.11  2013/11/11 22:17:18  root
 *  added hflip, allocbuffs
 *
 *  Revision 1.10  2012/11/30 03:17:03  root
 *  version for libdc1394 v.2. many many many changes
 *
 *  Revision 1.9  2010/04/29 22:44:33  stanley
 *  added some tVr (camera reg) checks, 1394B mode for speed > 400
 *
 *  Revision 1.8  2010/04/02 19:15:38  stanley
 *  many many format 7 and DCAM 1.31 and Pt. Grey changes.
 *
 *  Revision 1.7  2009/12/11 20:53:22  stanley
 *  added bump of shutter/gain if scale>1 and change too small to occur.
 *
 *  Revision 1.6  2009/02/20 21:12:27  stanley
 *  add "raw data" encoding in first 8 quadlets, get pixel order from camera
 *
 *  Revision 1.5  2006/04/14 23:37:53  stanley
 *  added status on dma failure
 *
 *  Revision 1.4  2005/01/29 00:42:34  stanley
 *  quieted down some messages
 *
 *  Revision 1.3  2004/12/17 21:30:20  stanley
 *  various fixed for use of real gain/shutter, bug in gain on micropix
 *
 *  Revision 1.2  2004/12/11 00:59:56  stanley
 *  modify to use real shutter gain, _setCamera _getCamera
 *
 *  Revision 1.1  2004/12/06 21:48:46  stanley
 *  Initial revision
 *
 *  Revision 1.6  2004/03/16 20:31:08  stanley
 *  added a lot of stuff, change gain, shutter, timings.
 *
 *  Revision 1.5  2003/05/01 21:48:39  stanley
 *  calc diff all the time, save for average report
 *
 *  Revision 1.4  2003/03/14 21:11:44  stanley
 *  fixes to several things, better verbosity
 *
 *  Revision 1.3  2003/03/14 18:32:58  stanley
 *  get auto-set values back so they can be reported to user
 *
 *  Revision 1.2  2002/11/06 23:55:14  stanley
 *  added gain to things sent back
 *
 *  Revision 1.1  2002/11/06 21:24:48  stanley
 *  Initial revision
 *
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

static const char rcsid[] =
     "$Id: micropix.c 395 2018-09-20 21:51:05Z stanley $";

#include "hotm.h"
#include "alarm.h"
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>

#define CAMERAMODULE
#include "camera.h"

int _getCamera(  );


static char module[128];

raw1394handle_t rawHandle;

void mapCameraNameToData(  );

float
shutterToReal(  )
{
     return -1.0;
}

float
gainToReal(  )
{
     return -1.0;
}

#define HAS_FEATURE(x)   (cameraData.features.feature[x-DC1394_FEATURE_MIN].available)
#define HAS_REAL(x)	 (cameraData.features.feature[x-DC1394_FEATURE_MIN].absolute_capable)
#define MINBINVAL(x)	 (cameraData.features.feature[x-DC1394_FEATURE_MIN].min)
#define MAXBINVAL(x)	 (cameraData.features.feature[x-DC1394_FEATURE_MIN].max)
#define MINREALVAL(x)	 (cameraData.features.feature[x-DC1394_FEATURE_MIN].abs_min)
#define MAXREALVAL(x)	 (cameraData.features.feature[x-DC1394_FEATURE_MIN].abs_max)

#define featureOFF(x) status = dc1394_feature_set_power( cameraData.camera, \
						x, \
						DC1394_OFF )
#define featureON(x) status = dc1394_feature_set_power( cameraData.camera, \
						x, \
						DC1394_ON )
#define autoOFF(x) status = dc1394_feature_set_mode( cameraData.camera, \
						x, \
						DC1394_FEATURE_MODE_MANUAL )
#define autoON(x) status = dc1394_feature_set_mode( cameraData.camera, \
						x, \
						DC1394_FEATURE_MODE_AUTO )
#define featureSET(x,y) status = dc1394_feature_set_value( cameraData.camera, \
						x, \
						y )
#define featureGET(x,y) status = dc1394_feature_get_value( cameraData.camera, \
						x, \
						y )
#define featureRealSET(x,y) status = dc1394_feature_set_absolute_value( cameraData.camera, \
						x, \
						y )
#define featureRealGET(x,y) status = dc1394_feature_get_absolute_value( cameraData.camera, \
						x, \
						y )
#define featureRealOnOff(x,y) status = dc1394_feature_set_absolute_control( \
						cameraData.camera, \
						x, \
						y )

/*#define testStatus(x) DC1394_WRN( status, x ); */
#define testStatus(x) \
		if( status ) { \
			printf("cam %lu: %s line %d: %s\n", \
				cameraModule.cameraNumber, \
				cameraModule.module, \
				__LINE__, \
				x ); \
		}

#define DEFAULT(x,y)     if( 0 == x ) x = y

#define tVr \
		status = dc1394_get_control_register( cameraData.camera, \
				0x0628UL, \
				value);\
		testStatus( "cannot get 0x0628UL" ); \
		if( value[0]>>31 ) { \
			printf("VMODE ERROR in line %d\n", __LINE__ ); \
		}

#define PRINTFEATURES      dc1394_feature_print_all( &cameraData.features, stdout )

void
_startCamera( struct _cameraModule_ *cam )
{
     int status;

     if( cam->verbose & VERB_START ) {
          KIDID;
          printf( "cam %ld ", cam->cameraNumber );
          printf( "start camera called, module \"%s\" \n", cam->module );
     }


     defaultAlarm( "startCamera start iso", cam->module );
     status = dc1394_video_set_transmission( cameraData.camera, DC1394_ON );
     DC1394_ERR( status, "set transmission on!" );

     clearAlarm(  );

     cam->frameCount = 0;
     cam->sumDiff = 0.0;
     cameraData.lastFrame.tv_sec = cameraData.lastFrame.tv_usec = 0;
}

/* a generic cleanup routine now. called by error handlers */
/* I don't know what HAS been started, so I don't care if I */
/* fail at stopping them. */
void
_stopCamera(  )
{
     int status;

     /* stop capturing anything */
     dc1394_capture_stop( cameraData.camera );

     /* stop ISO if started. */
     dc1394_video_set_transmission( cameraData.camera, DC1394_OFF );

     /* release ISO channel, if allocated */
     dc1394_iso_release_channel( cameraData.camera,
                                 cameraModule.cameraNumber );
     /* turn camera off */
     //dc1394_camera_set_power (cameraData.camera, DC1394_OFF);

     /* all done! might as well exit, we can do nothing more here */
     exit( -1 );

}

struct _frame_ *
_getFrame( struct _cameraModule_ *cam )
{
     int err;
     double diff;
     union
     {
          struct
          {
               unsigned int offset:12;
               unsigned int cycles:13;
               unsigned int secs:7;
          } cooked;
          quadlet_t raw;
     } ft, fc1, fc2;
     struct timeval now, filltime;
     dc1394video_frame_t *myFrame;
     double dft, dfc1, dfc2;
     double odft, odfc1, odfc2;
     int status;
     uint64_t localRawTime;
     long localRawSecs;
     long localRawUSecs;

     cameraData.testFrame.status = 0;

     defaultAlarm( "getFrame single capture", cam->module );
/* alarm fails for some reason, try a loop? */
//     err = dc1394_capture_dequeue (cameraData.camera,
//                                  DC1394_CAPTURE_POLICY_WAIT, &myFrame);
     {
          unsigned long count;
          count = 20 * 20;      /* twenty makes one second, 20 seconds. */
          while( count-- ) {
               err = dc1394_capture_dequeue( cameraData.camera,
                                             DC1394_CAPTURE_POLICY_POLL,
                                             &myFrame );
               if( myFrame )
                    break;
               MESLEEP( 50000000 );     /* 50 million ns == 50ms */
          }

          if( count == 0 ) {    /* timed out */
               _stopCamera(  );
               _exit( -1 );
          }

     }

     clearAlarm(  );
     DC1394_ERR( err, "capture dequeue" );

     if( myFrame->frames_behind > 0 ) {
          printf( "I am %d frames behind...\n", myFrame->frames_behind );
     }

     /* frame time in 1394 bus time from image */
     ft.raw = htonl( *( quadlet_t * ) myFrame->image );

#ifdef NEVER
     {
          int i;
          printf( "raw data: " );
          for( i = 0; i < 32; i++ )
               printf( "%02x ", myFrame->image[i] );
          printf( "\n" );
     }
#endif



     /* set to absurd invalid value so I can detect fails later */
     fc1.raw = fc2.raw = -1;

     /* find out mapping from now to 1394 bus time */
#ifdef CYCLEFROMCAMERA
     status = dc1394_get_register( cameraData.camera,
                                   CSR_CYCLE_TIME, &fc1.raw );
     testStatus( "first raw read failed" );
#else
     status = raw1394_read_cycle_timer( rawHandle, &fc1.raw, &localRawTime );
     if( status ) {
          printf( "trying raw cycle failed: code %d\n", status );
     }
     //printf("raw cycle: %lld time %d secs %d count %d offset\n", 
//              localRawTime, 
//              fc1.cooked.secs,
//              fc1.cooked.cycles,
//              fc1.cooked.offset );
#endif

     /*gettimeofday( &cameraData.camera.filltime, NULL ); */
     gettimeofday( &now, NULL );

#ifdef CYCLEFROMCAMERA
     status = dc1394_get_register( cameraData.camera,
                                   CSR_CYCLE_TIME, &fc2.raw );
     testStatus( "second raw read failed" );
#else
     status = raw1394_read_cycle_timer( rawHandle, &fc2.raw, &localRawTime );
     if( status ) {
          printf( "trying raw cycle failed: code %d\n", status );
     }
//     printf("raw cycle: %lld time %d secs %d count %d offset\n", 
//              localRawTime, 
//              fc2.cooked.secs,
//              fc2.cooked.cycles,
//              fc2.cooked.offset );
#endif

     //fc1.raw = htonl (fc1.raw);
     //fc2.raw = htonl (fc2.raw);

     /* some error processing, what do I do? */
     if( ( fc2.raw == -1 ) && ( fc1.raw != -1 ) )
          fc2.raw = fc1.raw;    /* got 1, not 2. Use 1 for both */
     if( ( fc1.raw == -1 ) && ( fc2.raw != -1 ) )
          fc1.raw = fc2.raw;    /* got 2, not 1, use 2 for both */

     /* filltime from system, convert back to timeval */
     filltime.tv_sec = myFrame->timestamp / 1000000;
     filltime.tv_usec = myFrame->timestamp % 1000000;

     /* if I got one or both camera cycle times, use NOW for filltime, 
        correct for camera to now timing. Otherwise, leave filltime alone
        and set camera times to frame time so correction becomes 0 */
     if( ( fc1.raw != -1 ) && ( fc2.raw != -1 ) ) {
          filltime = now;
          //    fc1.raw = htonl (fc1.raw);
          //    fc2.raw = htonl (fc2.raw);
     }
     else {
          fc1.raw = fc2.raw = ft.raw;
     }

#define DECODE_CYCLES(x) ( x.cooked.secs \
			+ (double)(x.cooked.cycles) / 8000 \
			+ (double)(x.cooked.offset) / 8000 / 3072 )

     odfc1 = dfc1 = DECODE_CYCLES( fc1 );
     odfc2 = dfc2 = DECODE_CYCLES( fc2 );
     odft = dft = DECODE_CYCLES( ft );

     /* there may be a rollover between second and first time */
     /* bus cycle rolls over at 128. correct for this by adding 128 */
     /* if second is smaller than first. BUT -- on rare occasions */
     /* the second is a tiny bit smaller than second. Cause has yet */
     /* to be determined. So, if there is a rollover, the difference */
     /* will be large. I've only seen a few hundred 'offsets', but */
     /* allow a ten second error */
     if( ( dfc2 + 10 ) < dfc1 ) /* rollover of 128 seconds */
          dfc2 += 128;
     dfc1 = ( dfc1 + dfc2 ) / 2;        /* average 1394 bus time at 'now' */
     if( dfc1 < dft )           /* rollover between frame and time check */
          dfc1 += 128;
     diff = dfc1 - dft;

     /* subtract  bus/system diff time from frame time */
     filltime.tv_usec -= ( diff * 1000000.0 );
     /* subtract int time from frame time */
     filltime.tv_usec -= ( cam->intTime / 2 * 1000000.0 );

     /* normalize time */
     while( filltime.tv_usec < 0 ) {
          filltime.tv_usec += 1000000;
          filltime.tv_sec--;
     }

     diff = filltime.tv_sec - cameraData.lastFrame.tv_sec;
     diff += ( filltime.tv_usec - cameraData.lastFrame.tv_usec )
          / 1000000.0;

     localRawUSecs = localRawTime % 1000000;
     localRawSecs = localRawTime / 1000000.0;

     if( ( ( odfc2 < odfc1 ) && ( ( odfc1 - odfc2 ) < 10 ) )
         || ( cam->verbose & VERB_TIMING ) ) {
          printf( "getframe %5ld at %ld.%06ld (%ld.%06ld)", cam->frameCount,
                  filltime.tv_sec,
                  filltime.tv_usec, localRawSecs, localRawUSecs );
          printf( " diff %.4f\n", diff );

          printf( "           ft: %3d %04d %04d odft:  %13.9f\n",
                  ft.cooked.secs, ft.cooked.cycles, ft.cooked.offset, odft );
          printf( "          fc1: %3d %04d %04d odfc1: %13.9f\n",
                  fc1.cooked.secs, fc1.cooked.cycles, fc1.cooked.offset,
                  odfc1 );
          printf( "          fc2: %3d %04d %04d odfc2: %13.9f\n",
                  fc2.cooked.secs, fc2.cooked.cycles, fc2.cooked.offset,
                  odfc2 );
     }


     /* skip when difference is too long -- bogus number! */
     if( diff < 1000.0 ) {
          cam->sumDiff += diff;
          cam->frameCount++;
     }

     if( cam->verbose & VERB_GETFRAME ) {
          KIDID;
          printf( "cam %ld ", cam->cameraNumber );
          printf( "getframe %5ld at %ld.%06ld",
                  cam->frameCount, filltime.tv_sec, filltime.tv_usec );
          printf( " ft: %3d %04d %04d", ft.cooked.secs, ft.cooked.cycles,
                  ft.cooked.offset );

          printf( " diff %.4f", diff );
          printf( " int: %.4f", cam->intTime );
          printf( " gain: %.2f\n", cam->realGain );
     }

     cameraData.lastFrame = filltime;
     cameraData.testFrame.dataPtr = ( unsigned char * ) myFrame->image;
     cameraData.testFrame.when = filltime;
     cameraData.testFrame.infoPtr = ( void * ) myFrame;

     return &cameraData.testFrame;

}

void
_releaseFrame(  )
{
     int status;

     defaultAlarm( "releaseFrame done with buffer", "micropix module" );

     status = dc1394_capture_enqueue( cameraData.camera,
                                      cameraData.testFrame.infoPtr );
     clearAlarm(  );

     testStatus( "releaseFrame: failed to release" );

}


/* set camera parameter. even though params are INTs, we pass in a */
/* double so we can do ADJUST. double has more than enough precision */
/* for holding int values when we SET */
int
_setCamera( char *caction, char *cfeature, double value )
{
     int action;
     int feature;
     unsigned int ltemp;
     unsigned int prev;
     long status;
     float ftemp;
     double dvalue;
     int i;
     unsigned long R, B;

     action = dc1394Decode( caction );
     feature = dc1394Decode( cfeature );

     if( feature == DC1394_FEATURE_NULL )
          return -1;

     if( cameraModule.verbose & VERB_SET ) {
          printf( "setCam: action %s (%d) feature: %s (%d) value: %f\n",
                  caction, action, cfeature, feature, value );
     }

     /* will this work for trigger mode? Must verify ... */
     if( action == CAM_OFF ) {
          featureOFF( feature );
          return 1;
     }

     if( ( feature > DC1394_FEATURE_MIN ) && HAS_REAL( feature ) )
          featureRealOnOff( feature, DC1394_ON );

/*	printf("setcamera: action %d feature %d value %f\n", action,
 *	feature, value ); */

     /* flag here, value later, if >0, worry about previous value */
     prev = 0;

     switch ( feature ) {

               /* all of these behave the same */
          case DC1394_FEATURE_BRIGHTNESS:
          case DC1394_FEATURE_EXPOSURE:
          case DC1394_FEATURE_SHARPNESS:
          case DC1394_FEATURE_HUE:
          case DC1394_FEATURE_SATURATION:
          case DC1394_FEATURE_GAMMA:
          case DC1394_FEATURE_IRIS:
          case DC1394_FEATURE_FOCUS:
          case DC1394_FEATURE_TEMPERATURE:
          case DC1394_FEATURE_TRIGGER_DELAY:
          case DC1394_FEATURE_WHITE_SHADING:
          case DC1394_FEATURE_FRAME_RATE:
          case DC1394_FEATURE_ZOOM:
          case DC1394_FEATURE_PAN:
          case DC1394_FEATURE_TILT:
          case DC1394_FEATURE_OPTICAL_FILTER:
          case DC1394_FEATURE_CAPTURE_SIZE:
          case DC1394_FEATURE_CAPTURE_QUALITY:
          case DC1394_FEATURE_SHUTTER:
          case DC1394_FEATURE_GAIN:

               if( HAS_FEATURE( feature ) == DC1394_FALSE )
                    return -1;

               ltemp = value;
               ftemp = value;


               switch ( action ) {


                    case CAM_SCALE:
                    case CAM_ADD:
                         featureGET( feature, &ltemp ); /* ALWAYS want binary value */
                         testStatus( "scale binval get" );
                         if( HAS_REAL( feature ) ) {
                              featureRealOnOff( feature, 1 );
                              testStatus( "scale real on" );
                              featureRealGET( feature, &ftemp );
                              testStatus( "scale real get" );
                              if( action == CAM_ADD )
                                   ftemp = ftemp + value;
                              else
                                   ftemp = ftemp * value;
                         }
                         else {
                              prev = ltemp;     /* old value saved for compare later */
                              if( action == CAM_ADD )
                                   ltemp = ltemp + value;
                              else
                                   ltemp = ltemp * value;
                         }

                         /* fall through! */
                    case CAM_SET:

                         /* if I'm setting it, turn off AUTO, turn feature ON, and */
                         /* make it REAL if capable */
                         autoOFF( feature );
                         testStatus( "set auto off" );
                         featureON( feature );
                         testStatus( "set feature on" );

                         if( HAS_REAL( feature ) ) {
                              if( ftemp > MAXREALVAL( feature ) )
                                   ftemp = MAXREALVAL( feature );
                              if( ftemp < MINREALVAL( feature ) )
                                   ftemp = MINREALVAL( feature );
                              featureRealSET( feature, ftemp );
                              testStatus( "set real set" );
                         }
                         else {
                              if( ltemp > MAXBINVAL( feature ) )
                                   ltemp = MAXBINVAL( feature );
                              if( ltemp < MINBINVAL( feature ) )
                                   ltemp = MINBINVAL( feature );
                              featureSET( feature, ltemp );
                              testStatus( "set binval set" );
                         }

                         if( prev ) {   /* here >0 is flag, convert to previous value */
                              featureGET( feature, &ltemp );
                              testStatus( "scale bump get" );
                              if( prev == ltemp ) {     /* didn't get incremented! */
                                   ltemp += 1;  /* increment it one */
                                   featureRealOnOff( feature, 0 );
                                   testStatus( "scale bump real off" );
                                   featureSET( feature, ltemp );
                                   testStatus( "previous bumper" );
                                   featureRealOnOff( feature, 1 );
                                   testStatus( "scale bump real on" );
                              }
                         }

                         break;
                    case CAM_AUTO:
                         /* I'm setting it to AUTO */
                         autoON( feature );
                         break;
                    default:
                         printf( "unknown action code %d\n", action );
                         return -1;
               }

               /* now, get the values back for the cameraModule */
               featureGET( feature, &ltemp );
               testStatus( "setCamera reget" );
               ftemp = ltemp;

               if( HAS_REAL( feature ) ) {
                    featureRealGET( feature, &ftemp );
                    testStatus( "setCamera reget real" );
               }

               if( feature == DC1394_FEATURE_SHUTTER ) {
                    cameraModule.shutter = ltemp;
                    cameraModule.intTime =
                         HAS_REAL( feature ) ? ftemp : shutterToReal( ltemp );
               }
               if( feature == DC1394_FEATURE_GAIN ) {
                    cameraModule.gain = ltemp;
                    cameraModule.realGain =
                         HAS_REAL( feature ) ? ftemp : gainToReal( ltemp );
               }
               break;

          case DC1394_FEATURE_WHITE_BALANCE:
               /* this is the encoded value of White balance */
               /* there is no scale or add, just set */
               /* special case, value of 0 turns it off */
               if( value == 0.0 )
                    featureOFF( feature );
               else {
                    featureON( feature );
                    ltemp = value;
                    featureSET( feature, ltemp );
               }
               break;

          case MY_FEATURE_WHITE_BALANCE_R:
               /* turn on white balance and set R */
               featureON( DC1394_FEATURE_WHITE_BALANCE );
               autoOFF( DC1394_FEATURE_WHITE_BALANCE );
               _getCamera( MY_FEATURE_WHITE_BALANCE_B, &dvalue );
               ltemp = value;
               B = dvalue;
               status =
                    dc1394_feature_whitebalance_set_value( cameraData.camera,
                                                           B, ltemp );
               testStatus( "white R set" );
               return 1;
               break;

          case MY_FEATURE_WHITE_BALANCE_B:
               /* turn on white balance and set B */
               featureON( DC1394_FEATURE_WHITE_BALANCE );
               autoOFF( DC1394_FEATURE_WHITE_BALANCE );
               _getCamera( MY_FEATURE_WHITE_BALANCE_R, &dvalue );
               ltemp = value;
               R = dvalue;
               status =
                    dc1394_feature_whitebalance_set_value( cameraData.camera,
                                                           ltemp, R );
               testStatus( "white B set" );
               return 1;
               break;


          case DC1394_FEATURE_TRIGGER:

               status = dc1394_external_trigger_set_mode( cameraData.camera,
                                                          value );
               testStatus( "trigger mode set" );
               status = dc1394_external_trigger_set_power( cameraData.camera,
                                                           DC1394_ON );
               testStatus( "trigger power on" );
               cameraData.tsource = 1;
               break;

          default:
               return -1;
               break;

     }

}

/* get camera parameter. */
/* NOTE: it is YOUR responsibility to know that the pointer points */
/* to a long enough result */
int
_getCamera( int feature, double *value )
{
     unsigned int ltemp;
     long status;
     float ftemp;
     int i;
     unsigned int R, B;

     switch ( feature ) {

               /* all of these behave the same */
          case DC1394_FEATURE_BRIGHTNESS:
          case DC1394_FEATURE_EXPOSURE:
          case DC1394_FEATURE_SHARPNESS:
          case DC1394_FEATURE_HUE:
          case DC1394_FEATURE_SATURATION:
          case DC1394_FEATURE_GAMMA:
          case DC1394_FEATURE_IRIS:
          case DC1394_FEATURE_FOCUS:
          case DC1394_FEATURE_TEMPERATURE:
          case DC1394_FEATURE_TRIGGER_DELAY:
          case DC1394_FEATURE_WHITE_SHADING:
          case DC1394_FEATURE_FRAME_RATE:
          case DC1394_FEATURE_ZOOM:
          case DC1394_FEATURE_PAN:
          case DC1394_FEATURE_TILT:
          case DC1394_FEATURE_OPTICAL_FILTER:
          case DC1394_FEATURE_CAPTURE_SIZE:
          case DC1394_FEATURE_CAPTURE_QUALITY:
          case DC1394_FEATURE_SHUTTER:
          case DC1394_FEATURE_GAIN:

               if( HAS_FEATURE( feature ) == DC1394_FALSE )
                    return -1;
               if( HAS_REAL( feature ) ) {
                    featureRealGET( feature, &ftemp );
                    *value = ftemp;
               }
               else {
                    featureGET( feature, &ltemp );
                    *value = ltemp;
               }
               return status;
               break;

          case MY_FEATURE_SHUTTER_REAL:

               if( HAS_REAL( DC1394_FEATURE_SHUTTER ) ) {
                    featureRealGET( DC1394_FEATURE_SHUTTER, &ftemp );
                    *value = ftemp;
               }
               else
                    return -1;  /* insert realToShutter here */

               return status;
               break;

          case MY_FEATURE_GAIN_REAL:
               if( HAS_REAL( DC1394_FEATURE_GAIN ) ) {
                    featureRealGET( DC1394_FEATURE_GAIN, &ftemp );
                    *value = ftemp;
               }
               else
                    return -1;  /* insert realToGain here */

               return status;
               break;

          case DC1394_FEATURE_WHITE_BALANCE:
               break;

          case MY_FEATURE_WHITE_BALANCE_R:
               /* return R value */
               status =
                    dc1394_feature_whitebalance_get_value( cameraData.camera,
                                                           &B, &ltemp );
               testStatus( "white get R" );
               *value = ltemp;
               return 1;
               break;

          case MY_FEATURE_WHITE_BALANCE_B:
               /* return R value */
               status =
                    dc1394_feature_whitebalance_get_value( cameraData.camera,
                                                           &ltemp, &R );
               testStatus( "white get B" );
               *value = ltemp;
               return 1;
               break;

          case DC1394_FEATURE_TRIGGER:
               break;

          default:
               return -1;
               break;

     }

}


void
x_init(  )
{
     struct _commandFileEntry_ *ptr;
     long size;
     int numCameras;
     int i;
     unsigned int ui;
     long camsOnBus = 1;
     int j;
     int foundCamera = 0;
     char line[128];
     char *tmp;
     int debug;
     int verbose;
     int status;
     long myValue;
     int portNode;
     dc1394bool_t yesNo;
     unsigned long B, R;
     float fvalue;
     unsigned int bpp;
     float myFrameRate;
     uint32_t value[3];
     long hflip = 0;
     long allocBuffs = 10;

     strcpy( module, "micropix" );
     strcpy( cameraModule.module, module );

     rawHandle = raw1394_new_handle_on_port( 0 );
     if( NULL == rawHandle ) {
          printf( "cannot get handle on port 0\n" );
     }

     /* init cameraData struct to all 0 */
     memset( &cameraData, 0, sizeof( cameraData ) );

     /* find out how verbose I am supposed to be */
     getLongParam( &cameraModule.verbose, "cameraverbose", module );
     verbose = cameraModule.verbose;

     /* stop verbosity if output is not a terminal! It makes me crash! */
     if( !isatty( 1 ) )
          verbose = cameraModule.verbose = 0;

     /* get my camera number */
     getLongParam( &cameraModule.cameraNumber, "cameraNumber", module );
     if( hotmParams.cameraNumber > -1 )
          cameraModule.cameraNumber = hotmParams.cameraNumber;

     /* find out how many other cameras there will be */
     getLongParam( &camsOnBus, "camerasOnBus", module );
     if( camsOnBus < 0 )
          camsOnBus = 1;

     /* how many dma buffers should I use? */
     getLongParam( &allocBuffs, "dmaBuffers", module );
     if( allocBuffs < 0 )
          allocBuffs = 10;

     /* look up the camera number to UID mapping from the file */
     tmp = mapCameraToUID( cameraModule.cameraNumber );
     if( NULL == tmp ) {
          printf( "%s: cannot map number %ld to UID. Giving up.\n",
                  module, cameraModule.cameraNumber );
          exit( -1 );
     }
     strcpy( cameraData.UID, tmp );

     /* connect to the 1393 system */
     cameraData.dc1394 = dc1394_new(  );


     /* getting camera info */
     mapUIDToPortAndNode( cameraData.UID, verbose & VERB_MAPPING );

     /* have camera, can I find "port"? */


     {
          dc1394error_t myerr;
          uint32_t node;
          uint32_t gen;


          myerr = dc1394_camera_get_node( cameraData.camera, &node, &gen );
          //fprintf( logFD, "node: %d gen: %d\n", node, gen );
     }


     /* report raw camera data, if verbosity */
     if( verbose & VERB_CAMERA ) {
          KIDID;
          dc1394_camera_print_info( cameraData.camera, stdout );
          printf( "\n" );
     }

     /* figure out what type of camera we have */
     mapCameraNameToData( 0, cameraData.camera->model );
     if( typeData.X == 0 ) {
          printf( "%s: unknown camera model %s, aborting...\n",
                  module, cameraData.camera->model );
          for( i = 0; cameraData.camera->model[i]; i++ )
               printf( "%02x ", cameraData.camera->model[i] );
          exit( -2 );
     }
     /* keep track of camera name for later */
     strcpy( cameraModule.module, typeData.name );

     get1394StringParam( &cameraData.format, "cameraFormat", module );
     DEFAULT( cameraData.format, DC1394_VIDEO_MODE_FORMAT7_0 );

     cameraModule.format =
          modeToFormat( getString( findCommand( "cameraColorCoding" ) ) );
     if( -1 == cameraModule.format )
          cameraModule.format = modeToFormat( typeData.defaultColorCoding );

     /* dc1394 v2 problem, need to save color coding. do it as mode */
     get1394StringParam( &cameraData.mode, "cameraColorCoding", module );
     DEFAULT( cameraData.mode, dc1394Decode( typeData.defaultColorCoding ) );

     /* before you waste your time trying to figure out why this 
        shouldn't work, notice that one "format" is cameraDATA and
        other is cameraMODULE. CameraDATA.format contains the FORMAT
        CODE for the camera, while the cameraMODULE.format contains
        an internal code for the DATA format. Those are two hours
        I'll never get back. Now go watch "My Spoon Is Too Big". */

     if( verbose & VERB_FORMAT ) {
          KIDID;
          printf
               ( "cam %ld data mode %ld interal format %d camera format %ld\n",
                 cameraModule.cameraNumber, cameraData.mode,
                 cameraModule.format, cameraData.format );
     }

     get1394StringParam( &cameraData.frameRate, "cameraFrameRate", module );
     DEFAULT( cameraData.frameRate,
              dc1394Decode( typeData.defaultFramerate ) );
     cameraModule.rate = cameraData.frameRate;

     cameraModule.top = cameraModule.left = 0;
     cameraModule.x = cameraModule.y = 0;

     getLongParam( &cameraModule.top, "cameraTop", module );
     getLongParam( &cameraModule.left, "cameraLeft", module );
     getLongParam( &cameraModule.x, "camerax", module );
     getLongParam( &cameraModule.y, "cameray", module );


     DEFAULT( cameraModule.x, typeData.X );
     DEFAULT( cameraModule.y, typeData.Y );
     DEFAULT( cameraModule.left, typeData.left );
     DEFAULT( cameraModule.top, typeData.top );
     DEFAULT( cameraModule.cameraWidth, typeData.referenceWidth );
     DEFAULT( cameraModule.cameraHeight, typeData.referenceHeight );

     cameraModule.maxY = typeData.referenceHeight;
     cameraModule.maxX = typeData.referenceWidth;

     /* default camera speed to 400 */
     cameraData.speed = DC1394_ISO_SPEED_400;
     get1394StringParam( &cameraData.speed, "cameraspeed", module );

     cameraModule.startCamera = _startCamera;
     cameraModule.stopCamera = _stopCamera;
     cameraModule.getFrame = _getFrame;
     cameraModule.releaseFrame = _releaseFrame;
     cameraModule.setCamera = _setCamera;
     cameraModule.getCamera = _getCamera;
     cameraModule.initCamera = x_init;

     defaultAlarm( "init init_camera", module );
     status = dc1394_camera_set_power( cameraData.camera, DC1394_ON );
     DC1394_ERR( status, "cannot turn power on" );
     MESLEEP( 1000000000 );

     status = dc1394_camera_reset( cameraData.camera );
     DC1394_ERR( status, "cannot reset" );
     clearAlarm(  );
     MESLEEP( 1000000000 );

     /* before I try to do anything else, let's try to release any
      * existing ISO allocations. Can't do bandwidth, it is unknown
      */
     status = dc1394_iso_release_channel( cameraData.camera,
                                          cameraModule.cameraNumber );

     /* now lets register our cleanup function */
     /* handled by hotm main, which registers an exit */

     status = dc1394_video_set_mode( cameraData.camera,
                                     DC1394_VIDEO_MODE_FORMAT7_0 );
     DC1394_ERR( status, "cannot set mode" );

     if( verbose & VERB_FORMAT ) {
          KIDID;
          printf( "IIDC version x code of camera: %d\n",
                  cameraData.camera->iidc_version );
     }
     if( cameraData.speed > DC1394_ISO_SPEED_400 )
          status =
               dc1394_video_set_operation_mode( cameraData.camera,
                                                DC1394_OPERATION_MODE_1394B );
     else
          status =
               dc1394_video_set_operation_mode( cameraData.camera,
                                                DC1394_OPERATION_MODE_LEGACY );
     DC1394_ERR( status, "failed setting operation mode" );

     status = dc1394_video_set_iso_speed( cameraData.camera,
                                          cameraData.speed );
     DC1394_ERR( status, "set iso speed" );

     status = dc1394_video_set_iso_channel( cameraData.camera,
                                            cameraModule.cameraNumber );
     DC1394_ERR( status, "set iso channel" );

     status =
          dc1394_video_set_framerate( cameraData.camera,
                                      cameraData.frameRate );
     DC1394_ERR( status, "setting framerate nominal" );

     /* get all the features for later testing */
     status =
          dc1394_feature_get_all( cameraData.camera, &cameraData.features );
     DC1394_ERR( status, "failed to get all features" );

     if( verbose & VERB_FEATURE )
          PRINTFEATURES;


#ifdef TESTING
     /* test -- what is default format 7 data? */
     status = dc1394_format7_get_image_size( cameraData.camera,
                                             DC1394_VIDEO_MODE_FORMAT7_0,
                                             &format7data.h_size,
                                             &format7data.v_size );
     printf( "format7 default sizes: %d x %d\n",
             format7data.h_size, format7data.v_size );
     status = dc1394_format7_get_image_position( cameraData.camera,
                                                 DC1394_VIDEO_MODE_FORMAT7_0,
                                                 &format7data.h_size,
                                                 &format7data.v_size );
     printf( "format7 default left: %d top: %d\n",
             format7data.h_size, format7data.v_size );
#endif

     /* get the format 7 info from the camera. Horse's mouth */
     status = dc1394_format7_get_max_image_size( cameraData.camera,
                                                 DC1394_VIDEO_MODE_FORMAT7_0,
                                                 &format7data.h_size,
                                                 &format7data.v_size );
     DC1394_ERR( status, "get max image size 7_0" );
     status = dc1394_format7_get_unit_size( cameraData.camera,
                                            DC1394_VIDEO_MODE_FORMAT7_0,
                                            &format7data.h_unit,
                                            &format7data.v_unit );
     DC1394_ERR( status, "get unit size 7_0" );
     status = dc1394_format7_get_color_codings( cameraData.camera,
                                                DC1394_VIDEO_MODE_FORMAT7_0,
                                                &format7data.codes );
     DC1394_ERR( status, "get color codings" );

     if( verbose & VERB_FORMAT ) {
          printf( "unit size is %ux%u\n", format7data.h_unit,
                  format7data.v_unit );
          printf( "max image size is %ux%u\n", format7data.h_size,
                  format7data.v_size );
          printf( "number of codings: %d, values: \n",
                  format7data.codes.num );
          for( i = 0; i < format7data.codes.num; i++ )
               printf( "     %s\n",
                       dc1394Encode( format7data.codes.codings[i] ) );
     }

     /* now figure out the basic settings and set them */

     _setCamera( "CAM_SET", "DC1394_FEATURE_FRAME_RATE",
                 framerateToDouble( cameraData.frameRate ) );
     _setCamera( "CAM_OFF", "DC1394_FEATURE_BRIGHTNESS", 0 );
     _setCamera( "CAM_OFF", "DC1394_FEATURE_EXPOSURE", 0 );
     _setCamera( "CAM_OFF", "DC1394_FEATURE_WHITE_BALANCE", 0 );
     _setCamera( "CAM_OFF", "DC1394_FEATURE_GAMMA", 0 );
     _setCamera( "CAM_SET", "DC1394_FEATURE_GAIN", 0.0 );
     _setCamera( "CAM_SET", "DC1394_FEATURE_SHUTTER", 0.01 );

     /* get all the features for later testing */
     status =
          dc1394_feature_get_all( cameraData.camera, &cameraData.features );
     DC1394_ERR( status, "failed to get all features" );

     /* we have a basic set of features that will allow camera ops */
     /* now set the specific things we want. */

     /* look for all the things that can be a feature */
     for( i = DC1394_FEATURE_MIN; i <= DC1394_FEATURE_MAX; i++ ) {

          double thisFeatureValue;      /* temp storage for value from decode */

          /* say it is unset */
          thisFeatureValue = FEATURE_UNSET;
          /* prepend "camera" to the param name */
          strcpy( line, "camera" );
          /* append the feature name */
          strcat( line, dc1394_feature_get_string( i ) );

          /* do a getParam on it */
          getDoubleParam( &thisFeatureValue, line, module );

          /* are we trying to set it? */
          if( FEATURE_UNSET != thisFeatureValue ) {
               if( !HAS_FEATURE( i ) ) {
                    printf( "init: trying to set missing feature: %s\n",
                            line );
               }
               else {
                    strcpy( line, "DC1394_FEATURE_" );
                    strcat( line, dc1394_feature_get_string( i ) );
                    _setCamera( "CAM_SET", line, thisFeatureValue );
               }

          }
     }
     /* get all the features for later testing */
     status =
          dc1394_feature_get_all( cameraData.camera, &cameraData.features );
     DC1394_ERR( status, "failed to get all features" );

     if( verbose & VERB_FEATURE )
          PRINTFEATURES;

     /* set all the format 7 parameters now */


     status =
          dc1394_video_set_framerate( cameraData.camera,
                                      cameraData.frameRate );
     DC1394_ERR( status, "setting framerate nominal" );


     status = dc1394_format7_set_roi( cameraData.camera,
                                      DC1394_VIDEO_MODE_FORMAT7_0,
                                      cameraData.mode,
                                      //DC1394_USE_MAX_AVAIL,
                                      cameraData.speed ==
                                      DC1394_ISO_SPEED_400 ? 4096 /
                                      camsOnBus : 8192 / camsOnBus,
                                      cameraModule.left, cameraModule.top,
                                      cameraModule.x, cameraModule.y );
     DC1394_ERR( status, "set roi" );


     status = dc1394_capture_setup( cameraData.camera, allocBuffs, 0 );
     DC1394_ERR( status, "capture setup!" );


     {
          unsigned int ppf;

          status = dc1394_format7_get_packets_per_frame( cameraData.camera,
                                                         DC1394_VIDEO_MODE_FORMAT7_0,
                                                         &ppf );
          testStatus( "get ppf" );
          cameraModule.xmitMicros = ppf * 125;

     }

     /* report back how big a "buffer" is */
     {
          uint64_t temp;
          dc1394_format7_get_total_bytes( cameraData.camera,
                                          DC1394_VIDEO_MODE_FORMAT7_0,
                                          &temp );
          cameraModule.bufferLength = temp;
     }

     /* report back some data on what I did when I setup */
     if( verbose & VERB_CAMERA ) {
          KIDID;
          printf( "cam %ld dma buffersize: %lu\n",
                  cameraModule.cameraNumber, cameraModule.bufferLength );
     }

     /* white balance */
     myValue = -1;
     getLongParam( &myValue, "camerawhitebalanceb", "" );
     if( myValue != -1 ) {
          B = myValue;
          _setCamera( "CAM_SET", "MY_FEATURE_WHITE_BALANCE_B", B );
          getLongParam( &myValue, "camerawhitebalancer", "" );
          R = myValue;
          _setCamera( "CAM_SET", "MY_FEATURE_WHITE_BALANCE_R", R );
     }

     /* turn on raw data */
     {
          uint64_t reg = 0x12f8;
          uint64_t val = 0x03ff;

          status = dc1394_set_control_register( cameraData.camera, reg, val );
          testStatus( "set frame embeded data" );

          reg = 0x1048;
          val = 0x0081;

          getLongParam( &hflip, "camerahflip", module );
          if( hflip )
               val = 0x0181;

          status = dc1394_set_control_register( cameraData.camera, reg, val );
          testStatus( "set raw bytes Y16 reg" );

     }

     {
          uint64_t reg = 0x1040;

          /* get pixel ordering, if there */
          status = dc1394_get_control_register( cameraData.camera,
                                                reg, value );
          testStatus( "get pixel order" );
     }

     value[1] = htonl( value[0] );
     memcpy( cameraModule.rawOrder, &value[1], 4 );
     value[2] = 0;
     if( verbose & VERB_CAMERA ) {
          KIDID;
          printf( "cam %ld byte order: %s\n",
                  cameraModule.cameraNumber, ( char * ) &value[1] );
     }


}
