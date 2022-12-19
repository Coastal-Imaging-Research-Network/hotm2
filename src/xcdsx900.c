
/*
	xcdsx900.c -- the camera module for the Sony XCD-SX900
	
	There are just too many 'gotcha's for this camera to be
	able to use the default.camera module productively. 
	
	1. no auto exposure control
	2. poor format 7 control
	
	loaded dynamically by hotm. contains the following functions:
	
	_init -- called by dlopen by default, must set up camera
	_startCamera -- turn on collection using previously determined
		parameters
	_stopCamera  -- turn camera off
	_getFrame    -- get the next frame please
	
*/

/* 
 *  Copied from default.camera.c
 *
 *  $Log: xcdsx900.c,v $
 *  Revision 1.2  2002/11/06 21:26:59  stanley
 *  changed call to mapUIDtoport and node
 *
 *  Revision 1.1  2002/08/10 00:24:55  stanleyl
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

static const char rcsid[] = "$Id: xcdsx900.c 172 2016-03-25 21:03:51Z stanley $";

#include "hotm.h"
#include <libraw1394/raw1394.h>
#include <libdc1394/dc1394_control.h>

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

struct {
	int	number;
	char	UID[128];
	struct	_frame_ testFrame;
	long	mode;
	long	format;
	long	frameRate;
	long	myFeatures[NUM_FEATURES];
	long	speed; /*  100/200/400 */
	dc1394_cameracapture camera;
	raw1394handle_t handle;
	nodeid_t *cameraNodes;
	dc1394_feature_set features;
	dc1394_camerainfo info;
	int	myNode;
} cameraData;

void sx900AutoExposure( long );

void _startCamera(struct _cameraModule_ *cam)
{
int err;
	if( cam->verbose & VERB_START ) { 
		KIDID;
		printf("cam %d ", cam->cameraNumber );
		printf("start camera called, module \"%s\" \n", cam->module);
	}
	
	err = dc1394_start_iso_transmission( cameraData.handle, 
						cameraData.camera.node );
						
	if( DC1394_SUCCESS != err ) {
		KIDID;
		printf("startCamera: failed to start\n");
		exit(-1);
	}
}

void _stopCamera(struct _cameraModule_ *cam)
{
int err;

	if( cam->verbose & VERB_STOP ) {
		KIDID;
		printf("cam %d stop camera called\n", cameraModule.cameraNumber );
	}
			
	err = dc1394_stop_iso_transmission( cameraData.handle, 
					cameraData.camera.node );
					
	if( DC1394_SUCCESS != err ) {
		KIDID;
		printf("stopCamera: failed to stop iso!!\n" );
	}
	err = dc1394_dma_release_camera( cameraData.handle, &cameraData.camera );
	if( DC1394_SUCCESS != err ) {
		KIDID;
		printf("stopCamera: failed to release camera!!\n" );
	}
	dc1394_destroy_handle( cameraData.handle );

}

struct _frame_ *_getFrame(struct _cameraModule_ *cam)
{
int err;

	cameraData.testFrame.status = 0;
	if( cam->verbose & VERB_GETFRAME ) {
		KIDID;
		printf("cam %d getFrame ... ", cam->cameraNumber );
		fflush(NULL);
	}
		
	err = dc1394_dma_single_capture( &cameraData.camera );

	if( DC1394_SUCCESS != err ) {
		KIDID;
		printf("getFrame: died with err on dma get\n");
		exit(-1);
	}
	cameraData.testFrame.dataPtr = 
		(unsigned char *)cameraData.camera.capture_buffer;
	cameraData.testFrame.when = cameraData.camera.filltime;
	
	if( cam->verbose & VERB_GETFRAME ) {
		KIDID;
		printf("cam %d ", cam->cameraNumber );
		printf("getframe at %d.%06d\n", 
			cameraData.camera.filltime.tv_sec,
			cameraData.camera.filltime.tv_usec );
	}
			
	return &cameraData.testFrame;
	
}

void _releaseFrame( )
{
int status;

	status = dc1394_dma_done_with_buffer( &cameraData.camera );
	if( DC1394_SUCCESS != status ) {
		KIDID;
		printf("releaseFrame: failed to release %d\n",
			cameraData.camera.dma_last_buffer );
	}

}

void _init()
{
struct 	_commandFileEntry_ *ptr;
char 	module[] = "xcd-sx900";
long	size;
int 	numCameras;
int 	i;
int	foundCamera = 0;
char	line[128];
quadlet_t	value[2];
char	*tmp;
int	debug;
int 	verbose;
int	status;
long	myValue;
int 	portNode;
dc1394bool_t yesNo;
	    
	strcpy( cameraModule.module, module );
	
	/* init cameraData struct to all 0 */
	memset( &cameraData, 0, sizeof(cameraData) );
	
	getLongParam( &cameraModule.verbose, "cameraverbose", module );
	verbose = cameraModule.verbose;	
	getLongParam( &cameraModule.cameraNumber, "cameraNumber", module );
	
	tmp = mapCameraToUID( cameraModule.cameraNumber );
	if( NULL == tmp ) {
		printf("%s: cannot map number %d to UID. Giving up.\n",
			module, 
			cameraModule.cameraNumber );
		exit(-1);
	}
	strcpy( cameraData.UID, tmp );

	get1394StringParam( &cameraData.mode, "cameramode", module );
	get1394StringParam( &cameraData.format, "cameraFormat", module );
	get1394StringParam( &cameraData.frameRate, "cameraFrameRate", module );
	cameraModule.rate = cameraData.frameRate;
	
	cameraModule.top = 
    cameraModule.left = 0;
	cameraModule.x = 
	cameraModule.y = 0;
	
	getLongParam( &cameraModule.top, "cameraTop", module );
	getLongParam( &cameraModule.left, "cameraLeft", module );
	getLongParam( &cameraModule.x, "camerax", module );
	getLongParam( &cameraModule.y, "cameray", module );
	
	/* special process for XCD-SX900 -- 1/4 screen blocks */
	/* x and left must be multiples of 320 */
	i = (cameraModule.left/320) * 320;
	if( i != cameraModule.left ) {
		KIDID;
		printf("cam %d changed left from %d to %d\n",
				cameraModule.cameraNumber, 
				cameraModule.left, 
				i );
		cameraModule.left = i;
	}
	i = (cameraModule.x/320) * 320;
	if( i+cameraModule.left > 1280 )
		i = 1280 - cameraModule.left;
	if( i != cameraModule.x ) {
		KIDID;
		printf("cam %d changed x from %d to %d\n",
				cameraModule.cameraNumber, 
				cameraModule.x, 
				i);
		cameraModule.x = i;
	}
	
	/* y and top must be multiples of 240 */
	i = (cameraModule.top/240) * 240;
	if( i != cameraModule.top ) {
		KIDID;
		printf("cam %d changed top from %d to %d\n",
				cameraModule.cameraNumber, 
				cameraModule.top, 
				i );
		cameraModule.top = i;
	}
	i = (cameraModule.y/240) * 240;
	if( i+cameraModule.top > 960 ) 
		i = 960 - cameraModule.top;
	if( i != cameraModule.y ) {
		KIDID;
		printf("cam %d changed y from %d to %d\n",
				cameraModule.cameraNumber, 
				cameraModule.y, 
				i );
		cameraModule.y = i;
	}
	

	/* default camera speed to 400 */
	cameraData.speed = 400;
	getLongParam( &cameraData.speed, "cameraspeed", module);

	cameraModule.format = modeToFormat( getString(findCommand("cameraMode")) );
	if( verbose & VERB_FORMAT ) {
		KIDID;
		printf("cam %d data format %d\n", 
			cameraModule.cameraNumber,
			cameraModule.format );
	}

	/* look for all the things that can be a feature */
	for( i=0; i<NUM_FEATURES; i++ ) {
		/* say it is unset */
		cameraData.myFeatures[i] = FEATURE_UNSET;
		/* prepend "camera" to the param name */
		strcpy( line, "camera" );
		/* append the feature name */
		strcat( line, dc1394_feature_desc[i] );
		/* do a getParam on it */
		getLongParam( &cameraData.myFeatures[i], line, module );
		if(verbose & VERB_FEATURE ) {
			KIDID;
			printf("cam %d %s: %s was ", 
				cameraModule.cameraNumber, 
				module, 
				line );
			if( FEATURE_UNSET == cameraData.myFeatures[i])
				printf("UNSET\n");
			else if( FEATURE_AUTO == cameraData.myFeatures[i] )
				printf("AUTO\n");
			else
				printf("%d\n", cameraData.myFeatures[i] );
		} 
	}

	cameraModule.startCamera = _startCamera;
	cameraModule.stopCamera = _stopCamera;
	cameraModule.getFrame = _getFrame;
	cameraModule.releaseFrame = _releaseFrame;		
	

    portNode = mapUIDToPortAndNode( cameraData.UID, 
						verbose & VERB_MAPPING,
						&cameraData.info );
    cameraData.myNode = portNode & 0xff;
    
   /*first we need to get access to the raw1394subsystem*/
    cameraData.handle = dc1394_create_handle(portNode>>8);
    if (cameraData.handle==NULL) {
        printf("Unable to aquire a raw1394 handle\n");
        exit(1);
    }
	
    /*get the camera nodes and describe them as we find them*/
    cameraData.cameraNodes =
    	dc1394_get_camera_nodes(cameraData.handle,&numCameras,0);

    
    /* whew, can I talk to that camera now? */
    if( cameraData.format == FORMAT_SCALABLE_IMAGE_SIZE ) {
		status = dc1394_dma_setup_format7_capture (
					cameraData.handle,
					cameraData.myNode,
					cameraModule.cameraNumber,
					cameraData.mode,
					cameraData.speed==400? SPEED_400 : 
						(cameraData.speed==200? SPEED_200 : SPEED_100 ),
					QUERY_FROM_CAMERA,
					cameraModule.left,
					cameraModule.top,
					cameraModule.x,
					cameraModule.y,
					3,
					&cameraData.camera );
	}
	else {		
		status = dc1394_dma_setup_capture( cameraData.handle,
   					cameraData.myNode,
					cameraModule.cameraNumber,
					cameraData.format,
					cameraData.mode,
					cameraData.speed==400? SPEED_400 : 
						(cameraData.speed==200? SPEED_200 : SPEED_100 ),
					cameraData.frameRate,
					10,
					0, 0,
					&cameraData.camera );
	}
    if( DC1394_SUCCESS != status ) {
		KIDID;
    	printf("%s: failure to setup dma capture!\n", module );
		dc1394_dma_release_camera( cameraData.handle, &cameraData.camera );
		raw1394_destroy_handle( cameraData.handle );
		exit(-1);
    }

    /* report back how big a "buffer" is */
    cameraModule.bufferLength = cameraData.camera.dma_frame_size;
    
    /* report back some data on what I did when I setup */
    if( verbose & VERB_CAMERA ) {
		KIDID;
	    printf("cam %d on port %d node %d (%d) device %s fd %d\n", 
			cameraModule.cameraNumber,
			cameraData.camera.port, 
			cameraData.myNode, cameraData.camera.node,
			cameraData.camera.dma_device_file,
			cameraData.camera.dma_fd);
		KIDID;
    	printf("cam %d dma buffersize: %d framesize: %d numbuffers: %d\n",
    			cameraModule.cameraNumber,
				cameraData.camera.dma_buffer_size,
				cameraData.camera.dma_frame_size,
				cameraData.camera.num_dma_buffers );
		KIDID;
   		printf("cam %d last buffer: %d channel: %d \n",
    		cameraModule.cameraNumber,
			cameraData.camera.dma_last_buffer,
			cameraData.camera.channel );
    }
    
    /* now set all the features that I know about. */
    status = dc1394_get_camera_feature_set( cameraData.handle,
    					cameraData.camera.node,
					&cameraData.features );
    if( DC1394_SUCCESS != status ) {
    	printf("%s: unable to get all features, can I set them?\n",
				module );
		exit(-1);
    }
    
    /* and now step through all the features I know about and try */
    /* either setting them or let them alone if unset */
    for( i=0; i<NUM_FEATURES; i++ ) {
    
    	myValue = cameraData.myFeatures[i];

    	/* change only if set, dufus */
    	if( FEATURE_UNSET != myValue ) {
	
		/* now see if AUTO and I can AUTO */
		if( (FEATURE_AUTO == myValue) 
		 && (cameraData.features.feature[i].auto_capable )) {
		 	/* set to auto */ 
			status = dc1394_auto_on_off( cameraData.handle,
						cameraData.camera.node,
						i+FEATURE_MIN,
						1 );
			if( DC1394_SUCCESS != status ) 
				printf("%s: failed setting %s to AUTO!\n", 
					module, 
					dc1394_feature_desc[i] );
		}
		else if( FEATURE_AUTO == myValue )
			printf("%s: cannot set %s to AUTO!\n", 
					module, 
					dc1394_feature_desc[i] );
		else if( (FEATURE_ONEPUSH == myValue)
		     &&  (cameraData.features.feature[i].one_push) ) {
		     	status = dc1394_start_one_push_operation( 
							cameraData.handle,
							cameraData.camera.node,
							i+FEATURE_MIN );
			if( DC1394_SUCCESS != status ) {
				printf("%s: cannot autopush %s\n",
					module,
					dc1394_feature_desc[i] );
				continue;
			}
			while( 1 ) {
				dc1394_is_one_push_in_operation(
						cameraData.handle,
						cameraData.camera.node,
						i+FEATURE_MIN,
						&yesNo ); 
				if( yesNo == 0 ) break;
			}
		}	
		else if( FEATURE_ONEPUSH == myValue )
			printf("%s: cannot one_push %s!\n", 
					module, 
					dc1394_feature_desc[i] );	
		else if( i == (FEATURE_WHITE_BALANCE - FEATURE_MIN) ) {
			/* white balance needs special coding */
			status = dc1394_set_white_balance( cameraData.handle,
						cameraData.camera.node,
						myValue & 0xff,
						(myValue>>8) &0xff );
			if( DC1394_SUCCESS != status )
				printf("%s: cannot set white balance!\n",
					module );
		}
		else {   /* everything else */
			if( myValue < cameraData.features.feature[i].min )
				printf("%s: feature %s value %d lower than min allowed (%d)\n",
					module,
					dc1394_feature_desc[i], myValue,
					cameraData.features.feature[i].min );
			else if( myValue > cameraData.features.feature[i].max )
				printf("%s: feature %s value %d higher than max allowed (%d)\n",
					module,
					dc1394_feature_desc[i], myValue,
					cameraData.features.feature[i].max );
			else {
				status = dc1394_set_feature_value( cameraData.handle,
								cameraData.camera.node,
								i+FEATURE_MIN,
								myValue );
				if( DC1394_SUCCESS != status )
					printf("%s: cannot set feature %s!\n",
						module,
						dc1394_feature_desc[i] );
			}
		
		
	    }
		
	} /* not unset */
	
	if(verbose & VERB_FEATURE) {
		printf("cam %d feature %s current setting ", 
			cameraModule.cameraNumber, dc1394_feature_desc[i] );
		if( cameraData.features.feature[i].auto_active ) 
			printf("AUTO value %d\n", cameraData.features.feature[i].value);
		else
			printf("value %d\n", cameraData.features.feature[i].value);	
		}
		

    }

    /* one last big problem -- autoexposure. Let's try doing it. */
	getLongParam( &myValue, "cameraexposure", module );
	printf("exposure == %d\n", myValue );
	if( myValue == FEATURE_AUTO ) 
		sx900AutoExposure(verbose);  /* return shutter and gain in cameraModule */
	

    cameraModule.x = cameraData.camera.frame_width;
    cameraModule.y = cameraData.camera.frame_height;
    		
}

void sx900AutoExposure(long verbose)
{

long gain;
long shutter;
long shutterMin, shutterMax;
int i, j, k;
char *module = "autoexposure";
long status;
struct _frame_ *myFrame;
long size;
long count;
long max;
unsigned char *ptr;
long percent;
int increment;
int aeLimited = 1;
long aeWidth;
long aeHeight;
long aeTop;
long aeLeft;
char aeValue[128];

	/* start with gain -- either use set value or max */
	getLongParam( &gain, "cameraGain", module );
	if( gain < 0 ) {
		/* not already set, use max gain */
		for(i=0; i<NUM_FEATURES; i++ ) {
			if( cameraData.features.feature[i].feature_id == FEATURE_GAIN )
				break;
		}
		gain = cameraData.features.feature[i].max;
		if( verbose & VERB_AUTO ) {
			KIDID;
			printf("cam %d setting gain to max (%d)\n", 
				cameraModule.cameraNumber, gain );
		}
		status = dc1394_set_feature_value( cameraData.handle,
						cameraData.camera.node,
						FEATURE_GAIN,
						gain );
		if( DC1394_SUCCESS != status ) {
			KIDID;
			printf("cam %d autoexposure unable to set gain to max (%d)\n",
				cameraModule.cameraNumber,
				gain );
		}
	} 
	else {
		KIDID;
		printf("cam %d using specified gain of %d\n", 
				cameraModule.cameraNumber, gain );
	}

	/* gain over, now shutter. find max and min */
	for( i=0; i<NUM_FEATURES; i++ ) 
		if( cameraData.features.feature[i].feature_id == FEATURE_SHUTTER ) 
			break;
	
	shutterMax = cameraData.features.feature[i].max;
	shutterMin = cameraData.features.feature[i].min;
	
	size = cameraModule.x * cameraModule.y;
	
	/* auto-exposure spot -- region of image or whole */
	getStringParam( aeValue, "cameraAERegion", module );
	
	aeWidth = cameraModule.x / 5;
	aeHeight = cameraModule.y / 5;
	
	if( !strcasecmp( aeValue, "full" ) ) {
		aeLimited = 0;
	}
	else if( *aeValue == 0 ) {
		aeLimited = 0;
	}
	else if( !strcasecmp( aeValue, "center" ) ) {
		aeTop = cameraModule.y/2 - aeHeight/2;
		aeLeft = cameraModule.x/2 - aeWidth/2;
	}
	else if( !strcasecmp( aeValue, "topleft" ) ) {
		aeTop = aeHeight/2;
		aeLeft = aeWidth/2;
	}
	else if( !strcasecmp( aeValue, "topright" ) ) {
		aeTop = aeHeight/2;
		aeLeft = cameraModule.x - aeWidth - aeWidth/2;
	}
	else if( !strcasecmp( aeValue, "bottomleft" ) ) {
		aeLeft = aeWidth/2;
		aeTop = cameraModule.y - aeHeight - aeHeight/2;
	}
	else if( !strcasecmp( aeValue, "bottomright" ) ) {
		aeLeft = cameraModule.x - aeWidth - aeWidth/2;
		aeTop = cameraModule.y - aeHeight - aeHeight/2;
	}
	else {
		KIDID;
		printf("cam %d aeSpot invalid: %s\n",
				cameraModule.cameraNumber, 
				aeValue );
		aeLimited = 0;
	}
	
	/* clip this amount at max value */
	percent = -1; /* not set yet */
	getLongParam( &percent, "cameraClippingPercent", module );
	if( percent < 0 )
		percent = 2;
		
	if( verbose & VERB_AUTO ) {
		KIDID;
		printf("cam %d percent clip set to %d%%\n",
				cameraModule.cameraNumber,
				percent );
	}			
	
	/* note: percent becomes pixel count here! */
	if( aeLimited )
		percent = (aeWidth*aeHeight*percent) / 100; 
	else 
		percent = (size * percent) / 100;
	
	/* start by going down (longer) 5 at a time */
	increment = -5;
	
	/* start at max, get frames, keep making smaller until good */
	_startCamera( &cameraModule );
	for( shutter=shutterMax; shutter>shutterMin; shutter+=increment ) {

		status = dc1394_set_feature_value( cameraData.handle,
						cameraData.camera.node,
						FEATURE_SHUTTER,
						shutter );
		if( DC1394_SUCCESS != status ) {
			KIDID;
			printf("cam %d autoexposure unable to set shutter to %d\n",
				cameraModule.cameraNumber,
				shutter );
		}
	
		myFrame = _getFrame( &cameraModule );
		_releaseFrame();
		myFrame = _getFrame( &cameraModule );
		max = count = 0;
		ptr = myFrame->dataPtr;
		if( aeLimited ) {
			for( j=aeLeft; j<aeLeft+aeWidth; j++ ){
				for( k=aeTop; k<aeTop+aeHeight; k++ ) {
					i = k*cameraModule.x + j;
					if( ptr[i] == 255 ) count++;
					if( ptr[i] > max ) max = ptr[i];
				}
			}
		}
		else {
			for( i=0; i<size; i++ ) {
				if( *ptr == 255 ) count++;
				if( *ptr > max ) max = *ptr;
				ptr++;
			}
		}
		
		_releaseFrame();
		
		if( verbose & VERB_AUTO ) {
			KIDID;
			printf("cam %d clip count: %d maxval: %d shutter: %d\n", 
					cameraModule.cameraNumber, 
					count, 
					max, 
					shutter );
		}
		
		if( (count > percent) && (increment<0) ) {
			increment = 1;
		}
		
		if( (count<=percent) && (increment>0) ) break;
		
	}
	
	status = dc1394_stop_iso_transmission( cameraData.handle, 
										cameraData.camera.node );
					
	if( DC1394_SUCCESS != status ) {
		KIDID;
		printf("%s: failed to stop iso!!\n", module );
	}

	if( verbose & VERB_AUTO ) {
		KIDID;
		printf("cam %d using shutter of %d\n", 
				cameraModule.cameraNumber, shutter );
	}
	
}
