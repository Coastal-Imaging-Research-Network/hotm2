/* 
	gige.c -- the camera module for the PtGrey GigE using 
		flycap library.
	
	copied from micropix.c

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
     "$Id: micropix.c 172 2016-03-25 21:03:51Z stanley $";

#include "hotm.h"
#include "alarm.h"

#define CAMERAMODULE
#include "gige-camera.h"


/*
 * differences in flycap lib between latest ARM (2.9) and X86_64 (2.10)
 */
#ifdef __arm__
#define RESENDTIMEOUT timeoutForPacketResend
#define MAXRESEND maxPacketsToResend
#define HOSTPORT hostPost
#else
#define RESENDTIMEOUT registerTimeout
#define MAXRESEND registerTimeoutRetries
#define HOSTPORT hostPort
#endif

static char module[128];
extern FILE *logFD;
int haveConnected = 0;

void mapCameraNameToData(  );

//#define printf //printf

#define FC2_ERR_CHECK(x,y)	if(x != FC2_ERROR_OK) { \
			printf( "something failed: %s\n", y ); \
			printf( "reason: %d %s\n", x, fc2ErrorToDescription(x) );\
			if( logFD ) {\
			fprintf( logFD, "something failed: %s\n", y ); \
			fprintf( logFD, "reason: %d %s\n", x, fc2ErrorToDescription(x) );\
			} \
			exit(-1);	\
			}

#define testStatus(x) \
		if( status != FC2_ERROR_OK ) { \
			printf("cam %ld: %s line %d: %s\n", \
				cameraModule.cameraNumber, \
				cameraModule.module, \
				__LINE__, \
				(x) ); \
			printf( "cam %ld: reason: %d %s\n", \
				cameraModule.cameraNumber, \
				status, \
				fc2ErrorToDescription(status) );\
			if( haveConnected ) { \
			fc2Connect( cameraData.context, \
				&cameraData.guid ); \
			printf("cam %ld: reconnecting\n", \
				cameraModule.cameraNumber ); \
			} \
}

#define PRINTTHIS if(cameraModule.verbose & VERB_FEATURE)

#define GPRINT if(cameraModule.verbose & VERB_CAMERA)

#define DEFAULT(x,y)     if( 0 == x ) x = y

void
sleepy( unsigned int len )
{
     struct timespec nano, rem;
     int stat;

     nano.tv_sec = 0;
     nano.tv_nsec = len;

     stat = nanosleep( &nano, &rem );

     if( stat < 0 ) {
          printf( "failed nanosleep: leftover %ld.%09ld\n",
                  rem.tv_sec, rem.tv_nsec );
          perror( "error was" );
     }

}



/* fc2 -- start startCamera */

void
_startCamera( struct _cameraModule_ *cam )
{
     fc2Error status;

     if( cam->verbose & VERB_START ) {
          KIDID;
          printf( "cam %ld ", cam->cameraNumber );
          printf( "start camera called, module \"%s\" \n", cam->module );
     }


     SetTimeStamping( cameraData.context, 1 );

     defaultAlarm( "startCamera start iso", cam->module );
     status = fc2StartCapture( cameraData.context );
     testStatus( "failed to start, first try" );

     if( status != FC2_ERROR_OK ) {
          status = fc2StopCapture( cameraData.context );
          testStatus( "cannot stop capture in failed start" );
          sleep( 1 );
          status = fc2StartCapture( cameraData.context );
          FC2_ERR_CHECK( status, "set transmission on second try!" );
     }

     clearAlarm(  );

     /* check for MULTI_SHOT mode flag and turn that on now. */
     cameraData.useMultiShot = 0;
     getLongParam( &cameraData.useMultiShot, "usemultishot", "startCamera" );

     if( cameraData.useMultiShot ) {

          //Turn off ISO_EN to allow ONE_SHOT enabled
          const unsigned int k_ISO_EN = 0x614;
          unsigned int regVal = 0;      //Disable ISO_EN
          status = fc2WriteRegister( cameraData.context, k_ISO_EN, regVal );
          testStatus( "disable Continuous mode" );

          const unsigned int k_IMAGE_RETRANSMIT = 0x634;
          regVal = 0xc0000000;
          status =
               fc2WriteRegister( cameraData.context, k_IMAGE_RETRANSMIT,
                                 regVal );
          testStatus( "set retran mode" );

     }

     cam->frameCount = 0;
     cam->sumDiff = 0.0;
     cameraData.lastFrame.tv_sec = cameraData.lastFrame.tv_usec = 0;
}

/* fc2 -- end startCamera */

/* a generic cleanup routine now. called by error handlers */
/* I don't know what HAS been started, so I don't care if I */
/* fail at stopping them. */

int stopHasBeenCalled;

void
_stopCamera(  )
{
     fc2Error status;

     /* report my existence */
     if( cameraModule.verbose & VERB_STOP ) {
          KIDID;
          printf( "cam %ld ", cameraModule.cameraNumber );
          printf( "stop camera called, module \"%s\" \n",
                  cameraModule.module );
     }

     if( stopHasBeenCalled ) {
          /* i'm being recursive due to atexit. don't do me */
          //printf( "stop called for the second time: ignoring\n" );
          return;
     }
     stopHasBeenCalled++;

     /* stop capturing anything */
     status = fc2StopCapture( cameraData.context );
     FC2_ERR_CHECK( status, "cannot stop capture" );

     /* disconnect */
     status = fc2Disconnect( cameraData.context );
     FC2_ERR_CHECK( status, "cannot disconnect" );

     /* I'm exiting next, I don't think I need to destroy the image */

     /* all done! might as well exit, we can do nothing more here */
     exit( -1 );

}

fc2Error
fireTrigger(  )
{

     fc2Error status;
     struct timeval b4, after;
     double diff;

     gettimeofday( &b4, NULL );

     status = fc2WriteRegister( cameraData.context, 0x62c, 0x80000000 );

     gettimeofday( &after, NULL );
     diff =
          after.tv_sec - b4.tv_sec + ( after.tv_usec -
                                       b4.tv_usec ) / 1000000.0;
     if( status != FC2_ERROR_OK ) {
          printf( "cam %ld: %s line %d: %s\n",
                  cameraModule.cameraNumber,
                  cameraModule.module, __LINE__, "fireTrigger failed" );
          printf( "cam %ld: reason: %d %s\n", cameraModule.cameraNumber,
                  status, fc2ErrorToDescription( status ) );
     }
     if( diff > 0.002 )
          printf( "cam %ld: software trigger response time: %9.6f\n",
                  cameraModule.cameraNumber, diff );

     return status;

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
          uint32_t raw;
     } ft, fc1, fc2;

     union
     {
          unsigned int raw;
          struct
          {
               int NUM:12;
               int MNI:12;
               int UNK:6;
               int TDS:1;
               int IBC:1;
          } cooked;
     } retran;

     const unsigned int k_ONE_SHOT_MULTI_SHOT = 0x61c;
     const unsigned int k_IMAGE_RETRANSMIT = 0x634;
     unsigned int regVal;

     struct timeval now, filltime;
     struct timeval trig1, trig2;
     double dft, dfc1, dfc2;
     double odft, odfc1, odfc2;
     fc2Error status;
     uint64_t localRawTime;
     long localRawSecs;
     long localRawUSecs;
     fc2TimeStamp prevTimestamp;
     int dropped;

     struct timespec nano, rem;

     dropped = 0;

     if( cameraData.tsource == 7 ) {    /* software trigger */

          gettimeofday( &now, NULL );
          nano.tv_sec = 0;
          nano.tv_nsec =
               1000 * ( cameraData.interval -
                        ( now.tv_usec % cameraData.interval ) );

          /* printf( "nanosleep for %ld at %ld.%06ld ", nano.tv_nsec,
             now.tv_sec, now.tv_usec ); */

          if( 0 > nanosleep( &nano, &rem ) ) {
               printf( "failed nanosleep: rem = %ld\n", rem.tv_nsec );
          }

          gettimeofday( &now, NULL );
          //printf( "back at %ld.%06ld\n", now.tv_sec, now.tv_usec );
          /* time to software trigger me */

          /* don't trigger if something already waiting! */
          if( cameraData.useMultiShot ) {

               retran.raw = 0;
               status = fc2ReadRegister( cameraData.context,
                                         k_IMAGE_RETRANSMIT, &retran.raw );
               testStatus( "asking for image presence before trigger" );

               if( retran.cooked.NUM == 0 ) {
                    status = fireTrigger(  );
                    testStatus( "fire software trigger" );
                    /* get "now" -- this is when we TOLD it to take it */
                    /* is an approximation. */
                    gettimeofday( &now, NULL );
               }
          }
          else {
               status = fireTrigger(  );
               testStatus( "fire software trigger" );
               /* get "now" -- this is when we TOLD it to take it */
               /* is an approximation. */
               gettimeofday( &now, NULL );
          }
     }

     defaultAlarm( "getFrame single capture", cam->module );

     while( 1 ) {

          if( dropped > 100 ) {
               /* stop capturing anything */

               if( logFD )
                    fprintf( logFD,
                             "getFrame restarting camera due to dropped frames\n" );
               status = fc2StopCapture( cameraData.context );
               FC2_ERR_CHECK( status, "cannot stop capture" );

               /* and restart it */
               _startCamera( &cameraModule );
               dropped = 0;

          }

          /* if camera is in buffered mode, then wait for per-camera
             delay, then wait for image -- should be ready long before
             we ask, except for c1 */

          if( cameraData.useMultiShot ) {

               long count = 0;


               while( 1 ) {

                    retran.raw = 0;
                    status = fc2ReadRegister( cameraData.context,
                                              k_IMAGE_RETRANSMIT,
                                              &retran.raw );
                    testStatus( "asking for image presence" );
                    GPRINT {
                         printf
                              ( "useMulti: regVal: 0x%08x count: %ld wait: %ld ms\n",
                                retran.raw, count, 10 * count );
                    }

                    if( retran.cooked.NUM ) {

                         /* one is waiting! */
                         {
                              struct timeval xxx;
                              gettimeofday( &xxx, NULL );
                              GPRINT {
                                   printf( "before offset sleep: %ld.%06ld\n",
                                           xxx.tv_sec, xxx.tv_usec );
                              }
                         }
                         /* sleep for offset */
                         sleepy( cameraData.offset );
                         {
                              struct timeval xxx;
                              gettimeofday( &xxx, NULL );
                              GPRINT {
                                   printf( "after  offset sleep: %ld.%06ld\n",
                                           xxx.tv_sec, xxx.tv_usec );
                              }
                         }


                         /* now trigger it to be sent */
                         regVal = 0x40000001;
                         status =
                              fc2WriteRegister( cameraData.context,
                                                k_ONE_SHOT_MULTI_SHOT,
                                                regVal );
                         break; /* out of while(1) */
                    }

                    sleepy( 10000000 ); /* sleep for 10ms */

                    if( ( count++ > 25 )
                        && ( cameraData.tsource == 7 ) ) {      /* software trigger */
                         /* refire the trigger if we haven't gotten an image yet */
                         status = fireTrigger(  );
                         testStatus( "refired trigger" );
                         GPRINT {
                              printf( " cam %ld refired trigger\n",
                                      cameraModule.cameraNumber );
                         }
                         count = 0;
                    }
                    /*elseif( count++ > 1000 ) {
                       printf
                       ( "cam %ld: count in wait for frame in retran exceeded!\n",
                       cameraModule.cameraNumber );
                       break;
                       count = 0;
                       }
                     */
               }
          }
          else {
               /* external/free run, wait for return */
               sleepy( cameraData.offset );
          }

          status = fc2RetrieveBuffer( cameraData.context, &cameraData.frame );

          if( cameraData.useMultiShot ) {

               struct timeval xxx;
               gettimeofday( &xxx, NULL );
               GPRINT {
                    printf( "multi buffer back at: %ld.%06ld\n",
                            xxx.tv_sec, xxx.tv_usec );
               }
          }

          if( FC2_ERROR_IMAGE_CONSISTENCY_ERROR != status )
               testStatus( "getFrame retrieve" );

          //PrintImageInfo( 2, &cameraData.frame );
          if( FC2_ERROR_IMAGE_CONSISTENCY_ERROR == status ) {

               dropped++;
               continue;
          }

          if( FC2_ERROR_OK == status )
               break;

          FC2_ERR_CHECK( status, "capture dequeue" );
     }

     //printf( "---------------------\n" );
     clearAlarm(  );

     {
          // Get and print out the time stamp
          fc2TimeStamp ts = fc2GetImageTimeStamp( &cameraData.frame );
	
	 /* handle a camera that doesn't know the time! */
	  if( ts.seconds < 1000000 ) {   /* returns a low number */

		/* foo! is right. */
		struct timeval foo;
		gettimeofday( &foo, NULL );
		ts.seconds = foo.tv_sec; ts.microSeconds = foo.tv_usec;

	  }
		
          int diff =
               ( ts.cycleSeconds -
                 prevTimestamp.cycleSeconds ) * 8000 +
               ( ts.cycleCount - prevTimestamp.cycleCount );
          prevTimestamp = ts;
          if( cam->verbose & VERB_GETFRAME )
               printf( "timestamp %lld.%06d [%d %d] - %d\n",
                       ts.seconds, ts.microSeconds,
                       ts.cycleSeconds, ts.cycleCount, diff );

          /* sigh. if I am not software trigger, then timestamp from */
          /* frame is the best I can do for time */
          if( cameraData.tsource != 7 ) {
               now.tv_sec = ts.seconds;
               now.tv_usec = ts.microSeconds;

		now.tv_usec -= cameraModule.intTime*1000000;
		now.tv_usec -= cameraData.offset/1000;
		if( now.tv_usec < 0 ) {
			now.tv_sec--;
			now.tv_usec += 1000000;
		}
          }

     }

     /* sadly, I don't know this until I get a frame! */
     cameraModule.bufferLength = cameraData.frame.receivedDataSize;

     /* frame time in 1394 bus time from image */
     ft.raw = htonl( *( uint32_t * ) cameraData.frame.pData );

//#define DUMPRAWIMAGE
#ifdef DUMPRAWIMAGE
     {
          int i;
          printf( "raw data: " );
          for( i = 0; i < 32; i++ )
               printf( "%02x ", cameraData.frame.pData[i] );
          printf( "\n" );
     }
#endif

     /* time of now */
     if( now.tv_sec == 0 )
          gettimeofday( &now, NULL );
     filltime = now;

#define DECODE_CYCLES(x) ( x.cooked.secs \
			+ (double)(x.cooked.cycles) / 8000 \
			+ (double)(x.cooked.offset) / 8000 / 3072 )
     odft = dft = DECODE_CYCLES( ft );
     diff = filltime.tv_sec - cameraData.lastFrame.tv_sec;
     diff += ( filltime.tv_usec - cameraData.lastFrame.tv_usec )
          / 1000000.0;
     if( dropped ) {

          printf( "cam %ld ", cam->cameraNumber );
          printf( "dropped %d frames %5ld at %ld.%06ld\n",
                  dropped,
                  cam->frameCount, filltime.tv_sec, filltime.tv_usec );
          if( logFD ) {
               fprintf( logFD, "cam %ld ", cam->cameraNumber );
               fprintf( logFD,
                        "dropped %d frames %5ld at %ld.%06ld\n",
                        dropped, cam->frameCount,
                        filltime.tv_sec, filltime.tv_usec );
          }
     }

     if( cam->verbose & VERB_GETFRAME ) {
          KIDID;
          printf( "cam %ld ", cam->cameraNumber );
          printf( "getframe %5ld at %ld.%06ld",
                  cam->frameCount, filltime.tv_sec, filltime.tv_usec );
          printf( " ft: %3d %04d %04d", ft.cooked.secs,
                  ft.cooked.cycles, ft.cooked.offset );
          printf( " diff %.4f\n", diff );
          printf( "    Bayer: %d", cameraData.frame.bayerFormat );
          printf( " int: %.4f", cam->intTime );
          printf( " gain: %.2f\n", cam->realGain );
     }

     cameraData.testFrame.when = cameraData.lastFrame = filltime;
     cameraData.testFrame.dataPtr = cameraData.frame.pData;
     return &cameraData.testFrame;
}

void
_releaseFrame(  )
{
     fc2Error status;
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

     fc2Error status;
     fc2PropertyType type;
     fc2Property property;
     fc2PropertyInfo info;

     float ftemp;
     double dvalue;

     int i;
     unsigned long R, B;

     ftemp = value;

     action = dc1394Decode( caction );
     feature = dc1394Decode( cfeature );

     if(feature == DC1394_FEATURE_NULL) return -1;

     if( cameraModule.verbose & VERB_SET ) {
          printf
               ( "setCam: action %s (%d) feature: %s (%d) value: %f\n",
                 caction, action, cfeature, feature, value );
     }

     /* flag here, value later, if >0, worry about previous value */
     prev = 0;
     /* primary job here: convert from DC1394 feature value to FC2 value */
     /* that's so I can use ONE dc1394ParameterList file for all cameras. */
     type = FC2_UNSPECIFIED_PROPERTY_TYPE;

     switch ( feature ) {

               /* all of these behave the same */
          case DC1394_FEATURE_BRIGHTNESS:
               type = FC2_BRIGHTNESS;
               break;
          case DC1394_FEATURE_EXPOSURE:
               type = FC2_AUTO_EXPOSURE;
               break;
          case DC1394_FEATURE_SHARPNESS:
               type = FC2_SHARPNESS;
               break;
          case DC1394_FEATURE_HUE:
               type = FC2_HUE;
               break;
          case DC1394_FEATURE_SATURATION:
               type = FC2_SATURATION;
               break;
          case DC1394_FEATURE_GAMMA:
               type = FC2_GAMMA;
               break;
          case DC1394_FEATURE_IRIS:
               type = FC2_IRIS;
               break;
          case DC1394_FEATURE_FOCUS:
               type = FC2_FOCUS;
               break;
          case DC1394_FEATURE_TEMPERATURE:
               type = FC2_TEMPERATURE;
               break;
          case DC1394_FEATURE_TRIGGER:
               type = FC2_TRIGGER_MODE;
               break;
          case DC1394_FEATURE_TRIGGER_DELAY:
               type = FC2_TRIGGER_DELAY;
               break;
          case DC1394_FEATURE_WHITE_BALANCE:
               type = FC2_WHITE_BALANCE;
               break;
          case DC1394_FEATURE_FRAME_RATE:
               type = FC2_FRAME_RATE;
               break;
          case DC1394_FEATURE_ZOOM:
               type = FC2_ZOOM;
               break;
          case DC1394_FEATURE_PAN:
               type = FC2_PAN;
               break;
          case DC1394_FEATURE_TILT:
               type = FC2_TILT;
               break;
          case DC1394_FEATURE_SHUTTER:
               type = FC2_SHUTTER;
               break;
          case DC1394_FEATURE_GAIN:
               type = FC2_GAIN;
               break;
          case MY_FEATURE_WHITE_BALANCE_R:
          case MY_FEATURE_WHITE_BALANCE_B:
               type = FC2_WHITE_BALANCE;
               break;
          default:
               return ( -1 );
               break;
     }

     /* do the special cases first. */
     if( type == FC2_WHITE_BALANCE ) {

          /* get the current white balance info */

          ltemp = value;
          property.type = type;
          status = fc2GetProperty( cameraData.context, &property );
          testStatus( "get property set camera WB" );
          PRINTTHIS PrintProperty( &property );

          info.type = type;
          status = fc2GetPropertyInfo( cameraData.context, &info );
          testStatus( "get property info setcamera" );
          PRINTTHIS PrintPropertyInfo( &info );

          property.absControl = FALSE;
          property.onOff = TRUE;
          if( feature == MY_FEATURE_WHITE_BALANCE_R )
               property.valueA = ltemp;
          else
               property.valueB = ltemp;
          status = fc2SetProperty( cameraData.context, &property );
          testStatus( "set white balance property" );
          return 0;
     }

     if( type == FC2_TRIGGER_MODE ) {

          fc2TriggerMode triggerMode;
          if( value < 0 ) {     /* disable trigger */
               triggerMode.onOff = FALSE;
               triggerMode.source = 0;
               triggerMode.parameter = 0;
               triggerMode.polarity = 0;
               triggerMode.mode = 0;
          }
          else {
               triggerMode.onOff = TRUE;
               triggerMode.source = cameraData.tsource;
               triggerMode.parameter = 0;
               triggerMode.polarity = 0;
               triggerMode.mode = ( int ) value;
          }
          status = fc2SetTriggerMode( cameraData.context, &triggerMode );
          testStatus( "set trigger mode" );

          {

               fc2TriggerMode triggerMode;
               fc2TriggerModeInfo tmi;
               status = fc2GetTriggerModeInfo( cameraData.context, &tmi );
               testStatus( "get trig mode info in _setCamera" );
               GPRINT
                    printf
                    ( "trig mode info: %s %s %s %s mask: 0x%08u %s\n",
                      tmi.present ? "present" : "not present",
                      tmi.onOffSupported ? "onOff" : "no OnOff",
                      tmi.polaritySupported ? "polarity" :
                      "no polarity",
                      tmi.valueReadable ? "readable" : "not readable",
                      tmi.sourceMask,
                      tmi.softwareTriggerSupported ?
                      "software trigger" : "no software trigger" );
               status = fc2GetTriggerMode( cameraData.context, &triggerMode );
               testStatus( "get trig mode in _setCamera" );
               GPRINT
                    printf
                    ( "trig mode: %s polarity: %u source: %u mode: %u param: %u\n",
                      triggerMode.onOff ? "ON" : "OFF",
                      triggerMode.polarity, triggerMode.source,
                      triggerMode.mode, triggerMode.parameter );
          }


          return 0;
     }

     /* must have info about property, and property, so ... */
     info.type = type;
     status = fc2GetPropertyInfo( cameraData.context, &info );
     testStatus( "get property info setcamera" );
     PRINTTHIS PrintPropertyInfo( &info );

     property.type = type;
     status = fc2GetProperty( cameraData.context, &property );
     testStatus( "get property setcamera" );
     /* we will set absVal flag always */
     property.absControl = TRUE;

     /* pretty much all of the rest are the same for me */
     switch ( action ) {        /* CAM_SET, CAM_ADD, CAM_SCALE */

          case CAM_SCALE:
          case CAM_ADD:

               /* and now add or scale what we want to the real value */
               if( action == CAM_ADD ) {
                    /* kludge for shutter -- camera is ms! */
                    if( type == FC2_SHUTTER )
                         ftemp *= 1000;
                    property.absValue += ftemp;
               }
               else
                    property.absValue *= ftemp;
               /* fall through to setting the values back. */

          case CAM_SET:

               /* the ONLY specific thing I do to set */
               if( action == CAM_SET ) {
                    /* kludge for shutter */
                    if( type == FC2_SHUTTER )
                         ftemp *= 1000;
                    property.absValue = ftemp;
               }

               if( property.absValue > info.absMax )
                    property.absValue = info.absMax;
               if( property.absValue < info.absMin )
                    property.absValue = info.absMin;

               /* turn extra stuff off if we are setting manually */
               property.onePush = FALSE;
               property.autoManualMode = FALSE;
               property.onOff = TRUE;

               /* now set it. */
               PRINTTHIS printf( "trying to set a property %s\n",
                                 PropertyNames[type] );
               PRINTTHIS PrintProperty( &property );
               status = fc2SetProperty( cameraData.context, &property );
               testStatus( "setCam set property" );
               break;

          case CAM_OFF:        /* turn it off */
               property.onOff = FALSE;
               status = fc2SetProperty( cameraData.context, &property );
               testStatus( "setCam set property" );
               break;

          default:
               printf
                    ( "cannot identify action for type %s action %d %s\n",
                      PropertyNames[type], action, caction );
               break;
     }

     /* while we are here, get shutter and gain */
     property.type = FC2_SHUTTER;
     status = fc2GetProperty( cameraData.context, &property );
     PRINTTHIS PrintProperty( &property );

     cameraModule.intTime = property.absValue / 1000.0;
     cameraModule.shutter = property.valueA;
     property.type = FC2_GAIN;
     status = fc2GetProperty( cameraData.context, &property );
     PRINTTHIS PrintProperty( &property );

     cameraModule.realGain = property.absValue;
     cameraModule.gain = property.valueA;
}

/* get camera parameter. */
/* NOTE: it is YOUR responsibility to know that the pointer points */
/* to a long enough result */
int
_getCamera( int feature, double *value )
{
     unsigned int ltemp;
     fc2Error status;
     fc2Property property;
     fc2PropertyType type;
     float ftemp;
     int i;
     unsigned int R, B;
     /* primary job here: convert from DC1394 feature value to FC2 value */
     /* that's so I can use ONE dc1394ParameterList file for all cameras. */
     type = FC2_UNSPECIFIED_PROPERTY_TYPE;
     switch ( feature ) {

               /* all of these behave the same */
          case DC1394_FEATURE_BRIGHTNESS:
               type = FC2_BRIGHTNESS;
               break;
          case DC1394_FEATURE_EXPOSURE:
               type = FC2_AUTO_EXPOSURE;
               break;
          case DC1394_FEATURE_SHARPNESS:
               type = FC2_SHARPNESS;
               break;
          case DC1394_FEATURE_HUE:
               type = FC2_HUE;
               break;
          case DC1394_FEATURE_SATURATION:
               type = FC2_SATURATION;
               break;
          case DC1394_FEATURE_GAMMA:
               type = FC2_GAMMA;
               break;
          case DC1394_FEATURE_IRIS:
               type = FC2_IRIS;
               break;
          case DC1394_FEATURE_FOCUS:
               type = FC2_FOCUS;
               break;
          case DC1394_FEATURE_TEMPERATURE:
               type = FC2_TEMPERATURE;
               break;
          case DC1394_FEATURE_TRIGGER:
               type = FC2_TRIGGER_MODE;
               break;
          case DC1394_FEATURE_TRIGGER_DELAY:
               type = FC2_TRIGGER_DELAY;
               break;
          case DC1394_FEATURE_WHITE_BALANCE:
               type = FC2_WHITE_BALANCE;
               break;
          case DC1394_FEATURE_FRAME_RATE:
               type = FC2_FRAME_RATE;
               break;
          case DC1394_FEATURE_ZOOM:
               type = FC2_ZOOM;
               break;
          case DC1394_FEATURE_PAN:
               type = FC2_PAN;
               break;
          case DC1394_FEATURE_TILT:
               type = FC2_TILT;
               break;
          case DC1394_FEATURE_SHUTTER:
          case MY_FEATURE_SHUTTER_REAL:
               type = FC2_SHUTTER;
               break;
          case DC1394_FEATURE_GAIN:
          case MY_FEATURE_GAIN_REAL:
               type = FC2_GAIN;
               break;
          case MY_FEATURE_WHITE_BALANCE_R:
          case MY_FEATURE_WHITE_BALANCE_B:
               type = FC2_WHITE_BALANCE;
               break;
          default:
               return ( -1 );
               break;
     }

     /* do the special cases first. */
     if( type == FC2_WHITE_BALANCE ) {

          return 0;
     }

     if( type == FC2_TRIGGER_MODE ) {

          printf( "trigger mode set not supported yet\n" );
          return 0;
     }

     /* get it */
     property.type = type;
     status = fc2GetProperty( cameraData.context, &property );
     testStatus( "get property getcamera" );
     *value = property.absValue;
     return 1;
}


/* fc2 -- beginning of _init */

void
_init(  )
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
     long delay;

     fc2Error status;
     fc2CameraInfo camInfo[50];
     fc2GigEStreamChannel streamChannel;
     fc2IPAddress dest;
     fc2GigEImageSettings imageSettings;
     fc2GigEImageSettingsInfo imageSettingsInfo;
     fc2FrameRate frameRate;
     fc2VideoMode videoMode;
     fc2GigEConfig gigeConfig;
     fc2Config config;

     unsigned int numStreamChannels;
     unsigned int numCamInfo = sizeof( camInfo ) / sizeof( fc2CameraInfo );
     unsigned int numCams;

     long myValue;
     int portNode;

     unsigned long B, R;
     float fvalue;
     unsigned int bpp;
     float myFrameRate;
     uint32_t value[3];

     long hflip = 0;
     long allocBuffs = 10;

     strcpy( module, "gige" );
     strcpy( cameraModule.module, module );

     /* init cameraData struct to all 0 */
     memset( &cameraData, 0, cameraData.mySize );

     /* set flag saying I'm not connected yet */
     haveConnected = 0;

     /* find out how verbose I am supposed to be */
     getLongParam( &cameraModule.verbose, "cameraverbose", module );
     verbose = cameraModule.verbose;

     /* stop verbosity if output is not a terminal! It makes me crash! */
     if( !isatty( 1 ) ) {
          printf( "not a tty?\n" );
          verbose = cameraModule.verbose = 0;
     }

     /* collection interval for software trigger */
     cameraModule.interval = cameraData.interval = 500000;      /* microsecs */
     getLongParam( &cameraData.interval, "camerainterval", module );

     cameraData.tsource = 0;    /* gpio input default */
     getLongParam( &cameraData.tsource, "cameratriggersource", module );

     cameraModule.tsource = cameraData.tsource;

     /* get my camera number */
     getLongParam( &cameraModule.cameraNumber, "cameraNumber", module );
     if( hotmParams.cameraNumber > -1 )
          cameraModule.cameraNumber = hotmParams.cameraNumber;

     /* either hardwire offset (cameraOffset) or use delay (cameraDelay) */
     /* offset is absolute, delay is multiplied by camera number */

     delay = 0;
     getLongParam( &delay, "cameraDelay", module );

     /* was -1 */
     cameraData.offset = delay * ( cameraModule.cameraNumber - 1 );     // - (delay/2);
     getLongParam( &cameraData.offset, "cameraoffset", module );

     /* find out how many other cameras there will be */
     /* this COULD be used to generate delay, but I'll force user */
     /* to set it for now, above */
     getLongParam( &camsOnBus, "camerasOnBus", module );
     if( camsOnBus < 0 )
          camsOnBus = 1;

     /* how many dma buffers should I use? */
     getLongParam( &allocBuffs, "dmaBuffers", module );
     if( allocBuffs < 0 )
          allocBuffs = 10;

     /* look up the camera number to UID mapping from the file */
     /* UID will be the serial number. In text. */
     tmp = mapCameraToUID( cameraModule.cameraNumber );
     if( NULL == tmp ) {
          printf
               ( "%s: cannot map number %ld to UID. Giving up.\n",
                 module, cameraModule.cameraNumber );
          exit( -1 );
     }
     strcpy( cameraData.UID, tmp );

     /* connect to the GigE system */
     status = fc2CreateGigEContext( &cameraData.context );
     testStatus( "create context" );

     status =
          fc2DiscoverGigECameras( cameraData.context, camInfo, &numCamInfo );
     testStatus( "discover cameras" );

     status = fc2GetNumOfCameras( cameraData.context, &numCams );
     testStatus( "get num of cams" );

     if( numCams == 0 ) {
          printf( "hotm2: cannot find any cameras! whazzap?\n" );
          exit( -1 );
     }

     /* find my serial number */
     ui = strtoul( cameraData.UID, NULL, 10 );
     status =
          fc2GetCameraFromSerialNumber( cameraData.context,
                                        ui, &cameraData.guid );
     testStatus( "cannot get camera from serial number" );

     /* connect to it and get data */
     status = fc2Connect( cameraData.context, &cameraData.guid );
     testStatus( "connect to camera" );

     /* now I am */
     haveConnected = 1;

     status = fc2GetCameraInfo( cameraData.context, camInfo );
     testStatus( "get camera info" );
     /* remove after testing done */
     PRINTTHIS PrintCameraInfo( &camInfo );

     /* testing -- what is my power status? */
     {
          unsigned int addr = 0x610;
          unsigned int pOn = 0x80000000;
          unsigned int val;
          int count;

          status = fc2ReadRegister( cameraData.context, addr, &val );
          testStatus( "read power register" );
          GPRINT printf( "cam %ld power val 0x%08x\n",
                         cameraModule.cameraNumber, val );

          status = fc2WriteRegister( cameraData.context, addr, 0 );
          testStatus( "write power register" );

          sleep( 1 );

          status = fc2ReadRegister( cameraData.context, addr, &val );
          testStatus( "read power register again" );
          GPRINT printf( "cam %ld power val 0x%08x\n",
                         cameraModule.cameraNumber, val );

          status = fc2WriteRegister( cameraData.context, addr, pOn );
          testStatus( "write power register" );

          sleep( 1 );

          count = 0;
          while( pOn != val ) {
               status = fc2ReadRegister( cameraData.context, addr, &val );
               testStatus( "read power register again" );
               GPRINT printf( "cam %ld power val 0x%08x\n",
                              cameraModule.cameraNumber, val );
               if( count > 20 ) {
                    printf
                         ( "cam %ld failed to turn on after 20 tries\n",
                           cameraModule.cameraNumber );
                    exit( -1 );
               }
               count++;
          }

     }


     /* stream info ? */
     status =
          fc2GetNumStreamChannels( cameraData.context, &numStreamChannels );
     testStatus( "get num stream channels" );

     {

          int ii;
          long ipd;
          long mtu;
          long ipdm;

          /* set the inter packet delay from parameters */
          /* interpacketdelay is the base delay */
          /* add 3000 times the cameraPacketDelayMult time camera number-1 */
          mtu = 9000;
          ipd = 6250;
          ipdm = 0;

          getLongParam( &ipd, "interpacketdelay", module );
          getLongParam( &ipdm, "cameraPacketDelayMult", module );
          getLongParam( &mtu, "cameraMTU", module );

          ipd += 3000 * ( cameraModule.cameraNumber - 1 ) * ipdm;

          for( ii = 0; ii < numStreamChannels; ii++ ) {

               status =
                    fc2GetGigEStreamChannelInfo( cameraData.context,
                                                 ii, &streamChannel );
               testStatus( "get stream channel info" );

               streamChannel.doNotFragment = TRUE;
               streamChannel.packetSize = mtu;
               streamChannel.interPacketDelay = ipd;
               status =
                    fc2SetGigEStreamChannelInfo( cameraData.context,
                                                 ii, &streamChannel );
               testStatus( "get stream channel info" );

               status =
                    fc2GetGigEStreamChannelInfo( cameraData.context,
                                                 ii, &streamChannel );
               testStatus( "get stream channel info" );
               PRINTTHIS PrintStreamChannelInfo( &streamChannel );
          }
     }

     /* turn on some config things */
     status = fc2GetGigEConfig( cameraData.context, &gigeConfig );
     testStatus( "get GigE config" );

     GPRINT
          printf( "gige config: %s tfps: %u mptr: %u \n",
                  gigeConfig.enablePacketResend ==
                  TRUE ? "TRUE" : "FALSE",
                  gigeConfig.RESENDTIMEOUT, gigeConfig.MAXRESEND );

     gigeConfig.enablePacketResend = TRUE;
     gigeConfig.RESENDTIMEOUT = 20000;
     //gigeConfig.MAXRESEND = 100;
     status = fc2SetGigEConfig( cameraData.context, &gigeConfig );
     testStatus( "set GigE config" );

     status = fc2GetGigEConfig( cameraData.context, &gigeConfig );
     testStatus( "get GigE config 2" );
     GPRINT
          printf( "gige config: %s tfps: %u mptr: %u \n",
                  gigeConfig.enablePacketResend ==
                  TRUE ? "TRUE" : "FALSE",
                  gigeConfig.RESENDTIMEOUT, gigeConfig.MAXRESEND );

     status = fc2GetConfiguration( cameraData.context, &config );
     testStatus( "get config" );

     GPRINT PrintConfigInfo( config );

     {
          fc2GigEProperty prop;

          prop.propType = FC2_HEARTBEAT;
          status = fc2GetGigEProperty( cameraData.context, &prop );
          testStatus( "get gige prop heart" );
          //printf("FC2_HEARTBEAT: min: %u max: %u val: %u\n",
          //      prop.min, prop.max, prop.value );
          prop.value = 0;
          status = fc2SetGigEProperty( cameraData.context, &prop );
          testStatus( "set gige prop heart" );
          prop.propType = FC2_HEARTBEAT_TIMEOUT;
          status = fc2GetGigEProperty( cameraData.context, &prop );
          testStatus( "get gige prop heart timeout" );
          //printf("FC2_HEARTBEAT: min: %u max: %u val: %u\n",
          //      prop.min, prop.max, prop.value );
     }

     config.numBuffers = allocBuffs;
     config.numImageNotifications = 1;
     config.minNumImageNotifications = 1;
     config.grabTimeout = 5000;
     config.grabMode = FC2_BUFFER_FRAMES;
     status = fc2SetConfiguration( cameraData.context, &config );
     testStatus( "set config" );

     status = fc2GetConfiguration( cameraData.context, &config );
     testStatus( "get config 2" );
     GPRINT PrintConfigInfo( config );

     /* dummy up camera Model for other stuff */
     strcpy( cameraData.modelName, camInfo[0].modelName );

     /* rawOrder of bayer from fc2 */
     switch ( camInfo[0].bayerTileFormat ) {

          case 0:
               strcpy( cameraModule.rawOrder, "YYYY" );
               break;
          case 1:
               strcpy( cameraModule.rawOrder, "RGGB" );
               break;
          case 2:
               strcpy( cameraModule.rawOrder, "GRBG" );
               break;
          case 3:
               strcpy( cameraModule.rawOrder, "GBRG" );
               break;
          case 4:
               strcpy( cameraModule.rawOrder, "BGGR" );
               break;
          default:
               /* recent pt greys sometimes report -1 */
               /* this can be anything! */
               /* try to get from user later, assume YYYY for now */
               strcpy( cameraModule.rawOrder, "YYYY" );
     }

     getStringParam( cameraModule.rawOrder, "cameraraworder", "gige.c");

     cameraModule.format = formatMONO8;
     /* report raw camera data, if verbosity */
     if( verbose & VERB_CAMERA ) {
          KIDID;
          PrintCameraInfo( &camInfo );
          printf( "\n" );
     }

     /* figure out what type of camera we have */
     mapCameraNameToData( 0, camInfo[0].modelName );
     if( typeData.X == 0 ) {
          printf
               ( "%s: unknown camera model %s, aborting...\n",
                 module, camInfo[0].modelName );
          for( i = 0; camInfo[0].modelName[i]; i++ )
               printf( "%02x ", camInfo[0].modelName[i] );
          exit( -2 );
     }
     /* keep track of camera name for later */
     strcpy( cameraModule.module, typeData.name );
     /* can I set frame rate here? */
     {
          fc2Property info;
          info.type = FC2_FRAME_RATE;
          info.absControl = TRUE;
          info.onOff = TRUE;
          info.absValue = 2.0;
          status = fc2SetProperty( cameraData.context, &info );
          testStatus( "set frame rate" );
     }

#ifdef xxx
     {                          /* set the strobe active for all cameras */

          fc2StrobeControl strobe;
          strobe.onOff = TRUE;
          strobe.source = 1;
          strobe.polarity = 0;
          strobe.delay = 0.0;
          strobe.duration = 1.0;
          status = fc2SetStrobe( cameraData.context, &strobe );
          testStatus( "set strobe in _init" );
     }
#endif


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

     cameraModule.startCamera = _startCamera;
     cameraModule.stopCamera = _stopCamera;
     cameraModule.getFrame = _getFrame;
     cameraModule.releaseFrame = _releaseFrame;
     cameraModule.setCamera = _setCamera;
     cameraModule.getCamera = _getCamera;
     cameraModule.initCamera = _init;

     /* now lets register our cleanup function */
     // cannot link dynamic! now in loadCameraModule
     //atexit( _stopCamera );
     stopHasBeenCalled = 0;

     imageSettings.offsetX = cameraModule.top;
     imageSettings.offsetY = cameraModule.left;
     imageSettings.height = cameraModule.y;
     imageSettings.width = cameraModule.x;
     imageSettings.pixelFormat = FC2_PIXEL_FORMAT_RAW8;

     /*printf( "imageSettings: top: %d left: %d x: %d y: %d\n",
        imageSettings.offsetX,
        imageSettings.offsetY,
        imageSettings.width,
        imageSettings.height ); */

     status = fc2SetGigEImageSettings( cameraData.context, &imageSettings );
     testStatus( "set gige image settings" );

     /* now figure out the basic settings and set them */
     {
          /* real framerate from text version */
          double myRate = 1.875;
          DEFAULT( cameraData.frameRate,
                   dc1394Decode( typeData.defaultFramerate ) );
          switch ( cameraData.frameRate ) {

               case DC1394_FRAMERATE_60:
                    myRate *= 2;
               case DC1394_FRAMERATE_30:
                    myRate *= 2;
               case DC1394_FRAMERATE_15:
                    myRate *= 2;
               case DC1394_FRAMERATE_7_5:
                    myRate *= 2;
               case DC1394_FRAMERATE_3_75:
                    myRate *= 2;
               case DC1394_FRAMERATE_1_875:
                    break;
          }
          cameraModule.rate = cameraData.frameRate = myRate;
          /* do we specify a real rate? */
          getLongParam( &cameraData.frameRate, "cameraframerate", module );
          myRate = cameraModule.rate = cameraData.frameRate;
          //_setCamera( "CAM_SET", "DC1394_FEATURE_FRAME_RATE", myRate );
     }

     _setCamera( "CAM_OFF", "DC1394_FEATURE_BRIGHTNESS", 0 );
     _setCamera( "CAM_OFF", "DC1394_FEATURE_EXPOSURE", 0 );
     _setCamera( "CAM_OFF", "DC1394_FEATURE_WHITE_BALANCE", 0 );
     _setCamera( "CAM_OFF", "DC1394_FEATURE_GAMMA", 0 );
     _setCamera( "CAM_SET", "DC1394_FEATURE_GAIN", 0 );
     _setCamera( "CAM_SET", "DC1394_FEATURE_SHUTTER", 0.01 );

     /* first, turn off trigger */
     _setCamera( "CAM_SET", "DC1394_FEATURE_TRIGGER", -1 );

     {
          fc2TriggerMode triggerMode;
          fc2TriggerModeInfo tmi;
          long value = -1;      /* default OFF */

          getLongParam( &value, "cameraTrigger", module );

          /* tsource set much earlier in init */
          status = fc2GetTriggerModeInfo( cameraData.context, &tmi );
          testStatus( "get trig mode info in _init" );
          GPRINT printf( "trig mode info: %s %s %s %s mask: 0x%08u %s\n",
                         tmi.present ? "present" : "not present",
                         tmi.onOffSupported ? "onOff" : "no OnOff",
                         tmi.polaritySupported ? "polarity" : "no polarity",
                         tmi.valueReadable ? "readable" : "not readable",
                         tmi.sourceMask,
                         tmi.softwareTriggerSupported ? "software trigger" :
                         "no software trigger" );

          status = fc2GetTriggerMode( cameraData.context, &triggerMode );
          testStatus( "get trig mode in _init" );
          GPRINT
               printf
               ( "trig mode: %s polarity: %u source: %u mode: %u param: %u\n",
                 triggerMode.onOff ? "ON" : "OFF", triggerMode.polarity,
                 triggerMode.source, triggerMode.mode,
                 triggerMode.parameter );

          if( value < 0 ) {     /* disable trigger */
               triggerMode.onOff = FALSE;
               triggerMode.source = 0;
               triggerMode.parameter = 0;
               triggerMode.polarity = 0;
               triggerMode.mode = 0;
               cameraData.tsource = -1; /* if off, this is it. */
          }
          else {
               triggerMode.onOff = TRUE;
               triggerMode.source = cameraData.tsource;
               triggerMode.parameter = 0;
               triggerMode.polarity = 0;
               triggerMode.mode = ( int ) value;
          }
          status = fc2SetTriggerMode( cameraData.context, &triggerMode );
          testStatus( "set trigger mode in _init" );

          status = fc2GetTriggerMode( cameraData.context, &triggerMode );
          testStatus( "get trig mode in _init after set" );
          GPRINT
               printf
               ( "trig mode: %s polarity: %u source: %u mode: %u param: %u\n",
                 triggerMode.onOff ? "ON" : "OFF",
                 triggerMode.polarity, triggerMode.source,
                 triggerMode.mode, triggerMode.parameter );
     }

     status = fc2CreateImage( &cameraData.frame );

     /* no idea until we actually get a frame! */
     cameraModule.bufferLength = cameraModule.x * cameraModule.y;

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

}

/* fc2 -- end of _init */

int
SetTimeStamping( fc2Context context, BOOL enableTimeStamp )
{
     fc2Error error;
     fc2EmbeddedImageInfo embeddedInfo;

     error = fc2GetEmbeddedImageInfo( context, &embeddedInfo );
     if( error != FC2_ERROR_OK ) {
          printf( "Error in fc2GetEmbeddedImageInfo: %d\n", error );
     }

     if( embeddedInfo.timestamp.available != 0 ) {
          embeddedInfo.timestamp.onOff = enableTimeStamp;

     }
     embeddedInfo.gain.onOff = enableTimeStamp;
     embeddedInfo.shutter.onOff = enableTimeStamp;
     embeddedInfo.brightness.onOff = enableTimeStamp;
     embeddedInfo.exposure.onOff = enableTimeStamp;
     embeddedInfo.whiteBalance.onOff = enableTimeStamp;
     embeddedInfo.frameCounter.onOff = enableTimeStamp;
     embeddedInfo.strobePattern.onOff = enableTimeStamp;
     embeddedInfo.GPIOPinState.onOff = enableTimeStamp;
     embeddedInfo.ROIPosition.onOff = enableTimeStamp;

     error = fc2SetEmbeddedImageInfo( context, &embeddedInfo );
     FC2_ERR_CHECK( error, "set embedded image info" );
}

int
PrintCameraInfo( fc2CameraInfo * pCamInfo )
{
     printf( "\n*** CAMERA INFORMATION ***\n"
             "Serial number - %u\n"
             "Camera model - %s\n"
             "Camera vendor - %s\n"
             "Sensor - %s\n"
             "Bayer - %d\n"
             "Resolution - %s\n"
             "Firmware version - %s\n"
             "Firmware build time - %s\n\n",
             pCamInfo->serialNumber,
             pCamInfo->modelName,
             pCamInfo->vendorName,
             pCamInfo->sensorInfo,
             pCamInfo->bayerTileFormat,
             pCamInfo->sensorResolution,
             pCamInfo->firmwareVersion, pCamInfo->firmwareBuildTime );

     if( pCamInfo->interfaceType == FC2_INTERFACE_GIGE ) {
          fprintf( stdout,
                   "GigE major version - %u\n"
                   "GigE minor version - %u\n"
                   "User-defined name - %s\n"
                   "XML URL1 - %s\n" "XML URL2 - %s\n"
                   "MAC address - %02X:%02X:%02X:%02X:%02X:%02X\n"
                   "IP address - %u.%u.%u.%u\n"
                   "Subnet mask - %u.%u.%u.%u\n"
                   "Default gateway - %u.%u.%u.%u\n\n",
                   pCamInfo->gigEMajorVersion,
                   pCamInfo->gigEMinorVersion,
                   pCamInfo->userDefinedName,
                   pCamInfo->xmlURL1, pCamInfo->xmlURL2,
                   pCamInfo->macAddress.octets[0],
                   pCamInfo->macAddress.octets[1],
                   pCamInfo->macAddress.octets[2],
                   pCamInfo->macAddress.octets[3],
                   pCamInfo->macAddress.octets[4],
                   pCamInfo->macAddress.octets[5],
                   pCamInfo->ipAddress.octets[0],
                   pCamInfo->ipAddress.octets[1],
                   pCamInfo->ipAddress.octets[2],
                   pCamInfo->ipAddress.octets[3],
                   pCamInfo->subnetMask.octets[0],
                   pCamInfo->subnetMask.octets[1],
                   pCamInfo->subnetMask.octets[2],
                   pCamInfo->subnetMask.octets[3],
                   pCamInfo->defaultGateway.octets[0],
                   pCamInfo->defaultGateway.octets[1],
                   pCamInfo->defaultGateway.octets[2],
                   pCamInfo->defaultGateway.octets[3] );
     }
}


int
PrintImageData( fc2Image im )
{

     printf( "rows: %u cols: %u stride: %u\n"
             "size: %u receivedSize: %u \n"
             "format: %08x  bayer: %d\n"
             "address: %08llx\n",
             im.rows, im.cols, im.stride, im.dataSize,
             im.receivedDataSize, im.format,
             im.bayerFormat, ( unsigned long long ) im.pData );
}


int
PrintStreamChannelInfo( fc2GigEStreamChannel * pStreamChannel )
{
     char ipAddress[32];
     sprintf( ipAddress, "%u.%u.%u.%u",
              pStreamChannel->destinationIpAddress.octets[0],
              pStreamChannel->destinationIpAddress.octets[1],
              pStreamChannel->destinationIpAddress.octets[2],
              pStreamChannel->destinationIpAddress.octets[3] );
     printf( "Network interface: %u\n"
             "Host post: %u\n"
             "Do not fragment bit: %s\n"
             "Packet size: %u\n"
             "Inter packet delay: %u\n"
             "Destination IP address: %s\n"
             "Source port (on camera): %u\n\n",
             pStreamChannel->networkInterfaceIndex,
             pStreamChannel->HOSTPORT,
             pStreamChannel->doNotFragment ==
             TRUE ? "Enabled" : "Disabled",
             pStreamChannel->packetSize,
             pStreamChannel->interPacketDelay,
             ipAddress, pStreamChannel->sourcePort );
}

int
PrintProperty( fc2Property * p )
{

     printf( "---------------------\n" );
     printf( "property type: %d %s\n", p->type, PropertyNames[p->type] );
     printf
          ( "  present: %s absControl: %s onePush: %s onOff: %s autoManual: %s\n",
            p->present == TRUE ? "TRUE" : "FALSE",
            p->absControl == TRUE ? "TRUE" : "FALSE",
            p->onePush == TRUE ? "TRUE" : "FALSE",
            p->onOff == TRUE ? "TRUE" : "FALSE",
            p->autoManualMode == TRUE ? "TRUE" : "FALSE" );
     printf( " values: (A) %u (B) %u (abs) %f\n",
             p->valueA, p->valueB, p->absValue );
     printf( "---------------------\n" );
}

int
PrintPropertyInfo( fc2PropertyInfo * p )
{

     printf( "---------------------\n" );
     printf( "property info type: %d %s\n", p->type, PropertyNames[p->type] );
     printf
          ( "  present: %s autoOK: %s manualOK: %s onOffOK: %s\n",
            p->present == TRUE ? "TRUE " : "FALSE",
            p->autoSupported == TRUE ? "TRUE " : "FALSE",
            p->manualSupported == TRUE ? "TRUE " : "FALSE",
            p->onOffSupported == TRUE ? "TRUE " : "FALSE" );
     printf
          ( "  onePushOK: %s absValOK: %s readOutOK: %s \n",
            p->onePushSupported ==
            TRUE ? "TRUE " : "FALSE",
            p->absValSupported == TRUE ? "TRUE " : "FALSE",
            p->readOutSupported == TRUE ? "TRUE " : "FALSE" );
     printf
          ( "  min: %u (%f)  max: %u (%f) units: %s (%s)\n",
            p->min, p->absMin, p->max, p->absMax, p->pUnits, p->pUnitAbbr );
     printf( "---------------------\n" );
}

int
PrintImageInfo( int when, fc2Image * ii )
{
     if( when == 1 ) {
          printf
               ( "image info before: %u x %u stride %u size: %u rsize: %u addr: %08llx\n",
                 ii->cols, ii->rows, ii->stride, ii->dataSize,
                 ii->receivedDataSize, ( unsigned long long ) ii->pData );
     }
     else {
          printf
               ( "image info  after: %u x %u stride %u size: %u rsize: %u addr: %08llx\n",
                 ii->cols, ii->rows, ii->stride, ii->dataSize,
                 ii->receivedDataSize, ( unsigned long long ) ii->pData );
     }
}

int
PrintConfigInfo( fc2Config c )
{

     printf( "---------------------\n" );
     printf( "Camera Config: \n" );
     printf( "  numBuffers: %u\n", c.numBuffers );
     printf( "  numImageNotifications: %u\n", c.numImageNotifications );
     printf( "  minNumImageNotifications: %u\n", c.minNumImageNotifications );
     printf( "  grabTimeout: %d\n", c.grabTimeout );
     printf( "  grabMode: %d\n", c.grabMode );
     printf( "---------------------\n" );
}
