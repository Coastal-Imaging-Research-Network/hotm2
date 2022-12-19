/*
	sony.c -- the camera module for the Sony color cameras
	
	There are just too many 'gotcha's for this camera to be
	able to use the default.camera module productively. 
	
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
 *  $Log: sony3.c,v $
 *  Revision 1.1  2004/12/06 21:52:09  stanley
 *  Initial revision
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

static const char rcsid[] = "$Id: sony3.c 172 2016-03-25 21:03:51Z stanley $";

#include "hotm.h"
#include "alarm.h"
#include <libraw1394/raw1394.h>
#include <libdc1394/dc1394_control.h>

#define CAMERAMODULE
#include "camera.h"

void mapCameraNameToData();

double shutterToTime();
long timeToShutter( char *, double);

void _startCamera(struct _cameraModule_ *cam)
{
int err;
	if( cam->verbose & VERB_START ) { 
		KIDID;
		printf("cam %d ", cam->cameraNumber );
		printf("start camera called, module \"%s\" \n", cam->module);
	}

	defaultAlarm( "startCamera start iso", cam->module );	
	err = dc1394_start_iso_transmission( cameraData.handle, 
						cameraData.camera.node );
	clearAlarm();
						
	if( DC1394_SUCCESS != err ) {
		KIDID;
		printf("startCamera: failed to start\n");
		exit(-1);
	}
	
	cam->frameCount = 0;
	cam->sumDiff = 0.0;
	cameraData.lastFrame.tv_sec = 
		cameraData.lastFrame.tv_usec = 0;
	
}

void _stopCamera(struct _cameraModule_ *cam)
{
int err;

	if( cam->verbose & VERB_STOP ) {
		KIDID;
		printf("cam %d stop camera called\n", cameraModule.cameraNumber );
	}
			
	defaultAlarm( "stopCamera stop iso", cam->module );				
	err = dc1394_stop_iso_transmission( cameraData.handle, 
					cameraData.camera.node );
	clearAlarm();
					
	if( DC1394_SUCCESS != err ) {
		KIDID;
		printf("stopCamera: failed to stop iso!!\n" );
	}
	
	defaultAlarm( "stopCamera release camera", cam->module );				
	/* get features back, so we can see what gain/shutter is now */
	err = dc1394_get_camera_feature_set( cameraData.handle,
    						cameraData.camera.node,
						    &cameraData.features );

	err = dc1394_dma_release_camera( cameraData.handle, &cameraData.camera );
	clearAlarm();
	
	cameraModule.shutter =
		cameraData.features.feature[FEATURE_SHUTTER-FEATURE_MIN].value;
    cameraModule.intTime = shutterToTime( typeData.name, cameraModule.shutter );
	cameraModule.gain = 
		cameraData.features.feature[FEATURE_GAIN-FEATURE_MIN].value;

	if( DC1394_SUCCESS != err ) {
		KIDID;
		printf("stopCamera: failed to release camera!!\n" );
	}
	dc1394_destroy_handle( cameraData.handle );

}

struct _frame_ *_getFrame(struct _cameraModule_ *cam)
{
int err;
double diff;

	cameraData.testFrame.status = 0;
	/*if( cam->verbose & VERB_GETFRAME ) {
		KIDID;
		printf("cam %d getFrame ... \n", cam->cameraNumber );
		fflush(NULL);
	}*/
	
	defaultAlarm( "getFrame single capture", cam->module );					
	err = dc1394_dma_single_capture( &cameraData.camera );
	clearAlarm();
	
	if( DC1394_SUCCESS != err ) {
		KIDID;
		printf("getFrame: died with err on dma get\n");
		exit(-1);
	}
	cameraData.testFrame.dataPtr = 
		(unsigned char *)cameraData.camera.capture_buffer;
	cameraData.testFrame.when = cameraData.camera.filltime;
		
	diff = cameraData.camera.filltime.tv_sec - cameraData.lastFrame.tv_sec;
    diff += (cameraData.camera.filltime.tv_usec - cameraData.lastFrame.tv_usec)
                                / 1000000.0;
	
	/* skip when difference is too long -- bogus number! */
	if( diff < 1000.0 ) {
		cam->sumDiff += diff;
		cam->frameCount++;
	}
	
	if( cam->verbose & VERB_GETFRAME ) {
		KIDID;
		printf("cam %d ", cam->cameraNumber );
		printf("getframe %d at %d.%06d ", 
			cam->frameCount,
			cameraData.camera.filltime.tv_sec,
			cameraData.camera.filltime.tv_usec );
            
        printf(" diff %.4f\n", diff );
	}

	cameraData.lastFrame = cameraData.camera.filltime;
						
	return &cameraData.testFrame;
	
}

void _releaseFrame( )
{
int status;

	defaultAlarm( "releaseFrame done with buffer", "sony module" );
				
	status = dc1394_dma_done_with_buffer( &cameraData.camera );
	clearAlarm();

	if( DC1394_SUCCESS != status ) {
		KIDID;
		printf("releaseFrame: failed to release %d\n",
			cameraData.camera.dma_last_buffer );
	}

}

/* change the camera gain by some amount */
void _changeGain( double ratio )
{
int status;
long gain;

	/* empircal reading of manual indicates that steps of gain */
	/* are 0.1dB */
	gain = cameraModule.gain;
	gain = gain + ratio * 10;
	
	/* special case -- if gain is greater than 100 (arbitrary cutoff) */
	/* then this is an absolute value to send to the camera */
	if( ratio > 100 ) gain = ratio;
	
	if( gain < 0x800 ) gain = 0x800;
	if( gain > 0x8b4 ) gain = 0x8b4;
	
	status = dc1394_set_feature_value( cameraData.handle,
							cameraData.camera.node,
							FEATURE_GAIN,
							gain );
	cameraModule.gain = gain;
}

/* change the integration time by a fraction */
void _changeShutter( double ratio )
{
int status;
long shutter;
double intTime;

	intTime = cameraModule.intTime;
	intTime = intTime * ratio;
	shutter = timeToShutter( typeData.name, intTime );
	/* if the ratio is small enough that the time doesn't */
	/* change, step the shutter by at least 1 unit anyway */
	if( shutter == cameraModule.shutter ) {
		shutter = cameraModule.shutter + ((ratio>1.0)? -1 : 1);
	}
	
	/* special case, if ratio is > 1000, it is a shutter setting */
	if( ratio > 1000 ) shutter = ratio;
	
	if( shutter > 0xb20 ) shutter = 0xb20;
	if( shutter < 0x7e2 ) shutter = 0x7e2;
	status = dc1394_set_feature_value( cameraData.handle,
							cameraData.camera.node,
							FEATURE_SHUTTER,
							shutter );
	cameraModule.shutter = shutter;
	cameraModule.intTime = shutterToTime( typeData.name, shutter );
}
	

void _init()
{
struct 	_commandFileEntry_ *ptr;
char 	module[] = "sony";
long	size;
int 	numCameras;
int 	i;
int 	j;
int		foundCamera = 0;
char	line[128];
quadlet_t	value[2];
char	*tmp;
int		debug;
int 	verbose;
int		status;
long	myValue;
int 	portNode;
dc1394bool_t yesNo;
unsigned int B, R;
	    
	strcpy( cameraModule.module, module );
	
	/* init cameraData struct to all 0 */
	memset( &cameraData, 0, sizeof(cameraData) );
	

	/* find out how verbose I am supposed to be */
	getLongParam( &cameraModule.verbose, "cameraverbose", module );
	verbose = cameraModule.verbose;	

	/* get my camera number */
	getLongParam( &cameraModule.cameraNumber, "cameraNumber", module );
	if( hotmParams.cameraNumber > -1 )
		cameraModule.cameraNumber = hotmParams.cameraNumber;
	
	/* look up the camera number to UID mapping from the file */
	tmp = mapCameraToUID( cameraModule.cameraNumber );
	if( NULL == tmp ) {
		printf("%s: cannot map number %d to UID. Giving up.\n",
			module, 
			cameraModule.cameraNumber );
		exit(-1);
	}
	strcpy( cameraData.UID, tmp );

	/* get port and node on bus, side effect is getting camera info */
    portNode = mapUIDToPortAndNode( cameraData.UID, 
								verbose & VERB_MAPPING,
								&cameraData.info );
    cameraData.myNode = portNode & 0xff;
	
	/* report raw camera data, if verbosity */
	if( verbose & VERB_CAMERA ) {
		KIDID;
		dc1394_print_camera_info( &cameraData.info );
	}
	
	/* figure out what type of camera we have */
	mapCameraNameToData();
	if( typeData.maxX == 0 ) {
		printf("%s: unknown camera model %s, aborting...\n",
				module, cameraData.info.model );
		exit(-2);
	}
	/* keep track of camera name for later */
	strcpy( cameraModule.module, typeData.name );
    
	get1394StringParam( &cameraData.mode, 	"cameramode", module );
	get1394StringParam( &cameraData.format, "cameraFormat", module );
	cameraModule.format = modeToFormat( getString(findCommand("cameraMode")) );
	
	/* check for unset mode and format */
	if( cameraData.mode == 0 ) {
		cameraData.mode = dc1394Decode( typeData.defaultMode );
		cameraModule.format = modeToFormat( typeData.defaultMode );
	}
	if( cameraData.format == 0 ) 
		cameraData.format = dc1394Decode( typeData.defaultFormat );

	if( verbose & VERB_FORMAT ) {
		KIDID;
		printf("cam %d data format %d\n", 
			cameraModule.cameraNumber,
			cameraModule.format );
	}
		
	get1394StringParam( &cameraData.frameRate, "cameraFrameRate", module );
	if( cameraData.frameRate == 0 )
		cameraData.frameRate = dc1394Decode( typeData.defaultFramerate );
	cameraModule.rate = cameraData.frameRate;
	
	cameraModule.top = 
    cameraModule.left = 0;
	cameraModule.x = 
	cameraModule.y = 0;
	
	getLongParam( &cameraModule.top,  "cameraTop", module );
	getLongParam( &cameraModule.left, "cameraLeft", module );
	getLongParam( &cameraModule.x, "camerax", module );
	getLongParam( &cameraModule.y, "cameray", module );
	
	/* special process for Sony format 7 */
	/* left must be multiple of some value */
	j = typeData.maxX / typeData.unitsPerX;
	i = (cameraModule.left/j) * j;
	if( i != cameraModule.left ) {
		KIDID;
		printf("cam %d changed left from %d to %d\n",
				cameraModule.cameraNumber, 
				cameraModule.left, 
				i );
		cameraModule.left = i;
	}
	/* width must be multiple of minXwidth */
	i = cameraModule.x - cameraModule.x%typeData.minXwidth;
	if( i+cameraModule.left > typeData.maxX )
		i = typeData.maxX - cameraModule.left;
	if( i != cameraModule.x ) {
		KIDID;
		printf("cam %d changed x from %d to %d\n",
				cameraModule.cameraNumber, 
				cameraModule.x, 
				i);
		cameraModule.x = i;
	}
	
	/* y and top must be multiples */
	j = typeData.maxY / typeData.unitsPerY;
	i = (cameraModule.top/j) * j;
	if( i != cameraModule.top ) {
		KIDID;
		printf("cam %d changed top from %d to %d\n",
				cameraModule.cameraNumber, 
				cameraModule.top, 
				i );
		cameraModule.top = i;
	}
	i = cameraModule.y - cameraModule.y%typeData.minYheight;
	if( i+cameraModule.top > typeData.maxY ) 
		i = typeData.maxY - cameraModule.top;
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

		if((verbose & VERB_FEATURE) 
		 &&(FEATURE_UNSET != cameraData.myFeatures[i])) {
			KIDID;
			printf("cam %d %s: %s is ", 
				cameraModule.cameraNumber, 
				module, 
				line );
			if( FEATURE_AUTO == cameraData.myFeatures[i] )
				printf("AUTO\n");
			else
				printf("%d\n", cameraData.myFeatures[i] );
		} 
	}

	cameraModule.startCamera = _startCamera;
	cameraModule.stopCamera = _stopCamera;
	cameraModule.getFrame = _getFrame;
	cameraModule.releaseFrame = _releaseFrame;		
	cameraModule.changeGain = _changeGain;
	cameraModule.changeShutter = _changeShutter;
	cameraModule.initCamera = _init;
	
    /*first we need to get access to the raw1394subsystem*/
    cameraData.handle = dc1394_create_handle(portNode>>8);
    if (cameraData.handle==NULL) {
        printf("Unable to aquire a raw1394 handle\n");
        exit(1);
    }
	
    /*get the camera nodes and don't describe them as we find them*/
	defaultAlarm( "init get nodes", module );
    cameraData.cameraNodes =
    	dc1394_get_camera_nodes(cameraData.handle,&numCameras,0);
	clearAlarm();
		
	defaultAlarm("init init_camera", module );
	status = dc1394_init_camera( cameraData.handle, cameraData.myNode );
	clearAlarm();
	sleep(1);
	
	if( DC1394_SUCCESS != status ) {
		KIDID;
		printf("%s: cannot initialize camera", module );
		exit(-1);
	}
    
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
					10, /* number of buffers */
					0,  /* dropframes? */
					0,  /* dma device file. NULL==use default */
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
	/*dc1394_print_feature_set(&cameraData.features);*/
    
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
				B = (myValue >> 8) & 0xff;
				R = myValue & 0xff;
				status = dc1394_set_white_balance( cameraData.handle,
							cameraData.camera.node,
							B, R );
				printf("white balance: U: %d V: %d\n", B, R );
				if( DC1394_SUCCESS != status )
					printf("%s: cannot set white balance!\n",
							module );
			}
			else if( i == (FEATURE_TRIGGER - FEATURE_MIN) ) {
				/* trigger needs special coding */
				/* YES, skip this for now! It defeats auto gain/exposure */
				/* and white balance. So, we must set this later! */
#ifdef never
				status = dc1394_set_trigger_mode(cameraData.handle,
							cameraData.camera.node,
							myValue ); 
				status = dc1394_set_trigger_on_off( cameraData.handle,
							cameraData.camera.node,
							1 );
				if( DC1394_SUCCESS != status )
					printf("%s: cannot set trigger!\n",
							module );
#endif
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
	}

/* at this point, we've set almost all the features we want set.
 * major exception: TRIGGER We must allow auto exposure to run for
 * awhile if we do use external trigger AND we are using auto exposure.
 * Otherwise, it never happens. The auto part, that is.
 */
 
 	{
		long i1, i2, i3, trigMode;
		int isAuto, isTrigger;
		long autoSleep = 15;
		unsigned int wb, wr;
 		getLongParam( &i1, "cameragain", "" );
		getLongParam( &i2, "camerashutter", "" );
		getLongParam( &i3, "camerawhite balance", "" );
		
		trigMode = -1;
		get1394StringParam( &trigMode, "cameratrigger", "" );
		isTrigger = ( trigMode == -1 ) ? 0 : 1;
	
		isAuto = ((FEATURE_AUTO == i1) 
			   || (FEATURE_AUTO == i2) 
			   || (FEATURE_AUTO	== i3));
	
		if( isAuto && isTrigger ) {
			
			getLongParam( &autoSleep, "cameraAutoSleep", module );
			
			/* must do a few frames to let auto stabilize */
			if( verbose & VERB_AUTO ) {
				KIDID;
				printf("cam %d starting autoexposure run, %d seconds\n",
					cameraModule.cameraNumber,
					autoSleep );
			}
			_startCamera( &cameraModule );
			/*sleep(autoSleep);
			
			defaultAlarm( "autoexposure stop iso", module );				
			status = dc1394_stop_iso_transmission( cameraData.handle, 
						cameraData.camera.node );
			clearAlarm();			
				
			defaultAlarm( "autoexposure clear buffs", module ); 
			for( i=0; i<cameraData.camera.num_dma_buffers; i++ ) { */
			for( i=0; i<autoSleep; i++ ) {
				_getFrame( &cameraModule );
				_releaseFrame();
				/*printf("release a frame autoexposure mode\n" );*/
			}
			status = dc1394_stop_iso_transmission( cameraData.handle, 
						cameraData.camera.node );
			clearAlarm(); 
			
			/* fix the shutter and gain at these values */
			defaultAlarm( "autoexposure fix values", module );					
			if( FEATURE_AUTO == i1 ) {
				dc1394_get_feature_value( cameraData.handle,
							cameraData.camera.node,
							FEATURE_GAIN,
							&i );
				dc1394_auto_on_off( cameraData.handle,
							cameraData.camera.node,
							FEATURE_GAIN,
							0 );
				dc1394_set_feature_value( cameraData.handle,
							cameraData.camera.node,
							FEATURE_GAIN,
							i );
			}
			if( FEATURE_AUTO == i2 ) {
				dc1394_get_feature_value( cameraData.handle,
							cameraData.camera.node,
							FEATURE_SHUTTER,
							&i );
				dc1394_auto_on_off( cameraData.handle,
							cameraData.camera.node,
							FEATURE_SHUTTER,
							0 );
				dc1394_set_feature_value( cameraData.handle,
							cameraData.camera.node,
							FEATURE_SHUTTER,
							i );
			}
			if( FEATURE_AUTO == i3 ) {
				dc1394_get_white_balance( cameraData.handle,
							cameraData.camera.node,
							&wb, &wr );
				dc1394_auto_on_off( cameraData.handle,
							cameraData.camera.node,
							FEATURE_WHITE_BALANCE,
							0 );
				dc1394_set_white_balance( cameraData.handle,
							cameraData.camera.node,
							wb, wr );			
			}
			clearAlarm();
		}
		
		if( isTrigger ) {

			/* now set trigger */
			defaultAlarm( "autoexposure set trigger", module );					
			status = dc1394_set_trigger_mode(cameraData.handle,
							cameraData.camera.node,
							trigMode ); 
			if( DC1394_SUCCESS != status ) {
				printf("%s: cannot set trigger mode!\n",	module );
				exit( -2 );
			}
			status = dc1394_set_trigger_on_off( cameraData.handle,
							cameraData.camera.node,
							1 );
			clearAlarm();
			if( DC1394_SUCCESS != status ) {
				printf("%s: cannot set trigger!\n",	module );
				exit( -2 );
			}
		}
	}
				

	/* lets get all values back before we report */
	defaultAlarm( "get features second time", module );
	status = dc1394_get_camera_feature_set( cameraData.handle,
    						cameraData.camera.node,
						    &cameraData.features );
    if( DC1394_SUCCESS != status ) {
    	printf("%s: unable to get all features back?\n",
					module );
		exit(-1);
    }
	clearAlarm();
		
	if( verbose & VERB_FEATURE ) {
			for( i=0; i<NUM_FEATURES; i++ ) {
    			printf("cam %d feature %s current setting ", 
					cameraModule.cameraNumber, dc1394_feature_desc[i] );
				if( cameraData.features.feature[i].auto_active ) 
					printf("AUTO ");
			
				if( i == (FEATURE_WHITE_BALANCE - FEATURE_MIN) ) {
					/* white balance needs special coding */
					status = dc1394_get_white_balance( cameraData.handle,
							cameraData.camera.node,
							&B, &R );
					myValue = ((B & 0xff) << 8) | (R & 0xff);
					printf("B: %d R: %d Coded: %d\n", B, R, myValue );
				}
				else
					printf("value %d\n", cameraData.features.feature[i].value);	
		}
	}
		
    cameraModule.x = cameraData.camera.frame_width;
    cameraModule.y = cameraData.camera.frame_height;
	cameraModule.cameraWidth = typeData.maxX;
	cameraModule.cameraHeight = typeData.maxY;
	
	cameraModule.shutter =
		cameraData.features.feature[FEATURE_SHUTTER-FEATURE_MIN].value;
    cameraModule.intTime = shutterToTime( typeData.name, cameraModule.shutter );
	cameraModule.gain = 
		cameraData.features.feature[FEATURE_GAIN-FEATURE_MIN].value;
		
	if( verbose & VERB_AUTO ) {
		KIDID;
		printf("%s: shutter: %d gain: %d integrate: %lf secs\n", 
				module, 
				cameraModule.shutter,
				cameraData.features.feature[FEATURE_GAIN-FEATURE_MIN].value,
				cameraModule.intTime );
	}
						
}

double shutterToTime( char *name, long exp )
{

//	printf("shutterToTime: with name %s shutter %d\n",
//				name, exp );
				
	if( !strncmp( name, "DFW-SX900", strlen("DFW-SX900") ) ) {

		if( exp == 2047 ) 
			return 1.0/7.5;
		else if( exp == 0xc2c ) 
			return 1.0/10000.0;
		else if( exp == 0xc2d ) 
			return 1.0/20000.0;
		else if( exp < 0x7f1 ) 
			return shutterToTime( name, 0x7f1 );
		else if( exp > 0xc2d )
			return shutterToTime( name, 0xc2d );
		else if( exp > 2047 )
			return (3116.4 - exp) / 7999.0;
		else if( exp < 2047 )
			return (2048 - exp) / 7.5;
		else
			return -1;
	}
	else if( !strncmp( name, "DFW-X700", strlen("DFW-X700") ) ) {

		if( exp == 2047 ) 
			return 1.0/15.0;
		else if( exp == 0xb20 ) 
			return 1.0/20000.0;
		else if( exp < 0x7e2 ) 
			return shutterToTime( name, 0x7e2 );
		else if( exp > 0xb20 )
			return shutterToTime( name, 0xb20 );
		else if( exp > 2047 )
			return (2848.4 - exp) / 11962.0;
		else if( exp < 2047 )
			return (2048.0 - exp) / 15.0;
		else
			return -1;
	}
	else {
			return -1;
	}
}

long timeToShutter( char *name, double tme )
{

long temp;

	if( !strncmp( name, "DFW-SX900", strlen("DFW-SX900") ) ) {
	
		if( tme >= 1.0/7.5 ) {
			temp = 0x800 - 7.5 * tme;
		}
		else if( tme < 1.5/20000.0 ) {
			temp = 0xc2d;
		}
		else if( tme < 1.0/5882.0 ) {
			temp = 0xc2c;
		}
		else
			temp = 3116.4 -  7999.0 * tme;
			
	}
	
	else if( !strncmp( name, "DFW-X700", strlen("DFW-X700") ) ) {
	
		if( tme >= 1.0/15.0 ) {
			temp = 0x800 - 15 * tme;
		}
		else if( tme < 1.0/10000.0 ) {
			temp = 0xb20;
		}
		else
		 	temp = 2848.4 - 11962.0 * tme;
			
	}
	else {
		temp = 0;
	}
		
	/* printf("timeToShutter: time: %f shutter: %d (0x%04x)\n", tme, temp, temp); */
	return temp;
}

