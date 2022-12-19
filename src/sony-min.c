/*
	sony-min.c -- the camera module for the Sony color cameras
	
	minimized -- strip out all the settings so we can see how
	it is working.

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
 *  $Log: sony-min.c,v $
 *  Revision 1.2  2004/12/07 19:23:33  stanley
 *  fix format7 setup
 *
 *  Revision 1.1  2004/12/06 21:52:25  stanley
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

static const char rcsid[] = "$Id: sony-min.c 172 2016-03-25 21:03:51Z stanley $";

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
	cameraModule.intTime = 
		shutterToTime( typeData.name, cameraModule.shutter );
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

void _setCamera( char *feature, char *value )
{
long ltemp;
long status;
double dtemp;
int i;

	ltemp = atol( value );
	
	for( i=0; i<NUM_FEATURES; i++ ) {
		if( !strcasecmp( dc1394_feature_desc[i], feature ) )
			break;
	}
	if( i<NUM_FEATURES ) {
		status = dc1394_set_feature_value( cameraData.handle,
					cameraData.camera.node,
					i+FEATURE_MIN,
					ltemp );
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
unsigned int ui;
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
    
	cameraData.mode = MODE_FORMAT7_0;
	cameraData.format = FORMAT_SCALABLE_IMAGE_SIZE;
	cameraModule.format = formatYUV422;
	
	if( verbose & VERB_FORMAT ) {
		KIDID;
		printf("cam %d data format %d\n", 
			cameraModule.cameraNumber,
			cameraModule.format );
	}
		
	cameraData.frameRate = FRAMERATE_7_5;
	cameraModule.rate = cameraData.frameRate;
	
	cameraModule.top = 
	cameraModule.left = 0;
	cameraModule.x = typeData.maxX;
	cameraModule.y = typeData.maxY;
	
	/* default camera speed to 400 */
	cameraData.speed = 400;
	getLongParam( &cameraData.speed, "cameraspeed", module);

	cameraModule.startCamera = _startCamera;
	cameraModule.stopCamera = _stopCamera;
	cameraModule.getFrame = _getFrame;
	cameraModule.releaseFrame = _releaseFrame;		
	cameraModule.changeGain = _changeGain;
	cameraModule.changeShutter = _changeShutter;
	cameraModule.setCamera = _setCamera;
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
    
	status = dc1394_dma_setup_format7_capture (
			cameraData.handle,
			cameraData.myNode,
			cameraModule.cameraNumber,
			cameraData.mode,
			cameraData.speed==400? SPEED_400 : 
			(cameraData.speed==200? SPEED_200 : SPEED_100 ),
			/*QUERY_FROM_CAMERA,*/
			320,
			cameraModule.left,
			cameraModule.top,
			cameraModule.x,
			cameraModule.y,
			3, 0, "/dev/video1394/0",
			&cameraData.camera );

	{ unsigned int minb, maxb;
	status = dc1394_set_format7_byte_per_packet( 
			cameraData.handle,
			cameraData.myNode,
			MODE_FORMAT7_2,
			384 );

	status = dc1394_query_format7_packet_para( 
			cameraData.handle,
			cameraData.myNode,
			MODE_FORMAT7_2,
			&minb, &maxb );
	printf("camera says min is %d max is %d\n", minb, maxb );
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
	printf("quads/packet: %d\n", cameraData.camera.quadlets_per_packet);
	printf("quads/frame: %d\n", cameraData.camera.quadlets_per_frame);
    
	status = dc1394_set_trigger_mode(cameraData.handle,
					cameraData.camera.node,
					TRIGGER_MODE_0 ); 
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

	cameraModule.x = cameraData.camera.frame_width;
	cameraModule.y = cameraData.camera.frame_height;
	cameraModule.cameraWidth = typeData.maxX;
	cameraModule.cameraHeight = typeData.maxY;
	
	cameraModule.shutter =
		cameraData.features.feature[FEATURE_SHUTTER-FEATURE_MIN].value;
	cameraModule.intTime = 
		shutterToTime( typeData.name, cameraModule.shutter );
	cameraModule.gain = 
		cameraData.features.feature[FEATURE_GAIN-FEATURE_MIN].value;
		
	printf("%s: shutter: %d gain: %d integrate: %lf secs\n", 
		module, 
		cameraModule.shutter,
		cameraData.features.feature[FEATURE_GAIN-FEATURE_MIN].value,
		cameraModule.intTime );
}
						

double shutterToTime( char *name, long exp )
{

				
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

