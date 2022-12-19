/* mapUIDtoPortAndNode -- version 2 of library, simply find all the 
cameras and then find the one we want. fills in the cameraData.camera
structure for that camera so we can use it later. */
	 
/* 
 *  $Log: mapUIDToPortAndNode.c,v $
 *  Revision 1.5  2011/11/10 00:54:45  root
 *  changed for dc1394 v2
 *
 *  Revision 1.4  2002/11/05 22:20:00  stanley
 *  return the camera info block, it's needed later
 *
 *  Revision 1.3  2002/10/03 23:39:31  stanley
 *  changed to case insensitive compare for UID
 *
 *  Revision 1.2  2002/08/08 23:28:20  stanleyl
 *  parameterize verbose flag
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

static const char rcsid[] = "$Id: mapUIDToPortAndNode.c 358 2018-08-15 23:44:08Z stanley $";

#include "hotm.h"
#include "camera.h"
#include <stdlib.h>

int mapUIDToPortAndNode( char *UID, int VERBOSE )
{

int 			numPorts;
int 			numCameras;
int 			i, j;
char 			module[] = "mapUIDToPort";
char 			line[128];
uint64_t                uidNum;
dc1394error_t           status;

	/* first get how many ports we have */
	status = dc1394_camera_enumerate( cameraData.dc1394, &cameraData.cameraList );
        DC1394_ERR_RTN( status, "No cameras on the bus?" );
        numPorts = cameraData.cameraList->num;
        uidNum = strtoull(UID, NULL, 16);
        
        if(HVERBOSE(HVERB_UIDMAP) ) {
          printf("%s: system has %d cameras\n", module, numPorts );
          printf("%s: looking for 0x%016lx (%s)\n", module, uidNum, UID );
          for( i=0; i<numPorts; i++ ) {
             printf("%s:        uid: 0x%016lx  unit: %d\n", 
                     module, 
                     cameraData.cameraList->ids[i].guid,
                     cameraData.cameraList->ids[i].unit );
             }
        }

        cameraData.camera = dc1394_camera_new( cameraData.dc1394, uidNum );
        if( cameraData.camera == NULL ) {
	    printf("%s: no camera   %s found on any port at any node!\n",
		    module, UID );
	    exit(-1);
        }
        
#ifdef NEVER
        /* in mainline camera module */
        if(HVERB_UIDMAP) {
          dc1394_camera_print_info( cameraData.camera, stdout );
          printf("\n");
        }
#endif
}
