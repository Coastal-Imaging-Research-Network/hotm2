/* 

	gigeSpin.c -- spinnaker version of gige.c

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

static const char rcsid[] = "$Id$";

#include "hotm.h"
#include "alarm.h"

#define CAMERAMODULE
#include "gigeSpin-camera.h"

#include <sys/time.h>
#include <sys/resource.h>

void getLimits(  );

static char module[128];
extern FILE *logFD;
int haveConnected = 0;
quickSpin myQs;
quickSpinTLDevice myQsTL;
int testStatusFatal = 0;
long ignoreChunkTime = 0;

int64_t lastTimestamp;
int64_t lastSequence;


void mapCameraNameToData(  );

#define SPIN_ERR_CHECK(x,y)	if(x != SPINNAKER_ERR_SUCCESS) { \
			printf( "something failed: %s\n", y ); \
			printf( "reason: %d\n", x);\
			if( logFD ) {\
			fprintf( logFD, "something failed: %s\n", y ); \
			fprintf( logFD, "reason: %d\n", x);\
			} \
			exit(-1);	\
			}

#define testStatus(x) \
if( status != SPINNAKER_ERR_SUCCESS ) {\
     printf( "cam %ld: %s line %d: %s\n",\
             cameraModule.cameraNumber,\
             cameraModule.module, __LINE__, ( x ) );\
     printf( "cam %ld: reason: %d\n", cameraModule.cameraNumber, status );\
     if( testStatusFatal ) exit(-1); \
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

/* turn on or off all Chunk data for Spin version. */

int
SetTimeStamping( spinNodeMapHandle hnm, bool8_t enableTimeStamp )
{
     spinError error;
     spinNodeHandle hChunkModeActive = NULL;
     char etxt[128];
     bool8_t isWritable;

     error = spinNodeMapGetNode( hnm, "ChunkModeActive", &hChunkModeActive );
     SPIN_ERR_CHECK( error, "no Chunk mode!" );

     error = spinBooleanSetValue( hChunkModeActive, True );
     SPIN_ERR_CHECK( error, "cannot enable chunk mode" );

     spinNodeHandle hChunkSelector = NULL;
     size_t numEntries = 0;

     error = spinNodeMapGetNode( hnm, "ChunkSelector", &hChunkSelector );
     SPIN_ERR_CHECK( error, "cannot get chunk selector" );

     error = spinEnumerationGetNumEntries( hChunkSelector, &numEntries );
     SPIN_ERR_CHECK( error, "cannot get num entries" );

     int ii;
     for( ii = 0; ii < numEntries; ii++ ) {

          spinNodeHandle hEntry = NULL;
          size_t lenEtxt = 128;
          error =
               spinEnumerationGetEntryByIndex( hChunkSelector, ii, &hEntry );
          sprintf( etxt, "cannot get chunk entry %d", ii );
          SPIN_ERR_CHECK( error, etxt );

          error = spinNodeGetDisplayName( hEntry, etxt, &lenEtxt );
          //printf( "chunk number %d name %s\n", ii, etxt );

          int64_t value = 0;

          error = spinEnumerationEntryGetIntValue( hEntry, &value );
          SPIN_ERR_CHECK( error, "get enum value for chunk" );

          //printf( "chunk name %s value %ld\n", etxt, value );

          spinNodeIsWritable( hChunkSelector, &isWritable );
          if( isWritable ) {
               error = spinEnumerationSetIntValue( hChunkSelector, value );
               SPIN_ERR_CHECK( error, "set value to selector" );

               spinNodeHandle hChunkEnable = NULL;

               error =
                    spinNodeMapGetNode( hnm, "ChunkEnable", &hChunkEnable );
               SPIN_ERR_CHECK( error, "getting handle to enable chunk" );

               spinNodeIsWritable( hChunkEnable, &isWritable );

               if( isWritable ) {

                    error =
                         spinBooleanSetValue( hChunkEnable, enableTimeStamp );
                    SPIN_ERR_CHECK( error, "changing on/off mode" );
               }
          }

     }

     return ( SPINNAKER_ERR_SUCCESS );

}

void onDeviceEvent(const spinDeviceEventData hEventData, const char* pEventName, void* pUserData)
{

    spinError status = SPINNAKER_ERR_SUCCESS;
    userData* eventInfo = (userData*)pUserData;
    uint64_t eventID = 0;
    int64_t eventTimestamp = 0;
    int64_t eventFrameID = 0;
    int64_t transferCurrentCount = 0;
    int64_t transferLost = 0;

    if (strcmp(pEventName, eventInfo->eventName) == 0) {
        status = spinDeviceEventGetId(hEventData, &eventID);
	testStatus("get eventID");

	status = spinIntegerGetValue( cameraData.qs.EventExposureEndTimestamp, &eventTimestamp ); 
	testStatus("get device timestamp");
	status = spinIntegerGetValue( cameraData.qs.EventExposureEndFrameID, &eventFrameID );
	testStatus("get device frame id");
	status = spinIntegerGetValue( cameraData.qs.TransferQueueCurrentBlockCount, &transferCurrentCount );
	testStatus("get device block count");
	status = spinIntegerGetValue( cameraData.qs.TransferQueueOverflowCount, &transferLost );
	testStatus("get device frame id");

#ifdef NONO
	printf("Device event %s with ID %u count %d timestamp %f frame %u\n", 
		pEventName, 
		(unsigned int)eventID, 
		eventInfo->count,
		eventTimestamp/1000/1000000.0,
		eventFrameID);
	printf("Device event %s with ID %u count %d block count %ld lost %ld\n", 
		pEventName, 
		(unsigned int)eventID, 
		eventInfo->count++,
		transferCurrentCount,
		transferLost );
#endif

	cameraData.framesReady+=1;  /* count how many we have ready. */

    }

}

/* SPIN -- start startCamera */

	userData eventInfo;

void
_startCamera( struct _cameraModule_ *cam )
{
     spinError status;
	spinDeviceEvent deviceEvent = NULL;

     if( cam->verbose & VERB_START ) {
          KIDID;
          printf( "cam %ld ", cam->cameraNumber );
          printf( "start camera called, module \"%s\" \n", cam->module );
     }

     /* enable device events for this camera */
     {

	int i;

	spinNodeHandle hEventSelector = NULL;
	status = spinNodeMapGetNode( cameraData.hNodeMap, 
		"EventSelector",
		&hEventSelector );
	testStatus("get event selector node map");

	size_t numEntries = 0;
	status = spinEnumerationGetNumEntries( hEventSelector,
					&numEntries );
	testStatus("get num event entries");
	
	for( i=0; i<numEntries; i++ ) {
		spinNodeHandle hEntry = NULL;
		char entryName[MAX_BUFF_LEN];
		size_t lenEntryName = MAX_BUFF_LEN;
		int64_t value = 0;

		status = spinEnumerationGetEntryByIndex( hEventSelector,
					i,
					&hEntry );
		testStatus("getentry by index");

		status = spinNodeGetDisplayName( hEntry,
					entryName,
					&lenEntryName );
		testStatus("get display name");
		
		status = spinEnumerationEntryGetIntValue( hEntry, &value);
		testStatus("get enum value");

		status = spinEnumerationSetIntValue( hEventSelector, value );
		testStatus("set int value event sel");

		// Enable entry on event notification node
		spinNodeHandle hEventNotification = NULL;
		spinNodeHandle hEventNotificationOn = NULL;
		int64_t eventNotificationOn = 0;

		status = spinNodeMapGetNode( cameraData.hNodeMap,
			"EventNotification", &hEventNotification );
		testStatus("get node notification");

		status = spinEnumerationGetEntryByName( hEventNotification,
					"On", 
					&hEventNotificationOn );
		testStatus("event note on");
		
		status = spinEnumerationEntryGetIntValue( hEventNotificationOn,
				&eventNotificationOn );
		testStatus("get in on value");

		status = spinEnumerationSetIntValue(hEventNotification, 
				eventNotificationOn);
		testStatus("set in on value");

	}

	eventInfo.count = 0;
	strcpy(eventInfo.eventName, "EventExposureEnd");

	status = spinDeviceEventCreate(&deviceEvent, &onDeviceEvent, (void*)&eventInfo);
	testStatus("create device event");

	if (chosenEvent == GENERIC) {
		status = spinCameraRegisterDeviceEvent(cameraData.hCam, 
				deviceEvent);
		testStatus("register device event");

	}
	else if (chosenEvent == SPECIFIC) {
		status = spinCameraRegisterDeviceEventEx(cameraData.hCam, 
			deviceEvent, 
			"EventExposureEnd");
		testStatus("register specific device event");
	}

     }

    SetTimeStamping( cameraData.hNodeMap, True );

     defaultAlarm( "startCamera start iso", cam->module );
     status = spinCameraBeginAcquisition( cameraData.hCam );
     testStatus( "failed to start, first try" );

     if( status != SPINNAKER_ERR_SUCCESS ) {
          status = spinCameraEndAcquisition( cameraData.hCam );
          testStatus( "cannot stop capture in failed start" );
          sleep( 1 );
          status = spinCameraBeginAcquisition( cameraData.hCam );
          SPIN_ERR_CHECK( status, "set transmission on second try!" );
     }


     clearAlarm(  );

     cam->frameCount = 0;
     cam->sumDiff = 0.0;
     cameraData.lastFrame.tv_sec = cameraData.lastFrame.tv_usec = 0;
}

/* SPIN -- end startCamera */


/* a generic cleanup routine now. called by error handlers */
/* I don't know what HAS been started, so I don't care if I */
/* fail at stopping them. */

int stopHasBeenCalled;

void
_stopCamera(  )
{
     spinError status;

     /* report my existence */
     //if( cameraModule.verbose & VERB_STOP ) {
          KIDID;
          printf( "cam %ld ", cameraModule.cameraNumber );
          printf( "stop camera called, module \"%s\" \n",
                  cameraModule.module );
     //}

     if( stopHasBeenCalled ) {
          /* i'm being recursive due to atexit. don't do me */
          //printf( "stop called for the second time: ignoring\n" );
          return;
     }
     stopHasBeenCalled++;

     /* stop capturing anything */
     status = spinCameraEndAcquisition( cameraData.hCam );
     SPIN_ERR_CHECK( status, "cannot stop capture" );

     /* I'm exiting next, I don't think I need to destroy the image */

     /* all done! might as well exit, we can do nothing more here */
     exit( -1 );

}

struct _frame_ *
_getFrame( struct _cameraModule_ *cam )
{
     int err;
     double diff;


     int64_t thisTimestamp;
     int64_t thisSequence;
     int64_t through;

     spinError status;
     spinImage myImage;
     int dropped;
     struct timeval filltime;
     struct timeval now;
     unsigned int secs;
     unsigned int micros;
     int lcount;


     dropped = 0;

/* sigh. alarms have stopped working. must use a signal-safe sleep
 * like in the macro MESLEEP(nanoseconds) and poll for images. Can't
 * poll for gigE spinnaker, must wait for an image event and go from 
 * there. So, while waiting for the event to give me an image, I must
 * loop and sleep.
 *
 * image events set up in the _init section. when image here, hFrame
 * is non-NULL. Must NULL it in releaseFrame 
 */

	lcount = 0;

	if( cameraData.ignoreEvents == 0 ) {
	
		while(1) {
			/* image is here! hurrah! */
			if( cameraData.framesReady ) break;

			/* no image yet ... */
			lcount++;

			/* timeout -- 100 loops */
			if(lcount > 100) break;

			/* sleep for a bit */
			MESLEEP( 50000000 ); /* 50ms, right? */

		}

		if( lcount>100 ) {
			/* timed out -- too many loops */
			_stopCamera();
			_exit(-1);
		}

	}


     if( cam->verbose & VERB_GETFRAME ) {
	gettimeofday(&now,NULL);
	printf( "in getFrame, asking for frame, %ld.%06ld, ready count %d loop count %d\n", now.tv_sec, now.tv_usec, cameraData.framesReady, lcount );
     }
     /* get the image! */
     status = spinCameraGetNextImage( cameraData.hCam, &cameraData.hFrame );
     testStatus( "failed get next image" );

     if( cam->verbose & VERB_GETFRAME ) {
	gettimeofday(&now,NULL);
	printf( "in getFrame, got one,     time %ld.%06ld\n", now.tv_sec, now.tv_usec );
     }

     //printf( "---------------------\n" );
//     clearAlarm(  );

     {
          bool8_t failed;
          bool8_t incomplete = False;
          status = spinImageIsIncomplete( cameraData.hFrame, &incomplete );
          if( incomplete ) {
               testStatus( "image is incomplete error" );
          }
     }

     double tIntTime;

     // get all the chunk data I can right now
     status =
          spinImageChunkDataGetFloatValue( cameraData.hFrame,
                                           "ChunkExposureTime", &tIntTime );
     testStatus( "getting exposure from chunk" );
     cameraModule.intTime = tIntTime / 1000000.0;

     status =
          spinImageChunkDataGetFloatValue( cameraData.hFrame,
                                           "ChunkGain",
                                           &cameraModule.realGain );
     testStatus( "getting gain from chunk" );

     status =
          spinImageChunkDataGetIntValue( cameraData.hFrame,
                                         "ChunkTimestamp", &thisTimestamp );
     testStatus( "getting timestamp" );

     status =
          spinImageChunkDataGetIntValue( cameraData.hFrame,
                                         "ChunkFrameID",
                                         &cameraData.frameID );
     testStatus( "getting frameID" );

     if( cameraData.referenceTimeval.tv_sec == 0 ) {
          gettimeofday( &cameraData.referenceTimeval, NULL );
          cameraData.referenceTimestamp = thisTimestamp;
     }

     {
          diff = ( thisTimestamp - lastTimestamp ) / 1000.0;    /* ns to micros */
          lastTimestamp = thisTimestamp;

          /* convert to real time from references */
          secs =
               ( thisTimestamp -
                 cameraData.referenceTimestamp ) / 1000000000 +
               cameraData.referenceTimeval.tv_sec;
          micros =
               ( ( thisTimestamp -
                   cameraData.referenceTimestamp ) % 1000000000 ) / 1000 +
               cameraData.referenceTimeval.tv_usec;
          while( micros > 1000000 ) {
               secs++;
               micros -= 1000000;
          }

          if( cam->verbose & VERB_GETFRAME )
               printf( "timestamp %d.%06d - %.3f micros\n",
                       secs, micros, diff );

          if( cameraData.use1588time == 1 ) {
               filltime.tv_sec = thisTimestamp / 1000000000;
               filltime.tv_usec = ( thisTimestamp % 1000000000 ) / 1000;
          }
          else {
		if( ignoreChunkTime ) 
			gettimeofday( &filltime, NULL );
		else {
		       filltime.tv_sec = secs;
		       filltime.tv_usec = micros;
		}
          }
     }

     /* temporary - get throughput info */
     status =
          spinIntegerGetValue( myQs.DeviceLinkCurrentThroughput, &through );
     testStatus( "get throughput" );

     if( cam->verbose & VERB_GETFRAME ) {
          gettimeofday( &now, NULL );
          KIDID;
          printf( "cam %ld ", cam->cameraNumber );
          printf( "getframe %5ld id: %6ld ",
                  cam->frameCount, cameraData.frameID );
          printf( " int: %.4fs", cam->intTime );
          printf( " gain: %.2fdB\n", cam->realGain );
          printf( "          at: %ld.%06ld",
                  filltime.tv_sec, filltime.tv_usec );
          printf( " diff %12.4f us\n", diff );
          diff = ( filltime.tv_sec + filltime.tv_usec / 1000000.0 )
               - ( now.tv_sec + now.tv_usec / 1000000.0 );
          diff *= 1000000;      /* microsecs to match previous line */
          printf( "    walltime: %ld.%06ld diff %12.4f us\n",
                  now.tv_sec, now.tv_usec, diff );
          printf( "    throughput: %ld\n", through );
     }

     if( cam->verbose & VERB_1588TIME ) {
          gettimeofday( &now, NULL );
          KIDID;
          printf( "cam %ld ", cam->cameraNumber );
          printf( " walltime: %ld.%06ld ", now.tv_sec, now.tv_usec );
          printf( " timestamp: %ld.%06ld ",
                  thisTimestamp / 1000000000,
                  ( thisTimestamp % 1000000000 ) / 1000 );
	  printf( "diff: %0.6f ", (double)(now.tv_sec-(thisTimestamp/1000000000)) 
			+ (now.tv_usec-((thisTimestamp%1000000000)/1000.0)) / 1000000);
	  printf( "1588: %s\n", cameraData.use1588time? "on" : "off" );
     }

     if( cameraData.lastframeID == 0 )
          cameraData.lastframeID = cameraData.frameID - 1;

     if( cameraData.framesReady > 1 ) {
          gettimeofday( &now, NULL );
          fprintf
               ( logFD,
                 "Multiple Frames Ready: %d frames at %ld.%06ld timestamp %u.%06u\n",
                 cameraData.framesReady, now.tv_sec,
                 now.tv_usec, secs, micros );
     }

     if( cameraData.frameID > (cameraData.lastframeID + 1) ) {
          /* dropped frame! */
          gettimeofday( &now, NULL );
          fprintf
               ( logFD,
                 "DROPPED FRAME: missed %ld frames (last: %ld now: %ld) at %ld.%06ld timestamp %u.%06u\n",
                 cameraData.frameID - cameraData.lastframeID - 1, 
		 cameraData.lastframeID, cameraData.frameID,
	 	 now.tv_sec,
                 now.tv_usec, secs, micros );
	  
          GPRINT {
               printf
                    ( "DROPPED FRAME: missed %ld frames (last: %ld now: %ld) at %ld.%06ld timestamp %u.%06u\n",
                      cameraData.frameID - cameraData.lastframeID - 1,
			cameraData.lastframeID, cameraData.frameID,
                      now.tv_sec, now.tv_usec, secs, micros );
          }

     }
     if( cameraData.frameID < (cameraData.lastframeID + 1) ) {
          /* recovered frame! */
          gettimeofday( &now, NULL );
          fprintf
               ( logFD,
                 "RECOVERED FRAME: recovered %ld frames (last: %ld now: %ld) at %ld.%06ld timestamp %u.%06u\n",
                  cameraData.lastframeID - cameraData.frameID - 1,
		 cameraData.lastframeID, cameraData.frameID,
	 	 now.tv_sec,
                 now.tv_usec, secs, micros );
	  
          GPRINT {
               printf
                    ( "RECOVERED FRAME: recovered %ld frames (last: %ld now: %ld) at %ld.%06ld timestamp %u.%06u\n",
                      cameraData.lastframeID - cameraData.frameID - 1,
			cameraData.lastframeID, cameraData.frameID,
                      now.tv_sec, now.tv_usec, secs, micros );
          }

     }

     cameraData.lastframeID = cameraData.frameID;

     cameraData.testFrame.when = cameraData.lastFrame = filltime;
     spinImageGetData( cameraData.hFrame,
                       ( void ** ) &cameraData.testFrame.dataPtr );

     { /* get resource usage, test for CPU problem */
	struct rusage use;
	struct timeval last;
	double myuse;
	
	if( getrusage( RUSAGE_SELF, &use ) )
		fprintf( logFD, "error getrusage? \n");
	else {
		myuse = use.ru_utime.tv_sec + use.ru_stime.tv_sec - last.tv_sec
		   + (use.ru_utime.tv_usec + use.ru_stime.tv_usec - last.tv_usec) / 1000000.0;
		last.tv_sec = use.ru_utime.tv_sec + use.ru_stime.tv_sec;
		last.tv_usec = use.ru_utime.tv_usec + use.ru_stime.tv_usec;
		if( myuse > 0.45 )  /* 90 % cpu! */
		fprintf( logFD, "my time used this pass: %.4f (%.0f%%)\n", myuse, myuse/.005 );
	}
     }

     return &cameraData.testFrame;
}

void
_releaseFrame(  )
{
     spinError status;
     //printf("release frame: frame %x\n", cameraData.hFrame );
     status = spinImageRelease( cameraData.hFrame );
     testStatus( "releasing frame" );
     cameraData.framesReady -= 1;

}


/* set camera parameter. even though params are INTs, we pass in a */
/* double so we can do ADJUST. double has more than enough precision */
/* for holding int values when we SET */
/* simplification for gige Spinnaker -- implement select list of setable 
   things. shutter, gain, frame-rate, white balance red and blue. until
   deemed necessary to be dynamic changes, require _init to deal with it all */

int
_setCamera( char *caction, char *cfeature, double value )
{
     int action;
     int feature;

     unsigned int ltemp;
     unsigned int prev;

     spinError status;
     spinBalanceRatioSelectorEnums myRB;

     double dvalue;

     int i;
     unsigned long R, B;

     int64_t through;

     //myQs = cameraData.qs;

     /* convert caction into number */
     for( action = 0; action < CAM_OFF + 1; action++ ) {
          if( 0 == strcasecmp( caction, CameraActions[action] ) ) {
               break;
          }
     }

     if( action > CAM_ON ) {
          printf( "invalid action: %s\n", caction );
          return -1;
     }

     /* convert feature into number */
     for( feature = 0;
          feature < ( sizeof( PropertyNames ) / sizeof( char * ) );
          feature++ ) {
          if( 0 == strcasecmp( cfeature, PropertyNames[feature] ) )
               break;
     }
     if( feature > ( sizeof( PropertyNames ) / sizeof( char * ) ) - 1 ) {
          printf( "invalid feature: %s\n", cfeature );
          return -1;
     }

     if( cameraModule.verbose & VERB_SET ) {
          printf
               ( "setCam: action %s (%d) feature: %s (%d) value: %f\n",
                 caction, action, cfeature, feature, value );
     }

     /* flag here, value later, if >0, worry about previous value */
     prev = 0;

/* the following three macros deal with converting PropertyNames and the
   position of a parameter in that list to the QuickSpin calls to do
   the action. Two kinds. QSFLOATSET(DO) deal with floating values.
   QSONOFFSET deals with turning features on and off. */

#define QSFLOATGETDO( XTHING, ANSWER ) \
               status = \
                   spinFloatSetValue( myQs.XTHING,  ANSWER ); \
                   testStatus( "cam_get " #XTHING )

#define QSFLOATSETDO( XTHING ) \
               status = \
                   spinFloatSetValue( myQs.XTHING, value ); \
                   testStatus( "cam_set " #XTHING )

#define QSFLOATSET( XTHING ) \
 \
               switch ( action ) { \
 \
                    case CAM_SCALE: \
                         status = spinFloatGetValue( myQs.XTHING, \
                                                     &dvalue ); \
                         testStatus( "cam_scale get " #XTHING ); \
                         value = dvalue * value; \
                         break;  \
 \
                    case CAM_ADD: \
                         status = spinFloatGetValue( myQs.XTHING, \
                                                     &dvalue ); \
                         testStatus( "cam_add get " #XTHING ); \
                         value = dvalue + value; \
			 break; \
 \
		} \
 \
		 if( value > cameraData.limits.max.XTHING ) \
		      value = cameraData.limits.max.XTHING; \
		 if( value < cameraData.limits.min.XTHING ) \
		      value = cameraData.limits.min.XTHING; \
		 QSFLOATSETDO(XTHING); \


#define QSONOFFSET( XTHING ) \
		switch( action ) { \
 \
		    case CAM_OFF: \
			 status = \
			      spinEnumerationSetEnumValue( myQs.XTHING, \
					 XTHING ## _Off ); \
			 testStatus( "cam_off " #XTHING ); \
			 break; \
\
		    case CAM_AUTO: \
			 status = \
			      spinEnumerationSetEnumValue( myQs.XTHING, \
					 XTHING ## _Continuous ); \
			 testStatus( "cam_auto on " #XTHING ); \
			 break; \
\
		    case CAM_ONCE: \
			 status = \
			      spinEnumerationSetEnumValue( myQs.XTHING, \
					 XTHING ## _Once ); \
			 testStatus( "cam_auto once " #XTHING ); \
			 break; \
                } \



     /* big select for all features enabled */
     switch ( feature ) {

          case FEAT_AUTO_GAIN:
               QSONOFFSET( GainAuto );
               break;

          case FEAT_AUTO_EXPOSURE:
               QSONOFFSET( ExposureAuto );
               break;

          case FEAT_SHUTTER:
               if( action == CAM_SCALE ) {
                    /* do nothing with value */
               }
               else {
                    /* convert from seconds to micros */
                    value = value * 1000000.0;
               }

               QSFLOATSET( ExposureTime );
               break;

          case FEAT_GAIN:
               QSFLOATSET( Gain );
               break;

          case FEAT_WHITE_BALANCE:     /* white balance */
               QSONOFFSET( BalanceWhiteAuto );
               break;

          case FEAT_WHITE_BALANCE_R:   /* white balance R */
          case FEAT_WHITE_BALANCE_B:   /* white balance B */
               myRB =
                    feature == FEAT_WHITE_BALANCE_R ?
                    BalanceRatioSelector_Red : BalanceRatioSelector_Blue;
               /* special -- I'm always CAM_SET, I can coerce action */
               action = CAM_OFF;
               QSONOFFSET( BalanceWhiteAuto );
               status =
                    spinEnumerationSetEnumValue( myQs.BalanceRatioSelector,
                                                 myRB );
               testStatus( "white balance selector set" );
               /* value is already set */
               QSFLOATSET( BalanceRatio );
               break;

          case FEAT_TRIGGER_MODE:      /* trigger mode */

               if( action == CAM_OFF ) {        /* turn off trigger mode */
                    status = spinEnumerationSetEnumValue( myQs.TriggerMode,
                                                          TriggerMode_Off );
                    testStatus( "trigger mode off" );
                    break;
               }

               /* anything else -- turn on */
               /* trigger type FrameStart, Source line0, Overlap Readout */
               /* and then turn off frame rate */
               status =
                    spinBooleanSetValue( myQs.AcquisitionFrameRateEnable,
                                         False );
               testStatus( "enable frame rate" );

               status = spinEnumerationSetEnumValue( myQs.TriggerSource,
                                                     TriggerSource_Line0 );
               testStatus( "trigger select line 0" );
               status = spinEnumerationSetEnumValue( myQs.TriggerSelector,
                                                     TriggerSelector_FrameStart );
               testStatus( "trigger frame start selector" );
               status = spinEnumerationSetEnumValue( myQs.TriggerOverlap,
                                                     TriggerOverlap_ReadOut );
               testStatus( "trigger overlap" );
               status = spinEnumerationSetEnumValue( myQs.TriggerActivation,
                                                     TriggerActivation_RisingEdge );
               testStatus( "trigger overlap" );

               status = spinEnumerationSetEnumValue( myQs.TriggerMode,
                                                     TriggerMode_On );
               testStatus( "trigger mode on" );



               break;

          case FEAT_FRAME_RATE:        /* frame rate */

               status = spinBooleanSetValue( myQs.AcquisitionFrameRateEnable, True );   /* ??? */
               testStatus( "enable frame rate" );

               through = value * cameraModule.bufferLength * 1.05;      /* safety factor */
               //if( through ) {
               //     status =
               //          spinIntegerSetValue( myQs.DeviceLinkThroughputLimit,
               //                               through );
               //     testStatus( "set throughput limit set camera" );
               //    printf("attempted throughput: %ld\n", through );
               //}

               QSFLOATSETDO( AcquisitionFrameRate );
               break;

          case FEAT_RESYNC_TIME:       /* special. zero sync time, force resync */
               cameraData.referenceTimeval.tv_sec = 0;
               break;

          case FEAT_SET_INTLIMIT:      /* special. set max for shutter */
               cameraData.limits.max.ExposureTime = 1000000.0 * value + 10;
               break;


          default:
               ;


     }

     status = spinFloatGetValue( myQs.ExposureTime, &cameraModule.intTime );
     testStatus( "set camera get int time" );
     //printf( "int time: %f\n", cameraModule.intTime );
     cameraModule.intTime /= 1000000.0; /* convert micros back to secs */

     status = spinFloatGetValue( myQs.Gain, &cameraModule.realGain );
     testStatus( "set camera get realGain" );
     //printf( "gain: %f\n", cameraModule.realGain );

}

/* get camera parameter. */
/* NOTE: it is YOUR responsibility to know that the pointer points */
/* to a long enough result */

int
_getCamera( int feature, double *value )
{
     unsigned int ltemp;
     spinError status;
     double tvalue;
     int i;
     unsigned int R, B;
     spinNodeHandle SNH = NULL;


     if( feature > sizeof( PropertyNames ) / sizeof( char * ) ) {
          printf( "invalid feature: %d\n", feature );
          return -1;
     }

     /* cant do quick spin here, need to find name from PropertyNames damn */

     status =
          spinNodeMapGetNode( cameraData.hNodeMap, spinNames[feature], &SNH );
     testStatus( "get cam get node" );
     status = spinFloatGetValue( SNH, value );
     testStatus( "get cam get value" );

     /* special case for shutter -- microsecs to seconds convert */
     if( feature == FEAT_SHUTTER )
          *value /= 1000000.0;

}


/* spin -- beginning of _init */

void
x_init(  )
{
     struct _commandFileEntry_ *ptr;
     long size;
     size_t numCameras;
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
     size_t  len;
     int64_t through;
     int64_t mtu;
     int64_t packetDelay;

     long myValue;
     int portNode;

     double B, R;
     float fvalue;
     unsigned int bpp;
     float myFrameRate;
     uint32_t value[3];

     spinError status;
     spinError err;

     long hflip = 0;
     long allocBuffs = 10;

     strcpy( module, "gigeSpin" );
     strcpy( cameraModule.module, module );

     /* sanity check linking -- cameraData.mySize must be non-zero! */
     if( cameraData.mySize == 0 ) {
          printf
               ( "Hey! You! I'm not connected to the real cameraData struct!\n" );
          printf( "I cannot continue.\n" );
          exit( -1 );
     }

     cameraData.use1588time = 0;        /* not using camera time yet */

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

     /* get my camera number */
     getLongParam( &cameraModule.cameraNumber, "cameraNumber", module );
     if( hotmParams.cameraNumber > -1 )
          cameraModule.cameraNumber = hotmParams.cameraNumber;

     /* find out how many other cameras there will be */
     /* this COULD be used to generate delay, but I'll force user */
     /* to set it for now, above */
     getLongParam( &camsOnBus, "camerasOnBus", module );
     if( camsOnBus < 0 )
          camsOnBus = 1;

     cameraData.ignoreEvents = 0; /* default to use event timing */
     getLongParam( &cameraData.ignoreEvents, "cameraignoreevents", module );

     ignoreChunkTime = 0; /* default to use chunk times */
     getLongParam( &ignoreChunkTime, "cameraignorechunktime", module );

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
     cameraData.hSystem = NULL;
     status = spinSystemGetInstance( &cameraData.hSystem );
     testStatus( "init get system instance" );

     cameraData.hCameras = NULL;
     status = spinCameraListCreateEmpty( &cameraData.hCameras );
     testStatus( "init create empty camera list" );

     //printf( "hCameras: %08x hSystem: %08x\n", ( unsigned long ) cameraData.hCameras,
     //        ( unsigned long ) cameraData.hSystem );

     status = spinSystemGetCameras( cameraData.hSystem, cameraData.hCameras );
     testStatus( "init get all cameras list" );

     status = spinCameraListGetSize( cameraData.hCameras, &numCameras );
     testStatus( "init get num of cams" );

     //printf( "found %lld cameras\n", numCameras );
     if( numCameras == 0 ) {
          printf( "hotm2: cannot find any cameras! whazzap?\n" );
          exit( -1 );
     }

     cameraData.hCam = NULL;
     status = spinCameraListGetBySerial( cameraData.hCameras, cameraData.UID,
                                         &cameraData.hCam );
     testStatus( "init get camera by serial" );

     if( cameraData.hCam == NULL ) {
          printf( "INIT FAILED: cannot find serial number %s\n",
                  cameraData.UID );
          exit( -1 );
     }

     /* ok! Found camera, info is in qs, qsD, and hCam */

     status = spinCameraInit( cameraData.hCam );
     if( status == -1005 ) {
          /* try again */
          sleep( 5 );
          status = spinCameraInit( cameraData.hCam );
          testStatus( "init init camera" );
     }

     /* get the genIcam node map */
     testStatusFatal=1;
     status = spinCameraGetNodeMap( cameraData.hCam, &cameraData.hNodeMap );
     testStatus( "get hnode map" );

     /* switch to quick spin mode please */
     status = quickSpinTLDeviceInit( cameraData.hCam, &cameraData.qsD );
     testStatus( "init QS TL Device init" );

     myQsTL = cameraData.qsD;

     status = quickSpinInit( cameraData.hCam, &cameraData.qs );
     testStatus( "init QS init seeking camera" );

     myQs = cameraData.qs;

     cameraData.use1588time = 1;
     getLongParam( &cameraData.use1588time, "camerause1588", "gigeSpin" );

     if( cameraData.use1588time ) {

          /* IEEE1588 */
          status = spinBooleanSetValue( myQs.GevIEEE1588, True );       /* ??? */
          testStatus( "set IEEE1588" );

          /* can we use 1588 time from camera? Did we just turn it on */
          if( status != SPINNAKER_ERR_SUCCESS ) /* we did not set 1588 flag ok */
               cameraData.use1588time = 0;

          else {

               /* set slave only! */
               status =
                    spinEnumerationSetEnumValue( myQs.GevIEEE1588Mode,
                                                 GevIEEE1588Mode_SlaveOnly );
               testStatus( "set IEEE1588 mode" );

          }

     }
     else {

          /* IEEE1588 - turn off */
          status = spinBooleanSetValue( myQs.GevIEEE1588, False );       /* ??? */
          testStatus( "unset IEEE1588" );
		
     }

     /* set the mtu */
     mtu = 9000;                /* default */
     getLongParam( (long *)&mtu, "cameramtu", "gigeSpin" );

     int64_t testMTU = 0;
     status = spinIntegerGetValue( myQs.GevSCPSPacketSize, &testMTU );
     testStatus( "get packet size" );

     // printf( "got mtu is %ld\n", testMTU );

     status = spinIntegerSetValue( myQs.GevSCPSPacketSize, mtu );
     testStatus( "set packet size" );

     /* packetDelay */
     packetDelay = 6250;
     getLongParam( (long *)&packetDelay, "interpacketdelay", "gigeSpin" );

     status = spinIntegerSetValue( myQs.GevSCPD, packetDelay );
     testStatus( "set packet delay" );

     testMTU = 0;
     status = spinIntegerGetValue( myQs.GevSCPD, &testMTU );
     testStatus( "get packet delay" );

     // printf( "got packet delay is %ld\n", testMTU );

     /* set the acquisition mode explicitely */
     status = spinEnumerationSetEnumValue( myQs.AcquisitionMode,
			AcquisitionMode_Continuous );
     testStatus( "set single frame acq mode" );

     /* must get limits for camera! */
     getLimits(  );
     cameraData.limits.max.TriggerMode = 16.0;
     cameraData.limits.min.TriggerMode = 0.0;

     /* turn off auto for things I want to control */
     _setCamera( "CAM_OFF", "FEATURE_AUTO_GAIN", 0 );
     _setCamera( "CAM_OFF", "FEATURE_AUTO_EXPOSURE", 0 );

     /* now I am */
     haveConnected = 1;

     /* clear the frame ready counter. incremented by onDeviceEvent.
	decremented by releaseFrame. */
     cameraData.framesReady = 0;

     /* dummy up camera Model for other stuff */
     len = 256;
     status =
          spinStringGetValue( cameraData.qs.DeviceModelName,
                              cameraData.modelName, &len );
     testStatus( "init get model name" );

     // printf( "model name: %s\n", cameraData.modelName );

     /* rawOrder of bayer */
     status = spinBooleanSetValue( myQs.ReverseY, False );
     strcpy( cameraModule.rawOrder, "RGGB" );
     status =
          spinEnumerationSetEnumValue( myQs.PixelFormat,
                                       PixelFormat_BayerRG8 );
     testStatus( "setting bayer RG8" );

     getLongParam( &hflip, "camerahflip", "" );
     if( hflip ) {
          status = spinBooleanSetValue( myQs.ReverseY, True );
          testStatus( "hflip" );

          strcpy( cameraModule.rawOrder, "GBRG" );
     }

     /* only time I need to pre-load? */
     cameraData.hFrame = NULL;

     cameraModule.format = formatMONO8;
     /* report raw camera data, if verbosity */
     if( verbose & VERB_CAMERA ) {
          KIDID;
          //PrintCameraInfo( &camInfo );
          printf( "\n" );
     }

     /* figure out what type of camera we have */
     mapCameraNameToData( 0, cameraData.modelName );
     if( typeData.X == 0 ) {
          printf
               ( "%s: unknown camera model %s, aborting...\n",
                 module, cameraData.modelName );
          for( i = 0; cameraData.modelName[i]; i++ )
               printf( "%02x ", cameraData.modelName[i] );
          exit( -2 );
     }
     /* keep track of camera name for later */
     strcpy( cameraModule.module, typeData.name );
     /* can I set frame rate here? */
     _setCamera( "CAM_SET", "FEATURE_FRAME_RATE", 2.0 );

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
     cameraModule.initCamera = x_init;

     /* now lets register our cleanup function */
     // cannot dynamic link, now in loadCameraModule
     //atexit( _stopCamera );
     stopHasBeenCalled = 0;

     _setCamera( "CAM_SET", "FEATURE_SHUTTER", 0.01 );
     _setCamera( "CAM_SET", "FEATURE_GAIN", 0 );
     _setCamera( "CAM_OFF", "FEATURE_BRIGHTNESS", 0 );
//     _setCamera( "CAM_OFF", "FEATURE_WHITE_BALANCE", 0 );
     _setCamera( "CAM_OFF", "FEATURE_GAMMA", 0 );

     /* first, turn off trigger */
     _setCamera( "CAM_OFF", "FEATURE_TRIGGER_MODE", 0 );

     {
          B = -1;
          getDoubleParam( &B, "cameraTrigger", module );
          if( B > -1 )
               _setCamera( "CAM_SET", "FEATURE_TRIGGER_MODE", B );

     }

     /* no idea until we actually get a frame! */
     cameraModule.bufferLength = cameraModule.x * cameraModule.y;

     /* set bandwidth */
     //through = 2.01 * cameraModule.bufferLength;   /* safety factor */
     //status = spinIntegerSetValue( myQs.DeviceLinkThroughputLimit, through );
     //testStatus( "set throughput limit" );

     /* white balance */
     B = -1;
     getDoubleParam( &B, "camerawhitebalanceb", "" );
     if( B != -1 ) {
          _setCamera( "CAM_SET", "FEATURE_WHITE_BALANCE_B", B );
          getDoubleParam( &R, "camerawhitebalancer", "" );
          _setCamera( "CAM_SET", "FEATURE_WHITE_BALANCE_R", R );
     }


     /* up until now, testStatus failures should be fatal. Now they can
        be non-fatal. */
     testStatusFatal = 0;

}

/* fc2 -- end of _init */

void
cameraInit(  )
{
     //printf("I am here, cameraInit.\n");
     x_init(  );
}

/* get limits from camera */
void
getLimits(  )
{

     spinError status;

     spinNodeHandle snh;
     double myLimit;

     //printf("cameraData addr getLimits: %x\n", &cameraData );

#define GETLIMITS(x) \
	status = spinNodeMapGetNode( cameraData.hNodeMap, \
			#x,\
			&snh );\
	testStatus( "get limit node " #x );\
\
	status = spinFloatGetMax( snh, &myLimit );\
	testStatus( "get max " #x );\
\
	cameraData.limits.max.x = myLimit;\
\
	status = spinFloatGetMin( snh, &myLimit );\
	testStatus( "get min " #x );\
\
	cameraData.limits.min.x = myLimit;\
\
	GPRINT printf(#x " max: %f min: %f\n", \
		cameraData.limits.max.x,\
		cameraData.limits.min.x );

     /* Exposure Time */
     GETLIMITS( ExposureTime );
     GETLIMITS( Gain );
     /* no. GETLIMITS(TriggerMode); */
     GETLIMITS( BalanceRatio );

}



#ifdef FOOXX
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
#endif
