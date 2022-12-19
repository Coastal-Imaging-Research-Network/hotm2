
#include "SpinnakerC.h"
#include "stdio.h"
#include "string.h"
/* for DC1394_FEATURE defines */
/*#include <dc1394/dc1394.h>*/

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
#define VERB_1588TIME 1024

#ifdef CAMERAMODULE 
	#define CEXTERN 
#else
	#define CEXTERN extern
#endif 

#define MAX_BUFF_LEN 256
typedef struct _userData
{
	int count;
	char eventName[MAX_BUFF_LEN];
} userData;


/* this is now the official name of property for setCamea and 
   getCamera. What to get or set becomes a switch once I find 
   the index into this array of the name I'm asked for. */
char *PropertyNames[] = { "FEATURE_BRIGHTNESS",
                "FEATURE_SHARPNESS",
                "FEATURE_WHITE_BALANCE",
		"FEATURE_WHITE_BALANCE_R",
		"FEATURE_WHITE_BALANCE_B",
                "FEATURE_HUE",
                "FEATURE_SATURATION",
                "FEATURE_GAMMA",
                "FEATURE_IRIS",
                "FEATURE_FOCUS",
                "FEATURE_ZOOM",
                "FEATURE_PAN",
                "FEATURE_TILT",
                "FEATURE_SHUTTER",
                "FEATURE_GAIN",
                "FEATURE_TRIGGER_MODE",
                "FEATURE_TRIGGER_DELAY",
                "FEATURE_FRAME_RATE",
                "FEATURE_TEMPERATURE",
		"FEATURE_AUTO_GAIN",
                "FEATURE_AUTO_EXPOSURE",
		"FEATURE_RESYNC_TIME",
		"FEATURE_SET_INTLIMIT",
                "UNSPECIFIED_PROPERTY_TYPE" };

enum { FEAT_BRIGHTNESS,
                FEAT_SHARPNESS,
                FEAT_WHITE_BALANCE,
		FEAT_WHITE_BALANCE_R,
		FEAT_WHITE_BALANCE_B,
                FEAT_HUE,
                FEAT_SATURATION,
                FEAT_GAMMA,
                FEAT_IRIS,
                FEAT_FOCUS,
                FEAT_ZOOM,
                FEAT_PAN,
                FEAT_TILT,
                FEAT_SHUTTER,
                FEAT_GAIN,
                FEAT_TRIGGER_MODE,
                FEAT_TRIGGER_DELAY,
                FEAT_FRAME_RATE,
                FEAT_TEMPERATURE,
		FEAT_AUTO_GAIN,
                FEAT_AUTO_EXPOSURE,
		FEAT_RESYNC_TIME,
		FEAT_SET_INTLIMIT,
                FEAT_UNSPECIFIED_PROPERTY_TYPE };
	

char *spinNames[] = { "FEATURE_BRIGHTNESS",
                "FEATURE_SHARPNESS",
                "FEATURE_WHITE_BALANCE",
		"FEATURE_WHITE_BALANCE_R",
		"FEATURE_WHITE_BALANCE_B",
                "FEATURE_HUE",
                "Saturation",
                "FEATURE_GAMMA",
                "FEATURE_IRIS",
                "FEATURE_FOCUS",
                "FEATURE_ZOOM",
                "FEATURE_PAN",
                "FEATURE_TILT",
                "ExposureTime",
                "Gain",
                "FEATURE_TRIGGER_MODE",
                "FEATURE_TRIGGER_DELAY",
                "FEATURE_FRAME_RATE",
                "FEATURE_TEMPERATURE",
		"FEATURE_AUTO_GAIN",
                "FEATURE_AUTO_EXPOSURE",
		"FEATURE_RESYNC_TIME",
                "UNSPECIFIED_PROPERTY_TYPE" };


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
     //fc2PixelFormat codes;
} format7data;

#define NUM_FEATURES 100

CEXTERN typedef struct { 
	double ExposureTime; 
	double Gain; 
	double TriggerMode; 
	double BalanceRatio;
} _limits;

typedef enum _deviceEventType
{
    GENERIC,
    SPECIFIC
} deviceEventType;
const deviceEventType chosenEvent = GENERIC;



extern struct {

#include "cameraDataCommon.h"

	spinSystem        hSystem;
	spinCameraList    hCameras;
	spinCamera        hCam;
	spinNodeMapHandle hNodeMap;
	spinNodeMapHandle hNodeTLDevice;
        spinNodeHandle    hNodeHandle;
	/* deep copy of incoming frame. remember to spinImageCreateEmpty */
	spinImage         hFrame;
	quickSpin         qs;  /* camera quick spin */
	quickSpinTLDevice qsD; /* device (TL) quick spin */

	long useMultiShot; /* use image buffer on cam */

	struct {
		_limits min;
		_limits max;
	} limits;

	long  use1588time;
	int   framesReady;  /* a counter of frames ready to fetch */
	long   ignoreEvents;

} cameraData;

/* parameters for setCamera */
#define CAM_SET 1
#define CAM_SCALE 2
#define CAM_ADD 3
#define CAM_AUTO 4
#define CAM_OFF 5
#define CAM_ONCE 6
#define CAM_ON 7  /* for ON settings */

CEXTERN char *CameraActions[] = {
	"NONE",
	"CAM_SET",
	"CAM_SCALE",
	"CAM_ADD",
	"CAM_AUTO",
	"CAM_OFF",
	"CAM_ONCE",
	"NULL_ACTION" };

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
int PrintImageInfo();
int PrintConfigInfo();
//extern "C" void mapCameraNameToData(void *,char *);
//extern "C" void defaultAlarm( char *, char * );
