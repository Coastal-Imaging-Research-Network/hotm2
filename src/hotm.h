
/* hotm.h -- basic common data and defines for the FireWire version 
  of 'm' */

/* 
 *  $Log: hotm.h,v $
 *  Revision 1.16  2011/11/10 01:02:22  root
 *  conversion to dc1394
 *
 *  Revision 1.15  2010/06/09 20:19:49  stanley
 *  added format 7 info, rawOrder, frame xmit
 *
 *  Revision 1.14  2004/12/17 21:29:01  stanley
 *  swapped pixel RGB bytes for RGGB deBayering function! cool cludge
 *
 *  Revision 1.13  2004/12/11 01:01:40  stanley
 *  changed to real gain, real shutter values
 *  added set/getCamera pointers
 *
 *  Revision 1.12  2004/12/07 18:16:45  stanley
 *  added camera control function pointers
 *
 *  Revision 1.11  2003/05/01 20:32:37  stanley
 *  added framecount, sumdiff
 *
 *  Revision 1.10  2003/03/14 18:37:30  stanley
 *  add declarations to make intel compiler happy
 *
 *  Revision 1.9  2002/11/06 23:56:38  stanley
 *  added gain to cameramodule
 *
 *  Revision 1.8  2002/10/31 20:53:19  stanley
 *  added cameranumber to params
 *
 *  Revision 1.7  2002/10/30 20:16:02  stanley
 *  added params to mapUIDToPortAndNode and include
 *
 *  Revision 1.6  2002/08/14 21:55:23  stanley
 *  added HVERBOSE verbose flags
 *  changed type of format 7 info for camera
 *
 *  Revision 1.5  2002/03/23 00:24:08  stanley
 *  message passing code
 *
 *  Revision 1.4  2002/03/22 19:58:34  stanley
 *  added timelimit to params
 *
 *  Revision 1.3  2002/03/18 19:58:35  stanley
 *  add format7 top and left in camerastruct
 *
 *  Revision 1.2  2002/03/15 21:58:55  stanley
 *  fix rcsid
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

static const char rcsidh[] = "$Id: hotm.h,v 1.16 2011/11/10 01:02:22 root Exp $";

#ifdef _MAIN_
#define EXTERN
#else
#define EXTERN extern
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <ctype.h>
#include <netinet/in.h>

EXTERN FILE *logFD;

/*#include <dc1394/dc1394.h>*/

/* what does each pixel look like? */
struct _pixel_ {
#ifdef BAYER_RGGB
	unsigned char b,g,r;
#else
	unsigned char r,g,b;
#endif
} ;

/* how are pixels passed around as a frame? */
struct _frame_ {
        void *infoPtr;           /* a "pointer" to something */
	struct timeval when;
	unsigned char *dataPtr;
	int	status;
};

/* enum for data formats I might see, recorded in cameraModule */
#define formatMONO 16
enum {
	formatRGB8,
	formatYUV444,
	formatYUV422,
	formatRGB16,
	formatMONO8 = formatMONO,	
	formatMONO16
};
  
/* lines from the command file, read one by one, processed by */
/* whatever module wants them */

struct _commandFileEntry_ {

	/* linked list */
	struct _commandFileEntry_ *next;
	/* the actual raw line */
	char    line[256];
	/* and the module(s) it was used by */
	char	module[10240];
	
};

EXTERN struct _commandFileEntry_ *firstCommandFileLine;

/* linked list of processing modules */
/*   these are the common parameters and pointers to */
/*     functions that need to be shared publicly */
struct _processModule_ {

	/* linked list */
	struct _processModule_ *next;
	
	/* module name */
	char	module[32];
	
	/* file name I came from */
	char	library[256];
	
	/* frame count, also "active" flag */
	long	remaining;
	
	/* now list of functions required */
	/* note: a process module may need to do things for */
	/* both frames and pixels, so keep both entry points. */
	void	(*processFrame)(struct _processModule_ *, struct _frame_ *);
	void	(*closeFrame)(struct _processModule_ * );
	void	(*processPixel)(struct _processModule_ *, struct _pixel_, unsigned long );
	void	(*saveResults)(struct _processModule_ *);
	
	/* debugging flag module specific */
	int		debug;
	long	verbose;
	
	/* keep track of processing time for a frame */
	/* in microseconds from 'gettimeofday' */
	unsigned long processMicros;
	
};

EXTERN struct _processModule_ *firstProcessModule;

/* camera module */
struct _cameraModule_ {

	/* loaded from file name */
	char		library[256];
	/* what is my "camera name" */
	char		module[32];
	
	/* some common params */
	long		x;    /* image width  */
	long		y;    /* image height */
	long 		top;  /* top of image */
	long 		left; /* left edge */
	long		cameraWidth; /* full width of camera */
	long		cameraHeight; /* full height of camera */
	long		cameraNumber;
	int		debug;
	long		verbose;
	long 		rate;
	double		intTime;  /* real world exposure, if known */
	long		shutter;  /* camera shutter setting, if known */
	long		gain;	  /* camera gain if known */
	long 		frameCount; /* count frames, so average diff */
	double		sumDiff;	/* can be calculated */

	/* data format from camera */
	int		format;
	unsigned long	bufferLength;

	/* functions we need to access */
	void	(*startCamera)(struct _cameraModule_ *);
	void	(*initCamera)();
	void	(*stopCamera)(struct _cameraModule_ *);
	struct _frame_  * (*getFrame)(struct _cameraModule_ *);
	void	(*releaseFrame)();
	int 	(*setCamera)(char*, char*, double);
	int	(*getCamera)(int, double *);

	int		version; /* version of hotm that made this */
	double		realGain; /* real world gain, in dB */
	char		rawOrder[5];

	long		maxX;
	long		maxY;
	/* xmitMicros is packet/frame * 125 micros/packet */
	unsigned long	xmitMicros;

	long 	tsource; /* trigger source */
	long 	interval; /* software trigger interval */

	double  maxGain; /* used to be 23 fixed for 1394, other pt grey */
			/* now can be 43 for BFS and newer */
	
} ;

EXTERN struct _cameraModule_ cameraModule;

/* a struct to hold global parameters */
EXTERN struct {
	int		debug;
	int 	verbose;
	char	commandFile[256];
	char	myName[256];
	char	sitename[32];
	char	programVersion[32];
	long	discard;
	long	skip;
	unsigned long baseEpoch;
	long	timeLimit;
	char    baseName[64];
	int 	mqid;
	int 	kidid;
	int		cameraNumber;
	int	writePNG;
} hotmParams;

#define KIDID if(hotmParams.kidid >=0 ) printf("kid %d: ", hotmParams.kidid ); 
#define HVERBOSE(x) (hotmParams.verbose & x)

/* define verbosity flags -- main routines are HVERB_... */
#define HVERB_COMMANDLINE	0x0001
#define HVERB_CAMERA 		0x0002
#define HVERB_MOMMA			0x0004
#define HVERB_UIDMAP		0x0008
#define HVERB_ALLCOMMANDS	0x0010
#define HVERB_COMMANDFILE	0x0020
#define HVERB_PROCESSIN		0x0040
#define HVERB_PROCESSTIMES	0x0080
#define HVERB_PROCESSOUT	0x0100
#define HVERB_MEMORY		0x0200

#define HVERB_FINDCOMMAND	0x10000000
#define HVERB_LISTCOMMANDS	0x20000000


struct _commandFileEntry_ *findCommand( char * );
struct _commandFileEntry_ *findCommandNext( char *, 
		struct _commandFileEntry_ *);
int getBoolean( struct _commandFileEntry_ * );
char *getString( struct _commandFileEntry_ * );
long getLong( struct _commandFileEntry_ * );
double getDouble( struct _commandFileEntry_ * );
unsigned long *memoryLock( unsigned long, char*, char* );
void getBooleanParam( int *, char *, char *);
void getLongParam( long *, char *, char *);
void getDoubleParam( double *, char *, char *);
void getStringParam( char *, char *, char *);
char *mapCameraToUID( long );
int modeToFormat( char * );
double framerateToDouble( long );
void loadCommandFile(void);
void appendList();
void tagCommand( struct _commandFileEntry_ *, char * );
long dc1394Decode( char * );
void whateverToRGB( struct _pixel_ *, unsigned char * );
void parseCommandLine( int, char ** );
void commonCommand();
void loadTimer();
void loadCameraModule();
void loadProcessModules();
void dumpCommandFile( int );
void processLoop();
void saveResults();
void get1394StringParam( long *, char *, char * );
void insertText( int, struct _pixel_ *, char * );
void save24( struct _pixel_ *, char *, int );
void savePNG( struct _pixel_ *, char *, int );
void defaultAlarm( char *, char * );
void mapCameraNameToData( int, char * );

/*
 *  Definitions for message passing stuff between hotmomma and the kids
 */

#define MOMMA	-1

struct _msgbuf_ {
	long    channel;    /* message type is sender and receiver ids */
	int     state;      /* the actual message */
};

enum {
	stateReady,
	stateStart,
	stateDone,
	stateFinish,
	stateExit
};

int 	AllocMessageQueue   (void);
void 	FreeMessageQueue    (int);
int	ExpectState 	    (int, int, int, int);
void	AssertState  	    (int, int, int, int);

#include <time.h>
#define MESLEEP(x) { \
	struct timespec foo; \
	foo.tv_sec=x/1000000000; \
	foo.tv_nsec=x%1000000000; \
	nanosleep(&foo,&foo); \
	}

