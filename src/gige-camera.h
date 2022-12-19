
#include "C/FlyCapture2_C.h"
/* for DC1394_FEATURE defines */
#include <dc1394/dc1394.h>

#include <time.h>


/* two feature values I might use in default.command */
#define FEATURE_AUTO (-3)
#define FEATURE_UNSET (-4)
#define FEATURE_ONEPUSH (-5)

#define VERB_START 1
#define VERB_STOP 2
#define VERB_GETFRAME 4
#define VERB_FORMAT 8
#define VERB_FEATURE 32
#define VERB_CAMERA 16
#define VERB_MAPPING 64
#define VERB_AUTO 128
#define VERB_SET 256
#define VERB_TIMING 512

#ifdef CAMERAMODULE 
	#define CEXTERN 
#else
	#define CEXTERN extern
#endif 

char *PropertyNames[] = { "FC2_BRIGHTNESS",
                "FC2_AUTO_EXPOSURE",
                "FC2_SHARPNESS",
                "FC2_WHITE_BALANCE",
                "FC2_HUE",
                "FC2_SATURATION",
                "FC2_GAMMA",
                "FC2_IRIS",
                "FC2_FOCUS",
                "FC2_ZOOM",
                "FC2_PAN",
                "FC2_TILT",
                "FC2_SHUTTER",
                "FC2_GAIN",
                "FC2_TRIGGER_MODE",
                "FC2_TRIGGER_DELAY",
                "FC2_FRAME_RATE",
                "FC2_TEMPERATURE",
                "FC2_UNSPECIFIED_PROPERTY_TYPE" };



/* data for the currently two sony cameras supported
	by this module. */
struct {
	char	name[64];
	char	defaultFramerate[64];
        char    defaultColorCoding[64];
	int	X;
	int	Y;
	int	left;
	int	top;
        int     referenceWidth;   /* how big is the "full size" image?*/
        int     referenceHeight;  
} typeData;

/* info from the format 7 requests for max size, unit,  etc */
CEXTERN struct {
     unsigned int h_size;
     unsigned int v_size;
     unsigned int h_unit;
     unsigned int v_unit;
     fc2PixelFormat codes;
} format7data;

#define NUM_FEATURES 100

struct {

#include "cameraDataCommon.h"

	fc2Context context;
	fc2Image frame;
	fc2PGRGuid guid;
	fc2CameraInfo camInfo;
	fc2GigEImageSettingsInfo imageSettingsInfo;
	fc2GigEImageSettings imageSettings;

	long useMultiShot; /* use image buffer on cam */
	long isTakingData;

} cameraData;

/* parameters for setCamera */
#define CAM_SET 1
#define CAM_SCALE 2
#define CAM_ADD 3
#define CAM_AUTO 4
#define CAM_OFF 5
#define MY_FEATURE_SHUTTER_REAL 1
#define MY_FEATURE_GAIN_REAL 2
#define MY_FEATURE_WHITE_BALANCE_R 3
#define MY_FEATURE_WHITE_BALANCE_B 4
#define DC1394_FEATURE_NULL 500
/* remainder of features are in dc1394control.h */

int PrintStreamChannel();
int PrintStreamChannelInfo();
int PrintCameraInfo();
int PrintPropertyInfo();
int PrintProperty();
int SetTimeStamping();
int PrintImageInfo();
int PrintConfigInfo();
