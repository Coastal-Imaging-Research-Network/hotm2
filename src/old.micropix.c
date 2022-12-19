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
     "$Id: old.micropix.c 172 2016-03-25 21:03:51Z stanley $";

#include "hotm.h"
#include "alarm.h"
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>
#include <libdc1394/dc1394_control.h>

#define CAMERAMODULE
#include "camera.h"

static char module[128];

void mapCameraNameToData ();

int
cooked1394_read (raw1394handle_t handle,
                 int node,
                 unsigned long long offset, int size, quadlet_t * value)
{
     int retval, i;

     for (i = 0; i < 5; i++) {
          retval = raw1394_read (handle, 0xffc0 | node, offset, size, value);

          if (retval >= 0)
               break;
          usleep (1000);
     }

     return retval;
}



float
shutterToReal ()
{
     return -1.0;
}
float
gainToReal ()
{
     return -1.0;
}

#define HAS_FEATURE(x)   (cameraData.features.feature[x-FEATURE_MIN].available)
#define HAS_REAL(x)	 (cameraData.features.feature[x-FEATURE_MIN].absolute_capable)
#define MINBINVAL(x)	 (cameraData.features.feature[x-FEATURE_MIN].min)
#define MAXBINVAL(x)	 (cameraData.features.feature[x-FEATURE_MIN].max)
#define MINREALVAL(x)	 (cameraData.features.feature[x-FEATURE_MIN].abs_min)
#define MAXREALVAL(x)	 (cameraData.features.feature[x-FEATURE_MIN].abs_max)

#define featureOFF(x) status = dc1394_feature_on_off( cameraData.handle, \
						cameraData.camera.node, \
						x, \
						0 )
#define featureON(x) status = dc1394_feature_on_off( cameraData.handle, \
						cameraData.camera.node, \
						x, \
						1 )
#define autoOFF(x) status = dc1394_auto_on_off( cameraData.handle, \
						cameraData.camera.node, \
						x, \
						0 )
#define autoON(x) status = dc1394_auto_on_off( cameraData.handle, \
						cameraData.camera.node, \
						x, \
						1 )
#define featureSET(x,y) status = dc1394_set_feature_value( cameraData.handle, \
						cameraData.camera.node, \
						x, \
						y )
#define featureGET(x,y) status = dc1394_get_feature_value( cameraData.handle, \
						cameraData.camera.node, \
						x, \
						y )
#define featureRealSET(x,y) status = dc1394_set_absolute_feature_value( cameraData.handle, \
						cameraData.camera.node, \
						x, \
						y )
#define featureRealGET(x,y) status = dc1394_query_absolute_feature_value( cameraData.handle, \
						cameraData.camera.node, \
						x, \
						y )
#define featureRealOnOff(x,y) status = dc1394_absolute_setting_on_off( \
						cameraData.handle, \
						cameraData.camera.node, \
						x, \
						y )
#define testStatus(x) if( DC1394_SUCCESS != status ) { \
		KIDID; \
		printf( "%s: cannot accomplish: %s at line %d\n", module, x, __LINE__ ); \
	}

#define tVr \
		GetCameraControlRegister( cameraData.handle, \
				cameraData.camera.node, \
				0x0628UL, \
				value);\
		if( value[0]>>31 ) { \
			printf("VMODE ERROR in line %d\n", __LINE__ ); \
		}


void
_startCamera (struct _cameraModule_ *cam)
{
     int err;
     quadlet_t value[2];

     if (cam->verbose & VERB_START) {
          KIDID;
          printf ("cam %d ", cam->cameraNumber);
          printf ("start camera called, module \"%s\" \n", cam->module);
     }

     defaultAlarm ("startCamera start iso", cam->module);
     err = dc1394_start_iso_transmission (cameraData.handle,
                                          cameraData.camera.node);
     clearAlarm ();
     tVr;

     if (DC1394_SUCCESS != err) {
          KIDID;
          printf ("startCamera: failed to start\n");
          exit (-1);
     }

     cam->frameCount = 0;
     cam->sumDiff = 0.0;
     cameraData.lastFrame.tv_sec = cameraData.lastFrame.tv_usec = 0;

}

void
_stopCamera (struct _cameraModule_ *cam)
{
     int err;

     if (cam->verbose & VERB_STOP) {
          KIDID;
          printf ("cam %d stop camera called\n", cameraModule.cameraNumber);
     }

     defaultAlarm ("stopCamera stop iso", cam->module);
     err = dc1394_stop_iso_transmission (cameraData.handle,
                                         cameraData.camera.node);
     clearAlarm ();

     if (DC1394_SUCCESS != err) {
          KIDID;
          printf ("stopCamera: failed to stop iso!!\n");
     }

     defaultAlarm ("stopCamera release camera", cam->module);

     err = dc1394_dma_release_camera (cameraData.handle, &cameraData.camera);
     clearAlarm ();


     if (DC1394_SUCCESS != err) {
          KIDID;
          printf ("stopCamera: failed to release camera!!\n");
     }
     dc1394_destroy_handle (cameraData.handle);

}

struct _frame_ *
_getFrame (struct _cameraModule_ *cam)
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
     struct timeval now;
     double dft, dfc1, dfc2;

     cameraData.testFrame.status = 0;

     defaultAlarm ("getFrame single capture", cam->module);
     err = dc1394_dma_single_capture (&cameraData.camera);
     clearAlarm ();

     if (DC1394_SUCCESS != err) {
          KIDID;
          printf ("getFrame: died with err on dma get\n");
          exit (-1);
     }

     /* frame time in 1394 bus time from image */
     ft.raw = htonl (*(quadlet_t *) cameraData.camera.capture_buffer);

     fc1.raw = fc2.raw = 0;

     /* find out mapping from now to 1394 bus time */
     err = cooked1394_read (cameraData.handle,
                            cameraData.myNode,
                            CSR_REGISTER_BASE + CSR_CYCLE_TIME, 4, &fc1.raw);
     if (err) {
          printf ("first raw read failed\n");
     }
     /* write over system filltime! */
     gettimeofday (&cameraData.camera.filltime, NULL);
     err = cooked1394_read (cameraData.handle,
                            cameraData.myNode,
                            CSR_REGISTER_BASE + CSR_CYCLE_TIME, 4, &fc2.raw);
     if (err) {
          printf ("second raw read failed\n");
     }
     fc1.raw = htonl (fc1.raw);
     fc2.raw = htonl (fc2.raw);

#define DECODE_CYCLES(x) ( x.cooked.secs \
			+ (double)(x.cooked.cycles) / 8000 \
			+ (double)(x.cooked.offset) / 8000 / 3072 )

     dfc1 = DECODE_CYCLES (fc1);
     dfc2 = DECODE_CYCLES (fc2);
     if (dfc2 < dfc1)           /* rollover of 128 seconds */
          dfc2 += 128;
     dfc1 = (dfc1 + dfc2) / 2;  /* average 1394 bus time at 'now' */
     dft = DECODE_CYCLES (ft);
     if (dfc1 < dft)            /* rollover between frame and time check */
          dfc1 += 128;
     diff = dfc1 - dft;

     /* subtract  bus/system diff time from frame time */
     cameraData.camera.filltime.tv_usec -= (diff * 1000000.0);
     /* subtract int time from frame time */
     cameraData.camera.filltime.tv_usec -= (cam->intTime / 2 * 1000000.0);

     /* normalize time */
     while (cameraData.camera.filltime.tv_usec < 0) {
          cameraData.camera.filltime.tv_usec += 1000000;
          cameraData.camera.filltime.tv_sec--;
     }

     diff = cameraData.camera.filltime.tv_sec - cameraData.lastFrame.tv_sec;
     diff +=
          (cameraData.camera.filltime.tv_usec - cameraData.lastFrame.tv_usec)
          / 1000000.0;

     /* skip when difference is too long -- bogus number! */
     if (diff < 1000.0) {
          cam->sumDiff += diff;
          cam->frameCount++;
     }

     if (cam->verbose & VERB_GETFRAME) {
          KIDID;
          printf ("cam %d ", cam->cameraNumber);
          printf ("getframe %5d at %d.%06d",
                  cam->frameCount,
                  cameraData.camera.filltime.tv_sec,
                  cameraData.camera.filltime.tv_usec);
          printf (" ft: %3d %04d %04d", ft.cooked.secs, ft.cooked.cycles,
                  ft.cooked.offset);

          printf (" diff %.4f", diff);
          printf (" int: %.4f", cam->intTime);
          printf (" gain: %.2f\n", cam->realGain);
     }

     cameraData.lastFrame = cameraData.camera.filltime;
     cameraData.testFrame.dataPtr =
          (unsigned char *) cameraData.camera.capture_buffer;
     cameraData.testFrame.when = cameraData.camera.filltime;

     return &cameraData.testFrame;

}

void
_releaseFrame ()
{
     int status;

     defaultAlarm ("releaseFrame done with buffer", "micropix module");

     status = dc1394_dma_done_with_buffer (&cameraData.camera);
     clearAlarm ();

     if (DC1394_SUCCESS != status) {
          KIDID;
          printf ("releaseFrame: failed to release %d\n",
                  cameraData.camera.dma_last_buffer);
     }

}

/* set camera parameter. even though params are INTs, we pass in a */
/* double so we can do ADJUST. double has more than enough precision */
/* for holding int values when we SET */
int
_setCamera (char *caction, char *cfeature, double value)
{
     int action;
     int feature;
     unsigned int ltemp;
     unsigned int prev;
     long status;
     float ftemp;
     int i;
     unsigned int R, B;


     action = dc1394Decode (caction);
     feature = dc1394Decode (cfeature);

/*	printf("setcamera: action %d feature %d value %f\n", action,
 *	feature, value ); */

     /* flag here, value later, if >0, worry about previous value */
     prev = 0;

     switch (feature) {

               /* all of these behave the same */
          case FEATURE_BRIGHTNESS:
          case FEATURE_EXPOSURE:
          case FEATURE_SHARPNESS:
          case FEATURE_HUE:
          case FEATURE_SATURATION:
          case FEATURE_GAMMA:
          case FEATURE_IRIS:
          case FEATURE_FOCUS:
          case FEATURE_TEMPERATURE:
          case FEATURE_TRIGGER_DELAY:
          case FEATURE_WHITE_SHADING:
          case FEATURE_FRAME_RATE:
          case FEATURE_ZOOM:
          case FEATURE_PAN:
          case FEATURE_TILT:
          case FEATURE_OPTICAL_FILTER:
          case FEATURE_CAPTURE_SIZE:
          case FEATURE_CAPTURE_QUALITY:

               if (HAS_FEATURE (feature) == DC1394_FALSE)
                    return -1;

               /*printf("setcamera: feature ok\n" ); */

               ltemp = value;
               ftemp = value;
               switch (action) {
                    case CAM_SCALE:
                         if (HAS_REAL (feature)) {
                              featureRealOnOff (feature, 1);
                              featureRealGET (feature, &ftemp);
                              ftemp = ftemp * value;
                         }
                         else {
                              featureGET (feature, &ltemp);
                              ltemp = ltemp * value;
                         }
                         /* fall through! */
                    case CAM_SET:
                         featureON (feature);
                         if (HAS_REAL (feature))
                              featureRealSET (feature, ftemp);
                         else
                              featureSET (feature, ltemp);
                         testStatus ("setCamera SET SET");
                         return status;
                         break;
                    case CAM_ADD:
                         break;
                    default:
                         printf ("unknown action code %d\n", action);
                         return -1;
               }
               break;

          case FEATURE_SHUTTER:
          case FEATURE_GAIN:

               ftemp = value;
               status = -1;
               /* if I am scaling, do this ... */
               if (action == CAM_SCALE) {
                    if (HAS_REAL (feature)) {
                         featureRealGET (feature, &ftemp);
                         testStatus ("setCamera Get Real Scale");
                         ftemp = ftemp * value;
                    }
                    else {
                         featureGET (feature, &ltemp);
                         testStatus ("setCamera Get Scale");
                         ftemp = ltemp * value;
                    }
                    /* scaling shutter or gain UP, make sure it happens */
                    if (value > 1) {
                         prev++;
                         featureGET (feature, &ltemp);  /* make sure i have this */
                    }
               }
               /* if I am adding, do this ... */
               if (action == CAM_ADD) {
                    if (HAS_REAL (feature)) {
                         featureRealGET (feature, &ftemp);
                         testStatus ("setCamera Real Get Add");
                         ftemp = ftemp + value;
                    }
                    else {
                         featureGET (feature, &ltemp);
                         testStatus ("setCamera Get Add");
                         ftemp = ltemp + value;
                    }
               }
               /* in ALL cases, I now have the value to set */
               if (HAS_REAL (feature)) {
                    featureRealOnOff (feature, 1);
                    if (ftemp > MAXREALVAL (feature))
                         ftemp = MAXREALVAL (feature);
                    if (ftemp < MINREALVAL (feature))
                         ftemp = MINREALVAL (feature);
                    featureRealSET (feature, ftemp);
                    testStatus ("setCamera Real Set");
               }
               else {
                    if (ftemp > MAXBINVAL (feature))
                         ftemp = MAXBINVAL (feature);
                    if (ftemp < MINBINVAL (feature))
                         ftemp = MINBINVAL (feature);
                    ltemp = ftemp;
                    featureSET (feature, ltemp);
                    testStatus ("setCamera Bin Set");
               }


               /* watch out, if scaling shutter or gain UP, we might not */
               /* have asked for enough increase, FORCE an increase by */
               /* incrementing integer value by 1 */
               if (prev) {      /* here >0 is flag, convert to previous value */
                    prev = ltemp;
                    featureGET (feature, &ltemp);
                    if (prev == ltemp) {        /* didn't get incremented! */
                         ltemp += 1;    /* increment it one */
                         featureRealOnOff (feature, 0);
                         featureSET (feature, ltemp);
                         testStatus ("previous bumper");
                         featureRealOnOff (feature, 1);
                    }
               }

               /* now, get the values back for the cameraModule */
               featureGET (feature, &ltemp);
               testStatus ("setCamera reget");

               if (HAS_REAL (feature)) {
                    featureRealGET (feature, &ftemp);
                    testStatus ("setCamera reget real");
               }

               if (feature == FEATURE_SHUTTER) {
                    cameraModule.shutter = ltemp;
                    cameraModule.intTime =
                         HAS_REAL (feature) ? ftemp : shutterToReal (ltemp);
               }
               else {
                    cameraModule.gain = ltemp;
                    cameraModule.realGain =
                         HAS_REAL (feature) ? ftemp : gainToReal (ltemp);
               }
               return status;
               break;

          case FEATURE_WHITE_BALANCE:
               /* this is the encoded value of White balance */
               /* there is no scale or add, just set */
               /* special case, value of 0 turns it off */
               if (value == 0.0)
                    featureOFF (feature);
               else {
                    featureON (feature);
                    ltemp = value;
                    featureSET (feature, ltemp);
               }
               break;

          case MY_FEATURE_WHITE_BALANCE_R:
               /* turn on white balance and set R */
               featureON (FEATURE_WHITE_BALANCE);
               _getCamera (MY_FEATURE_WHITE_BALANCE_B, &B);
               ltemp = value;
               dc1394_set_white_balance (cameraData.handle,
                                         cameraData.camera.node, B, ltemp);
               return 1;
               break;

          case MY_FEATURE_WHITE_BALANCE_B:
               /* turn on white balance and set B */
               featureON (FEATURE_WHITE_BALANCE);
               _getCamera (MY_FEATURE_WHITE_BALANCE_R, &R);
               ltemp = value;
               dc1394_set_white_balance (cameraData.handle,
                                         cameraData.camera.node, ltemp, R);
               return 1;
               break;


          case FEATURE_TRIGGER:
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
_getCamera (int feature, void *value)
{
     unsigned int ltemp;
     long status;
     float ftemp;
     int i;
     unsigned int R, B;

     switch (feature) {

               /* all of these behave the same */
          case FEATURE_BRIGHTNESS:
          case FEATURE_EXPOSURE:
          case FEATURE_SHARPNESS:
          case FEATURE_HUE:
          case FEATURE_SATURATION:
          case FEATURE_GAMMA:
          case FEATURE_IRIS:
          case FEATURE_FOCUS:
          case FEATURE_TEMPERATURE:
          case FEATURE_TRIGGER_DELAY:
          case FEATURE_WHITE_SHADING:
          case FEATURE_FRAME_RATE:
          case FEATURE_ZOOM:
          case FEATURE_PAN:
          case FEATURE_TILT:
          case FEATURE_OPTICAL_FILTER:
          case FEATURE_CAPTURE_SIZE:
          case FEATURE_CAPTURE_QUALITY:
          case FEATURE_SHUTTER:
          case FEATURE_GAIN:

               if (HAS_FEATURE (feature) == DC1394_FALSE)
                    return -1;
               featureGET (feature, value);
               return status;
               break;

          case MY_FEATURE_SHUTTER_REAL:

               if (HAS_REAL (FEATURE_SHUTTER))
                    featureRealGET (FEATURE_SHUTTER, value);
               else
                    return -1;  /* insert realToShutter here */

               return status;
               break;

          case MY_FEATURE_GAIN_REAL:
               if (HAS_REAL (FEATURE_GAIN))
                    featureRealGET (FEATURE_GAIN, value);
               else
                    return -1;  /* insert realToGain here */

               return status;
               break;

          case FEATURE_WHITE_BALANCE:
               break;

          case MY_FEATURE_WHITE_BALANCE_R:
               /* return R value */
               dc1394_get_white_balance (cameraData.handle,
                                         cameraData.camera.node, &B, value);
               return 1;
               break;

          case MY_FEATURE_WHITE_BALANCE_B:
               /* return R value */
               dc1394_get_white_balance (cameraData.handle,
                                         cameraData.camera.node, value, &R);
               return 1;
               break;

          case FEATURE_TRIGGER:
               break;

          default:
               return -1;
               break;

     }

}


void
_init ()
{
     struct _commandFileEntry_ *ptr;
     long size;
     int numCameras;
     int i;
     unsigned int ui;
     int j;
     int foundCamera = 0;
     char line[128];
     quadlet_t value[2];
     char *tmp;
     int debug;
     int verbose;
     int status;
     long myValue;
     int portNode;
     dc1394bool_t yesNo;
     unsigned int B, R;
     float fvalue;
     unsigned int bpp;
     float myFrameRate;

     strcpy (module, "micropix");
     strcpy (cameraModule.module, module);

     /* init cameraData struct to all 0 */
     memset (&cameraData, 0, sizeof (cameraData));


     /* find out how verbose I am supposed to be */
     getLongParam (&cameraModule.verbose, "cameraverbose", module);
     verbose = cameraModule.verbose;

     /* stop verbosity if output is not a terminal! It makes me crash! */
     if (!isatty (1))
          verbose = cameraModule.verbose = 0;

     /* get my camera number */
     getLongParam (&cameraModule.cameraNumber, "cameraNumber", module);
     if (hotmParams.cameraNumber > -1)
          cameraModule.cameraNumber = hotmParams.cameraNumber;

     /* look up the camera number to UID mapping from the file */
     tmp = mapCameraToUID (cameraModule.cameraNumber);
     if (NULL == tmp) {
          printf ("%s: cannot map number %d to UID. Giving up.\n",
                  module, cameraModule.cameraNumber);
          exit (-1);
     }
     strcpy (cameraData.UID, tmp);

     /* get port and node on bus, side effect is getting camera info */
     portNode = mapUIDToPortAndNode (cameraData.UID,
                                     verbose & VERB_MAPPING,
                                     &cameraData.info);
     cameraData.myNode = portNode & 0xff;

     /* report raw camera data, if verbosity */
     if (verbose & VERB_CAMERA) {
          KIDID;
          dc1394_print_camera_info (&cameraData.info);
     }

     /* figure out what type of camera we have */
     mapCameraNameToData ();
     if (typeData.maxX == 0) {
          printf ("%s: unknown camera model %s, aborting...\n",
                  module, cameraData.info.model);
          for (i = 0; cameraData.info.model[i]; i++)
               printf ("%02x ", cameraData.info.model[i]);
          exit (-2);
     }
     /* keep track of camera name for later */
     strcpy (cameraModule.module, typeData.name);

     get1394StringParam (&cameraData.mode, "cameramode", module);
     get1394StringParam (&cameraData.format, "cameraFormat", module);
     cameraModule.format =
          modeToFormat (getString (findCommand ("cameraMode")));

     /* check for unset mode and format */
     if (cameraData.mode == 0) {
          cameraData.mode = dc1394Decode (typeData.defaultMode);
          cameraModule.format = modeToFormat (typeData.defaultMode);
     }
     /* before you waste your time trying to figure out why this 
        shouldn't work, notice that one "format" is cameraDATA and
        other is cameraMODULE. CameraDATA.format contains the FORMAT
        CODE for the camera, while the cameraMODULE.format contains
        an internal code for the DATA format. Those are two hours
        I'll never get back. Now go watch "My Spoon Is Too Big". */
     if (cameraData.format == 0)
          cameraData.format = dc1394Decode (typeData.defaultFormat);

     if (verbose & VERB_FORMAT) {
          KIDID;
          printf ("cam %d data mode %d interal format %d camera format %d\n",
                  cameraModule.cameraNumber,
                  cameraData.mode, cameraModule.format, cameraData.format);
     }

     get1394StringParam (&cameraData.frameRate, "cameraFrameRate", module);
     if (cameraData.frameRate == 0)
          cameraData.frameRate = dc1394Decode (typeData.defaultFramerate);
     cameraModule.rate = cameraData.frameRate;

     cameraModule.top = cameraModule.left = 0;
     cameraModule.x = cameraModule.y = 0;

     getLongParam (&cameraModule.top, "cameraTop", module);
     getLongParam (&cameraModule.left, "cameraLeft", module);
     getLongParam (&cameraModule.x, "camerax", module);
     getLongParam (&cameraModule.y, "cameray", module);

     /* default camera speed to 400 */
     cameraData.speed = SPEED_400;
     get1394StringParam (&cameraData.speed, "cameraspeed", module);

     /* look for all the things that can be a feature */
     for (i = 0; i < NUM_FEATURES; i++) {

          /* say it is unset */
          cameraData.myFeatures[i] = FEATURE_UNSET;

          /* prepend "camera" to the param name */
          strcpy (line, "camera");

          /* append the feature name */
          strcat (line, dc1394_feature_desc[i]);

          /* do a getParam on it */
          getLongParam (&cameraData.myFeatures[i], line, module);

          if ((verbose & VERB_FEATURE)
              && (FEATURE_UNSET != cameraData.myFeatures[i])) {
               KIDID;
               printf ("cam %d %s: %s is ",
                       cameraModule.cameraNumber, module, line);
               if (FEATURE_AUTO == cameraData.myFeatures[i])
                    printf ("AUTO\n");
               else
                    printf ("%d\n", cameraData.myFeatures[i]);
          }
     }

     cameraModule.startCamera = _startCamera;
     cameraModule.stopCamera = _stopCamera;
     cameraModule.getFrame = _getFrame;
     cameraModule.releaseFrame = _releaseFrame;
     cameraModule.setCamera = _setCamera;
     cameraModule.getCamera = _getCamera;
     cameraModule.initCamera = _init;

     /*first we need to get access to the raw1394subsystem */
     cameraData.handle = dc1394_create_handle (portNode >> 8);
     if (cameraData.handle == NULL) {
          printf ("Unable to aquire a raw1394 handle\n");
          exit (1);
     }

     defaultAlarm ("init init_camera", module);
     status = dc1394_init_camera (cameraData.handle, cameraData.myNode);
     clearAlarm ();

     if (DC1394_SUCCESS != status) {
          KIDID;
          printf ("%s: cannot initialize camera", module);
          exit (-1);
     }

     /* let's try turning it on */
     /*status = dc1394_camera_on( cameraData.handle,
        cameraData.myNode );
        testStatus( "turn camera on" );

        sleep(1); */


     {
          int version;
          status = dc1394_get_sw_version (cameraData.handle,
                                          cameraData.myNode, &version);
          testStatus ("get version of camera");
          if (verbose & VERB_FORMAT) {
               KIDID;
               printf ("IIDC version of camera: %d\n", version);
          }
     }


     if (cameraData.speed > SPEED_400) {
          status = dc1394_set_operation_mode (cameraData.handle,
                                              cameraData.myNode,
                                              OPERATION_MODE_1394B);
          testStatus ("set 1394b");
     }

     /* whew, can I talk to that camera now? */
     if (cameraData.format == FORMAT_SCALABLE_IMAGE_SIZE) {

          /* deal with max image size, unit pos */
          /* and bytes/packet and etc. */
          unsigned int maxX, maxY;
          unsigned int unitPosX, unitPosY;
          unsigned int unitSizeX, unitSizeY;
          unsigned int minBPP, maxBPP;  /* bytes/packet */
          unsigned int ppf;
          unsigned int depth;
          unsigned long totalBytes;

          /* no, not yet. Re-decode color coding id (RGB8, etc)
           *  because FORMAT7 MODE doesn't give color coding at
           *  all!  Must look for cameraColorCoding value, which 
           *  will be something like COLOR_FORMAT7_RGB8, etc... */
          cameraModule.format =
               modeToFormat (getString (findCommand ("cameraColorCoding")));

          if (cameraModule.format < 0) {
               printf
                    ("cam %d: MUST specify cameraColorCoding for format 7!\n",
                     cameraModule.cameraNumber);
               exit (-1);
          }

          if (verbose & VERB_FORMAT) {
               KIDID;
               printf
                    ("cam %d data mode %d interal format %d camera format %d\n",
                     cameraModule.cameraNumber, cameraData.mode,
                     cameraModule.format, cameraData.format);
          }

          /* now get color coding again for format7 setup */
          i = dc1394Decode (getString (findCommand ("cameraColorCoding")));

          status = dc1394_set_format7_color_coding_id (cameraData.handle,
                                                       cameraData.myNode,
                                                       cameraData.mode, i);
          testStatus ("format7 color coding");


          cameraModule.maxX = cameraModule.maxY = 0;

          status = dc1394_query_format7_max_image_size (cameraData.handle,
                                                        cameraData.myNode,
                                                        cameraData.mode,
                                                        &maxX, &maxY);
          testStatus ("get format7 image size");

          cameraModule.maxX = maxX;
          cameraModule.maxY = maxY;

          status = dc1394_query_format7_unit_size (cameraData.handle,
                                                   cameraData.myNode,
                                                   cameraData.mode,
                                                   &unitSizeX, &unitSizeY);

          testStatus ("get format7 unit size");

          status = dc1394_query_format7_unit_position (cameraData.handle,
                                                       cameraData.myNode,
                                                       cameraData.mode,
                                                       &unitPosX, &unitPosY);

          testStatus ("get format7 unit position");

          if (cameraModule.left % unitPosX)
               cameraModule.left -= cameraModule.left % unitPosX;

          if (cameraModule.top % unitPosY)
               cameraModule.top -= cameraModule.top % unitPosY;

          if (cameraModule.x % unitSizeX)
               cameraModule.x -= cameraModule.x % unitSizeX;

          if (cameraModule.y % unitSizeY)
               cameraModule.y -= cameraModule.y % unitSizeY;

          if ((cameraModule.left + cameraModule.x) > maxX)
               cameraModule.x = maxX - cameraModule.left;

          if ((cameraModule.top + cameraModule.y) > maxY)
               cameraModule.top = maxY - cameraModule.top;

          status = dc1394_query_format7_packet_para (cameraData.handle,
                                                     cameraData.myNode,
                                                     cameraData.mode,
                                                     &minBPP, &maxBPP);

          testStatus ("get format7 packet para");

          myFrameRate = 1.875 * (1 << (cameraData.frameRate - FRAMERATE_MIN));

          /* set bytes/packet to something that will be done slightly */
          /* faster than my specified framerate */

          /* for some reason, this call doesn't work! Must assume depth */
          /* status = dc1394_query_format7_data_depth( 
             cameraData.handle, 
             cameraData.myNode, 
             cameraData.mode,
             &depth );

             testStatus( "get format 7 depth" ); */
          depth = 1;

          totalBytes = cameraModule.x * cameraModule.y * depth;

          bpp = (totalBytes / (int) (8000 / (myFrameRate * 1.20)) / minBPP) *
               minBPP;

          status = dc1394_dma_setup_format7_capture (cameraData.handle, cameraData.myNode, cameraModule.cameraNumber, cameraData.mode, cameraData.speed, bpp, cameraModule.left, cameraModule.top, cameraModule.x, cameraModule.y, 10,  /* dma buffs */
                                                     0, /* drop frames */
                                                     NULL,      /* dma device file */
                                                     &cameraData.camera);

          testStatus ("format7 DMA setup");

          /* get current pack/frame */
          status = dc1394_query_format7_packet_per_frame (cameraData.handle,
                                                          cameraData.myNode,
                                                          cameraData.mode,
                                                          &ppf);

          cameraModule.xmitMicros = ppf * 125;
          printf ("ppf: %d  xmitMicros: %d\n", ppf, cameraModule.xmitMicros);

          status = dc1394_set_video_framerate (cameraData.handle,
                                               cameraData.myNode,
                                               cameraData.frameRate);
          testStatus ("format7 set framerate");

          tVr;

          status = SetCameraControlRegister (cameraData.handle,
                                             cameraData.camera.node,
                                             0x083cLL, 0x42000000LL);

          /* SetCamera... GetCamera success is ZERO! */
          if (status == 0)
               status = DC1394_SUCCESS;
          testStatus ("set framerate register format 7");

          status = DC1394_SUCCESS;

     }
     else {

          unsigned int ppf;

          status = dc1394_dma_setup_capture (cameraData.handle, cameraData.myNode, cameraModule.cameraNumber, cameraData.format, cameraData.mode, cameraData.speed, cameraData.frameRate, 10,   /* number of buffers */
                                             0, /* dropframes? */
                                             0, /* dma device file. NULL==use default */
                                             &cameraData.camera);

          testStatus ("dma setup non-format7");
          tVr;

          ppf = cameraData.camera.dma_frame_size
               / cameraData.camera.quadlets_per_packet / 4;
          cameraModule.xmitMicros = ppf * 125;
          printf ("ppf: %d  xmitMicros: %d\n", ppf, cameraModule.xmitMicros);

     }

     if (DC1394_SUCCESS != status) {
          KIDID;
          printf ("%s: failure to setup dma capture! status: %d\n", module,
                  status);
          dc1394_dma_release_camera (cameraData.handle, &cameraData.camera);
          raw1394_destroy_handle (cameraData.handle);
          exit (-1);
     }

     /* report back how big a "buffer" is */
     cameraModule.bufferLength = cameraData.camera.dma_frame_size;

     /* report back some data on what I did when I setup */
     if (verbose & VERB_CAMERA) {
          KIDID;
          printf ("cam %d on port %d node %d (%d) device %s fd %d\n",
                  cameraModule.cameraNumber,
                  cameraData.camera.port,
                  cameraData.myNode, cameraData.camera.node,
                  cameraData.camera.dma_device_file,
                  cameraData.camera.dma_fd);
          KIDID;
          printf ("cam %d dma buffersize: %d framesize: %d numbuffers: %d\n",
                  cameraModule.cameraNumber,
                  cameraData.camera.dma_buffer_size,
                  cameraData.camera.dma_frame_size,
                  cameraData.camera.num_dma_buffers);
          KIDID;
          printf ("cam %d last buffer: %d channel: %d \n",
                  cameraModule.cameraNumber,
                  cameraData.camera.dma_last_buffer,
                  cameraData.camera.channel);
     }

     /* now set all the features that I know about. */
     status = dc1394_get_camera_feature_set (cameraData.handle,
                                             cameraData.camera.node,
                                             &cameraData.features);
     if (DC1394_SUCCESS != status) {
          printf ("%s: unable to get all features, can I set them?\n",
                  module);
          exit (-1);
     }
     /* dc1394_print_feature_set(&cameraData.features); */


     /* now figure out the basic settings and set them */

     autoOFF (FEATURE_BRIGHTNESS);
     testStatus ("autoOFF BRIGHTNESS");
     tVr;
     featureOFF (FEATURE_BRIGHTNESS);
     testStatus ("featureOFF BRIGHTNESS");
     tVr;
     autoOFF (FEATURE_EXPOSURE);
     testStatus ("autoOFF EXPOSURE");
     tVr;
     featureOFF (FEATURE_EXPOSURE);
     testStatus ("featureOFF EXPOSURE");
     tVr;

     autoOFF (FEATURE_WHITE_BALANCE);
     testStatus ("autoOFF WHITE_BALANCE");
     featureON (FEATURE_WHITE_BALANCE);
     testStatus ("featureON WHITE_BALANCE");
     /* values set later in demon */

     featureOFF (FEATURE_GAMMA);

     autoOFF (FEATURE_GAIN);
     testStatus ("autoOFF GAIN");
     featureON (FEATURE_GAIN);
     testStatus ("featureON GAIN");
     status = _setCamera ("CAM_SET", "FEATURE_GAIN", 0.01);
     testStatus ("setCamera GAIN");

     autoOFF (FEATURE_SHUTTER);
     testStatus ("autoOFF SHUTTER");
     tVr;
     featureON (FEATURE_SHUTTER);
     testStatus ("featureON SHUTTER");
     tVr;
     status = _setCamera ("CAM_SET", "FEATURE_SHUTTER", 0.03);
     testStatus ("featureSET SHUTTER");

     if (cameraData.format == FORMAT_SCALABLE_IMAGE_SIZE) {
          status = _setCamera ("CAM_SET", "FEATURE_FRAME_RATE", myFrameRate);
          testStatus ("format7 final framerate setting");
     }

     if (verbose & VERB_CAMERA) {
          _getCamera (MY_FEATURE_SHUTTER_REAL, &fvalue);
          printf ("After setting, shutter is %f\n", fvalue);
          _getCamera (MY_FEATURE_GAIN_REAL, &fvalue);
          printf ("After setting, gain    is %f\n", fvalue);
     }

     /* now get all the features that I know about. */
     status = dc1394_get_camera_feature_set (cameraData.handle,
                                             cameraData.camera.node,
                                             &cameraData.features);
     if (DC1394_SUCCESS != status) {
          printf ("%s: unable to get all features for the second time\n",
                  module);
          exit (-1);
     }

     /*if( verbose & VERB_FEATURE )
        dc1394_print_feature_set(&cameraData.features); */

     myValue = -1;
     get1394StringParam (&myValue, "cameratrigger", "");
     if (myValue != -1) {
          printf ("turning on trigger mode %d\n", myValue);
          status = dc1394_set_trigger_mode (cameraData.handle,
                                            cameraData.camera.node, myValue);
          testStatus ("set trigger mode");
          status = dc1394_set_trigger_on_off (cameraData.handle,
                                              cameraData.camera.node, 1);
          testStatus ("set trigger on");
     }

     /* white balance */
     myValue = -1;
     getLongParam (&myValue, "camerawhitebalanceb", "");
     if (myValue != -1) {
          B = myValue & 0xffff;
          getLongParam (&myValue, "camerawhitebalancer", "");
          R = myValue & 0xffff;
          status = dc1394_set_white_balance (cameraData.handle,
                                             cameraData.camera.node, B, R);
          /*printf("white balance: U: %d V: %d\n", B, R ); */
          if (DC1394_SUCCESS != status)
               printf ("%s: cannot set white balance!\n", module);
     }

     /* turn on raw data */
     printf ("sccr: %d\n", SetCameraControlRegister (cameraData.handle,
                                                     cameraData.camera.node,
                                                     0x12F8LL, 0x03FFUL));

     printf ("Y16: %d\n", SetCameraControlRegister (cameraData.handle,
                                                    cameraData.myNode,
                                                    0x1048LL, 0x0081UL));


     /* get pixel ordering, if there */
     GetCameraControlRegister (cameraData.handle,
                               cameraData.camera.node, 0x1040LL, value);
     value[1] = htonl (value[0]);
     memcpy (cameraModule.rawOrder, &value[1], 4);

     cameraModule.x = cameraData.camera.frame_width;
     cameraModule.y = cameraData.camera.frame_height;
     cameraModule.cameraWidth = typeData.maxX;
     cameraModule.cameraHeight = typeData.maxY;

     /* if we weren't set based on format 7 registers, we are copies
        of format0/1/2 data */
     if (0 == cameraModule.maxY) {
          cameraModule.maxY = cameraModule.cameraHeight;
          cameraModule.maxX = cameraModule.cameraWidth;
     }

     if (verbose & VERB_AUTO) {
          KIDID;
          printf ("%s: shutter: %d gain: %d integrate: %lf secs\n",
                  cameraModule.module,
                  cameraModule.shutter,
                  cameraData.features.feature[FEATURE_GAIN -
                                              FEATURE_MIN].value,
                  cameraModule.intTime);
     }


}
