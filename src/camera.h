#include <dc1394/dc1394.h>

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

char *dc1394Encode();
long dc1394Decode();
int  mapUIDToPortAndNode();

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
     dc1394color_codings_t   codes;
} format7data;

#define NUM_FEATURES DC1394_FEATURE_NUM

struct {

#include "cameraDataCommon.h"

        dc1394_t *dc1394;
	dc1394camera_t *camera;
        dc1394camera_list_t *cameraList;
        dc1394camera_id_t *myCamera;
        dc1394featureset_t features;

	char undef[10000]; /* buffer for other camrea's data */

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

