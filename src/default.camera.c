
/*
	default.camera.c -- the default camera module
	
	loaded dynamically by hotm. contains the following functions:
	
	_init -- called by dlopen by default, must set up camera
	_startCamera -- turn on collection using previously determined
		parameters
	_stopCamera  -- turn camera off
	_getFrame    -- get the next frame please
	
*/

/* 
 *  $Log: default.camera.c,v $
 *  Revision 1.8  2002/10/30 20:18:03  stanley
 *  added info param to mapUIDToPort...
 *
 *  Revision 1.7  2002/10/04 00:17:42  stanley
 *  added diff time for frame input
 *
 *  Revision 1.6  2002/08/10 00:28:53  stanleyl
 *  removed device name, let library figure it out
 *
 *  Revision 1.5  2002/08/02 23:58:53  stanleyl
 *  change to dma_setup_capture
 *
 *  Revision 1.4  2002/04/29 16:56:24  stanley
 *  change verbose codes
 *
 *  Revision 1.3  2002/03/22 19:56:48  stanley
 *  many small changes, added cameraspeed command.
 *
 *  Revision 1.2  2002/03/18 19:59:23  stanley
 *  add format 7 top and left offsets
 *
 *  Revision 1.1  2002/03/15 21:57:00  stanley
 *  Initial revision
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

static const char rcsid[] = "$Id: default.camera.c 172 2016-03-25 21:03:51Z stanley $";

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

struct {
	int	number;
	char	UID[128];
	struct	_frame_ testFrame;
	long	mode;
	long	format;
	long	frameRate;
	long	myFeatures[NUM_FEATURES];
	long	speed; /*  100/200/400 */
	struct timeval lastFrame;
	dc1394_cameracapture camera;
	raw1394handle_t handle;
	nodeid_t *cameraNodes;
	dc1394_feature_set features;
	dc1394_camerainfo info;
	int	myNode;
} cameraData;

void _startCamera(struct _cameraModule_ *cam)
{
int err;
	if( cam->verbose & VERB_START ) { 
		printf("cam %d ", cam->cameraNumber );
		printf("start camera called, module \"%s\" \n", cam->module);
	}
	
	err = dc1394_start_iso_transmission( cameraData.handle, 
						cameraData.camera.node );
	if( DC1394_SUCCESS != err ) {
		printf("startCamera: failed to start\n");
		exit(-1);
	}
}

void _stopCamera(struct _cameraModule_ *cam)
{
int err;

	if( cam->verbose & VERB_STOP )
		printf("stop camera called\n" );
		
	err = dc1394_stop_iso_transmission( cameraData.handle, 
					cameraData.camera.node );
					
	if( DC1394_SUCCESS != err ) {
		printf("stopCamera: failed to stop iso!!\n" );
	}
	err = dc1394_dma_release_camera( cameraData.handle, &cameraData.camera );
	if( DC1394_SUCCESS != err ) {
		printf("stopCamera: failed to release camera!!\n" );
	}
	dc1394_destroy_handle( cameraData.handle );

}

struct _frame_ *_getFrame(struct _cameraModule_ *cam)
{
int err;
double diff;

	cameraData.testFrame.status = 0;
	if( cam->verbose & VERB_GETFRAME ) {
		printf("cam %d getFrame ... ", cam->cameraNumber );
		fflush(NULL);
	}
		
	err = dc1394_dma_single_capture( &cameraData.camera );

	if( DC1394_SUCCESS != err ) {
		printf("getFrame: died with err on dma get\n");
		exit(-1);
	}
	cameraData.testFrame.dataPtr = 
		(unsigned char *)cameraData.camera.capture_buffer;
	cameraData.testFrame.when = cameraData.camera.filltime;
	
	if( cam->verbose & VERB_GETFRAME ) {
		printf("at %d.%06d", 
			cameraData.camera.filltime.tv_sec,
			cameraData.camera.filltime.tv_usec );
			
		diff = cameraData.camera.filltime.tv_sec - cameraData.lastFrame.tv_sec;
		diff += (cameraData.camera.filltime.tv_usec - cameraData.lastFrame.tv_usec)
				/ 1000000.0;
		printf(" diff %.4f\n", diff );
	}
				
	cameraData.lastFrame = cameraData.camera.filltime;
			
	return &cameraData.testFrame;
	
}

void _releaseFrame( )
{
int status;

	status = dc1394_dma_done_with_buffer( &cameraData.camera );
	if( DC1394_SUCCESS != status ) {
		printf("releaseFrame: failed to release %d\n",
			cameraData.camera.dma_last_buffer );
	}

}

void _init()
{
struct 	_commandFileEntry_ *ptr;
char 	module[] = "default camera";
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
	

	/* default camera speed to 400 */
	cameraData.speed = 400;
	getLongParam( &cameraData.speed, "cameraspeed", module);

	cameraModule.format = modeToFormat( getString(findCommand("cameraMode")) );
	if( verbose & VERB_FORMAT ) 
		printf("cam %d data format %d\n", 
			cameraModule.cameraNumber,
			cameraModule.format );
	
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
					5,
					0, 0,
					&cameraData.camera );
	}
    if( DC1394_SUCCESS != status ) {
    	printf("%s: failure to setup dma capture!\n", module );
		dc1394_dma_release_camera( cameraData.handle, &cameraData.camera );
		raw1394_destroy_handle( cameraData.handle );
		exit(-1);
    }

    /* report back how big a "buffer" is */
    cameraModule.bufferLength = cameraData.camera.dma_frame_size;
    
    /* report back some data on what I did when I setup */
    if( verbose & VERB_CAMERA ) {
	    printf("cam %d on port %d node %d (%d) device %s fd %d\n", 
			cameraModule.cameraNumber,
			cameraData.camera.port, 
			cameraData.myNode, cameraData.camera.node,
			cameraData.camera.dma_device_file,
			cameraData.camera.dma_fd);
    	printf("cam %d dma buffersize: %d framesize: %d numbuffers: %d\n",
    			cameraModule.cameraNumber,
				cameraData.camera.dma_buffer_size,
				cameraData.camera.dma_frame_size,
				cameraData.camera.num_dma_buffers );
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
    
    cameraModule.x = cameraData.camera.frame_width;
    cameraModule.y = cameraData.camera.frame_height;
    		
}

