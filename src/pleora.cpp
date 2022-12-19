
extern "C"
{

#include "hotm.h"
#define CAMERAMODULE
#include "camera.h"

}

#include <PvSampleUtils.h>
#include <PvSystem.h>
#include <PvInterface.h>
#include <PvStream.h>
#include <PvDevice.h>
#include <PvPipeline.h>
#include <PvDeviceFinderWnd.h>
#include <PvPixelType.h>
#include <PvBufferWriter.h>


     static PvDevice lDevice;
     static PvGenParameterArray *lDeviceParams = NULL;

void
_startCamera(struct _cameraModule_ *cam)
{

	printf("starting camera! number= %ld\n",
		cam->cameraNumber);

     // The pipeline is already "armed", we just have to tell the device
     // to start sending us images
     lDeviceParams->ExecuteCommand ("AcquisitionStart");

	printf("ending startCamera\n");

}

void
_myInit ()
{

     char lDeviceAddress[1024];
     char lMulticastAddress[1024];
     char lLocalAddress[1024];
     char module[128];

     sprintf(module,"pleora");
     memset (lLocalAddress, 0, 1024);
     sprintf (lMulticastAddress, "239.192.1.1");
     memset (lDeviceAddress, 0, 1024);
     sprintf (lDeviceAddress, "169.254.32.18");

     PvUInt32 lChannel = 0;
     PvUInt16 lHostPort = 1042;
     PvResult lResult;

     PvStream lStream;

	printf("entering _init...\n");

     lResult =
          lStream.Open (lDeviceAddress, lHostPort, lChannel, lLocalAddress);

     if (!lResult.IsOK ()) {
          printf ("Failed opening the incoming stream: %s\n",
                  lResult.GetDescription ().GetAscii ());
          return;
     }

     PvPipeline lPipeline (&lStream);

     PvDevice lDevice;
     lResult = lDevice.Connect (lDeviceAddress);
     if (!lResult.IsOK ()) {
          printf
               ("Failed connecting to the device to set its destination and initiate an AcquisitionStart: %s\n",
                lResult.GetDescription ().GetAscii ());
          return;
     }
     lDevice.SetStreamDestination (lStream.GetLocalIPAddress (),
                                   lStream.GetLocalPort (), lChannel);

     // Get device parameters need to control streaming
     lDeviceParams = lDevice.GetGenParameters ();

     // Reading payload size from device. Otherwise, the pipeline may miss the first several images.
     PvInt64 lReceivePayloadSize = 0;
     lDeviceParams->GetIntegerValue ("PayloadSize", lReceivePayloadSize);

     // Set the Buffer size and the Buffer count
     lPipeline.SetBufferSize (static_cast < PvUInt32 > (lReceivePayloadSize));
     lPipeline.SetBufferCount (16);     // Increase for high frame rate without missing block IDs
     lPipeline.Start ();

     lDeviceParams->SetIntegerValue ("TLParamsLocked", 1);

     lDeviceParams->ExecuteCommand ("GevTimestampControlReset");
     lDeviceParams->ExecuteCommand ("AcquisitionStart");

     getLongParam (&cameraModule.cameraNumber, "cameraNumber", module);
     if( hotmParams.cameraNumber > -1 )
           cameraModule.cameraNumber = hotmParams.cameraNumber;


	cameraModule.startCamera = _startCamera;

	printf("leaving _init...\n");
}

extern "C" {
	void _init() { _myInit(); }
}

void * __dso_handle;
