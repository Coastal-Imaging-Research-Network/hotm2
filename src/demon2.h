/* demon.h  */

/*  
 * the 'demon' with embedded image product production. no temp file */
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


#include <time.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>

#include "hotm.h"
#include "camera.h"

#define VERBOSE(x) if(me->verbose & x)
#define VERB_INIT 1
#define VERB_PROCESS 2
#define VERB_SAVE 4

#define CONCURRENT_ACTIONS 10

#define RUNNINGK (3.0)

static double frameInterval;
static struct timeval lastFrameSeen;
static double previous;

#define freeAndClear(x)  if(x) { free(x); x = NULL; }

#define TEST16BIT  \
     if( (cameraModule.format == formatMONO16)\
        || (cameraModule.format == formatRGB16) )\
                is16bit = 1;\
     else is16bit = 0

int is16bit;
int writePNG;

struct _pixlist_ {
	unsigned long pixel;
	unsigned long type;
};

struct _diskdata_ {
	int fd;
	uint32_t size;	/* number of bytes per frame */
	uint32_t numPix;	/* number of PIXELS per frame */
	char name[128];
	int32_t  pixelCount;         /* number of pixels to load */
	struct _pixlist_ *pPtr;     /* pointer into pixlist */
	struct _pixlist_ *pixlist;
	unsigned char *pixOutLine; /* place to compose output */
	char aoiFilename[256]; 
	unsigned char stackDeBayer;
	uint32_t count;
	uint32_t MSC;
	double begin;
	pid_t requestor;
	uint32_t skip;
	uint32_t msgBack;  /* msg id for response to 'add' command */
	uint32_t pixDone;  /* flag to say pixlist is done */
	uint32_t      *sumPtr;
	uint64_t      *sumsqPtr;
	uint16_t      *snapPtr, *brightPtr, *darkPtr;
	double        *runDarkPtr;
	uint32_t      nFrames; /* saved count for timex/var */
	double frameInterval;
	double lastFrameProcessedTime; 
} ;

struct smalltimeval {
	uint32_t tv_sec;
	uint32_t tv_usec;
};

struct _imageHeader_ {
	struct smalltimeval when;
	double	shutter;
	double	gain;
	uint32_t  encoding;   /* is data compressed, etc? */
	uint32_t  length;     /* length of data */
} ;

void stackInit();
void stackWrite();
void newStackInit();
void newStackWrite();
int demonPostProc();
int demonPostDump();

