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

struct _pixlist_ {
	unsigned long pixel;
	unsigned long type;
};

struct _diskdata_ {
	int fd;
	unsigned long size;
	char name[128];
	long pixelCount;         /* number of pixels to load */
	struct _pixlist_ *pPtr;     /* pointer into pixlist */
	struct _pixlist_ *pixlist;
	unsigned char *pixOutLine; /* place to compose output */
	char aoiFilename[256]; 
	unsigned char stackDeBayer;
	unsigned long count;
	unsigned long MSC;
	double begin;
	pid_t requestor;
	int skip;
	int msgBack;  /* msg id for response to 'add' command */
	int pixDone;  /* flag to say pixlist is done */
	unsigned long *sumPtr, *sumsqPtr;
	unsigned char *snapPtr, *brightPtr, *darkPtr;
	unsigned long nFrames; /* saved count for timex/var */
} ;

struct _imageHeader_ {
	struct timeval when;
	double	shutter;
	double	gain;
	unsigned long encoding;   /* is data compressed, etc? */
	unsigned long length;     /* length of data */
} ;

void stackInit();
void stackWrite();
void newStackInit();
void newStackWrite();

