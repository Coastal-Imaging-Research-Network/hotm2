/*

	linkedCamera.c == a "dummy function" to create a shareable
	library that has an _init entry point, the sole purpose of
	which is to call "cameraInit()".

	This is a KLUDGE HACK WORKAROUND currently needed only for
	the Pt Grey Spinnaker camera library because it cannot be
	called from within a loadable library. Therefore, I have to
	hard link the gigeSpin.o object into my hotm executable. Then
	I have to SOMEhow call the cameraInit() function in that module.

	This is it.

	Worse than I thought. I cannot call the camera init function from
	a loadable library, even if all the Spinnaker functions are linked
	into the executable directly. I must SET the init address in the 
	hotm struct (which is visible because it is linked in and not a
	.so) and then call it from the loadCameraModule function when I
	get done loading this one. Sigh

*/

#include <stdio.h>
#include "hotm.h"

void cameraInit();

void 
_init()
{

	cameraModule.initCamera = cameraInit;

}
