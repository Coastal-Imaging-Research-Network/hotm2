/* demon.c -- write data to disk when asked
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

static const char rcsid[] = "$Id: okdemon.c 172 2016-03-25 21:03:51Z stanley $";

#include "demon.h"

extern int errno;


struct _diskdata_  diskdata[CONCURRENT_ACTIONS];

extern void stackInit( struct _diskdata_ * );
extern void stackWrite( struct _diskdata_ *, struct _frame_ * );

struct timeval lastFrameSeen;

key_t msgIn;
int   msgId;
int   msgAnswer;  /* for acks to transient commands */

struct {
	long type;
	key_t replyKey;  /* sent as part of the message by client */
	char bufc[2048];
} msgBuf;

unsigned long MSC; /* fake MSC to help skip frames */
struct {
	long high;
	long low;
	long skip;
	double intLimit;
	unsigned long pixList[2048];
} autoShutter;

FILE *logFD;

void msgOUT ( int msgQue, long type, char *text ) 
{
struct {
	long type;
	key_t replyKey;
	char bufc[256];
}  myBuf;

	myBuf.type = type;
	strcpy( myBuf.bufc, text );
	myBuf.replyKey = msgIn;
	
	msgsnd( msgQue, &myBuf, strlen(myBuf.bufc)+sizeof(key_t), IPC_NOWAIT );
	fprintf( logFD, "%s\n", text );
	
}
	
#define msgOK(y) (msgOUT(msgAnswer, 1, y))
#define msgDone(y) (msgOUT(msgAnswer, 2, y))
#define msgBAD(y) (msgOUT(msgAnswer,999,y))
#define msgSTATUS(y) (msgOUT(msgAnswer, 3, y))

#define freeAndClear(x)  if(x) { free(x); x = NULL; }

void newAutoList(void ) {

int i;
int j;

	for( j=0,i=1; 
		(i<cameraModule.bufferLength)&(j<sizeof(autoShutter.pixList)/sizeof(unsigned long)-1); 
		i+=autoShutter.skip,j++ ) 
		autoShutter.pixList[j] = i;
	autoShutter.pixList[j] = 0;
	return;
}

void AElist( int minX, int minY, int maxX, int maxY, int skip ) {

int x = minX;
int y = minY;
unsigned long ptr;
int i;


	if( skip == -1 ) { /* autoskip */
	
		ptr = (maxX-minX)*(maxY-minY);
		skip = ptr/1500;
		if( skip == 0 ) skip = 1;
		
	}
	
	for( i=0; i<2047; i++ ) {
	
		ptr = cameraModule.x * y + x;
		autoShutter.pixList[i] = ptr * 2 + 1;
		
		x += skip;
		while( x > maxX ) {
			y++;
			x = minX + x - maxX;
		}
		if( y > maxY ) break;
		
	}
	autoShutter.pixList[i] = 0;
	return;
}

void sonyAEspot( int spot ) {

	switch( spot ) {

		case 0: /* all screen */
			AElist( 1, 
					1, 
					cameraModule.x-1, 
					cameraModule.x-1, 
					-1 );
			break;
		case 1: /* center small */
			AElist( (cameraModule.x/2)-40, 
					(cameraModule.y/2)-40,
					(cameraModule.x/2)+40,
					(cameraModule.y/2)+40,
					-1 );
			break;
		case 2:	/* left down */
			AElist( 1, 
					(cameraModule.y/2),
					(cameraModule.x/2),
					cameraModule.y-1,
					-1 );
			break;
		case 3:	/* right down */
			AElist( (cameraModule.x/2), 
					(cameraModule.y/2),
					cameraModule.x-1,
					cameraModule.y-1,
					-1 );
			break;
		case 4:	/* lower center */
			AElist( (cameraModule.x/2)-40, 
					cameraModule.y-80,
					(cameraModule.x/2)+40,
					cameraModule.y-1,
					-1 );
			break;
		case 5:	/* left up */
			AElist( 1, 
					1,
					(cameraModule.x/2),
					(cameraModule.y/2),
					-1 );
			break;
		case 6:	/* right up */
			AElist( (cameraModule.x/2),
					(cameraModule.y/2),
					cameraModule.x-1,
					cameraModule.y-1,
					-1 );
			break;
		case 7: /* center big */
			AElist( (cameraModule.x/2)-100, 
					(cameraModule.y/2)-100,
					(cameraModule.x/2)+100,
					(cameraModule.y/2)+100,
					-1 );
			break;
		default:
			break;
	}
}


void getCommand( void ) {

char line[128];
int i;
int j;
double dtemp;
char *token;
int tempFD;
int msgSize;
struct _diskdata_ *p;
unsigned short *pi;

	msgSize = msgrcv( msgId, &msgBuf, 2052, 0, IPC_NOWAIT );
	if( msgSize>0 ) {
	
		msgBuf.bufc[msgSize-sizeof(key_t)] = 0;
		/* create a response key for ack */
		msgAnswer = msgget( msgBuf.replyKey, 0 );

		/* sort out the message types */
		if (msgBuf.type == 6 ) {
			/* type 6 -- autoshutter pixel list */
			/* a set of unsigned long indexes into image */
			if( msgSize == sizeof(key_t) ) /* no list, replace old */
				newAutoList();
			else {
				memcpy( autoShutter.pixList, msgBuf.bufc, msgSize-sizeof(key_t) );
				/* ensure a 0 at the end */
				autoShutter.pixList[(msgSize/4)] = 0;
			}
			msgOK( "new autoshutter list" );
			return;
		}
		
		if( msgBuf.type == 5 ) {
			/* type 5 is 'pixlist input'. will have raw binary */
			
			/* find the slot for this data */
			for( i=0; i<CONCURRENT_ACTIONS; i++ ) {
				p = &diskdata[i];
				if( p->fd == -1 ) continue;
				if( p->msgBack == msgAnswer ) break;
			}
			if( i == CONCURRENT_ACTIONS ) {
				sprintf( line, 
					"getCommand type 5 no slot matches key %x",
					msgAnswer );
				msgBAD( line );
				return;
			}
			
			/* ok, p points to slot, get count of input UV */
			i = (msgSize - sizeof(key_t)) / sizeof(unsigned short);
			if( i == 0 ) { /* pixel list done! */
				p->pixDone = 1;
				msgDone( "pixlist accepted" );
				stackInit( p );
				return;
			}
			if( i%2 ) {
				msgBAD( "odd number of numbers in UV" );
				return;
			}

			pi = (unsigned short *)msgBuf.bufc;
			for( ; i>0 ; i-=2 )
				*p->pPtr++ = (*pi++ - 1) +
						(*pi++ - 1) * cameraModule.x;
			msgOK( "pixlist go ahead" );
			
			return;
		}
			
		if( msgBuf.type != 1 ) {
			sprintf( line, "unknown type: %d for msg: %s\n",
				msgBuf.type, msgBuf.bufc );
			msgBAD( line );
			return;
		}

		fprintf(logFD, "getCommand read %d bytes, as: %s\n", strlen(msgBuf.bufc), msgBuf.bufc );

		/* have command, lets parse it */
		/* 'add outputname numberofframes skip aoifile pid' */
		token = strtok( msgBuf.bufc, " " );
		if( !strcasecmp( "add", token ) ) {

			/* first, find empty diskdata struct to use */
			for( i=0; i<CONCURRENT_ACTIONS; i++ ) {
				if( diskdata[i].fd == -1 ) 
					break;
			}
			if( i == CONCURRENT_ACTIONS ) {
				msgBAD( "too many actions going on, ignoring" );
				return;
			}
			p = &diskdata[i];

			/* get output destination */
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD( "missing output from add command" );
				return;
			}
			strcpy( p->name, token );
			
			/* get number of frames */
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD( "missing number of frames from add command" );
				return;
			}
			p->count = strtol( token, NULL, 0 );
			
			/* get skip count */
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD( "missing skip from add command" );
				return;
			}
			p->skip = strtol( token, NULL, 0 );
			
			/* get start time */
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD( "missing start time from add command" );
				return;
			}
			p->begin = strtol( token, NULL, 0 )
				+ (((lastFrameSeen.tv_usec+250000)%500000)/1000000.0);
			p->MSC = 0;

			/* get aoifile, says this is stack if not '0' */
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD( "missing pixcount from add command" );
				return;
			}

			p->pixDone = 0;
			p->pixelCount = strtol( token, NULL, 0 );
			if( p->pixelCount > 0 ) {
				if( p->pixelCount%2 ) /* odd length */
					p->pixelCount++;  /* make even -- rasterfile */
				p->pixlist = p->pPtr = 
					calloc( p->pixelCount, sizeof( unsigned long ) );
				if( p->pixlist == NULL ) {
					msgBAD( "cannot malloc pixlist" );
					return;
				}
				p->pixOutLine = 
					calloc( p->pixelCount*3+8, 1 );
				if( p->pixOutLine == NULL ) {
					msgBAD( "cannot malloc pixOutput" );
					freeAndClear(p->pixlist);
					return;
				}
			/* later type 5 messages will fill in the pixlist */
			}
			
			/* get aoifilename -- so pixel tools can use this stack */
			token = strtok( NULL, " " ) ;
			if( p->pixelCount ) {
				if( token == NULL ) {
					msgBAD( "no aoifilename" );
					freeAndClear( p->pixlist );
					freeAndClear( p->pixOutLine );
					return;
				}
				strcpy( p->aoiFilename, token );
			}
			
			/* pid of requestor is replyKey from message */
			p->requestor = msgBuf.replyKey;
			p->msgBack = msgAnswer;
			
			/* open output */
			p->fd = open( p->name, O_WRONLY|O_CREAT|O_NDELAY, S_IRWXU );
			if( p->fd == -1 ) {
				freeAndClear( p->pixlist );
				freeAndClear( p->pixOutLine );
				perror( "add command open file" );
				msgBAD( "cannot open output" );
				return;
			}
			
			/* only write the header if we aren't a stack! */
			if( p->pixlist == NULL ) {
				cameraModule.frameCount = p->count;
				write( p->fd, &hotmParams, sizeof hotmParams);
				write( p->fd, &cameraModule, sizeof cameraModule );
			}
		
			sprintf(line, 
					"added output to file %s for %d frames requestor %d at %f\n", 
					p->name, 
					p->count,
					p->requestor,
					p->begin );
			msgOK( line );

		}
		else if( !strcasecmp( "endAll", token ) ) {
			for(i=0; i<CONCURRENT_ACTIONS; i++) 
				diskdata[i].count = 1;
			msgOK("endAll");
		}
		
		else if( !strcasecmp( "exit", token ) ) {
			msgOK("exiting at your command");
			exit(0);
		}
		
		else if( !strcasecmp( "ping", token ) ) {
			msgOK("pong");
		}
		
		else if( !strcasecmp( "changeGain", token ) ) {
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD("missing ratio for changeGain");
				return;
			}
			dtemp = strtod( token, NULL );
			(*cameraModule.changeGain)(dtemp);
			msgOK("changeGain");
		}
		else if( !strcasecmp( "changeShutter", token ) ) {
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD("missing ratio for changeShutter");
				return;
			}
			dtemp = strtod( token, NULL );
			(*cameraModule.changeShutter)(dtemp);
			msgOK("changeShutter");
		}
		else if( !strcasecmp( "restart", token ) ) {
			(*cameraModule.stopCamera)(&cameraModule);
			(*cameraModule.initCamera)(&cameraModule);			
			msgOK("restart");
		}
		else if( !strcasecmp( "autoShutter", token ) ) {
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD("missing low value for autoShutter");
				return;
			}
			autoShutter.low = strtol( token, NULL, 0 );
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD("missing high value for autoShutter");
				return;
			}
			autoShutter.high = strtol( token, NULL, 0 );
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD("missing skip value for autoShutter");
				return;
			}
			autoShutter.skip = strtol( token, NULL, 0 );
			newAutoList();
			msgOK("autoShutter");
		}
		else if( !strcasecmp( "intLimit", token ) ) {
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD("missing value for intLimit");
				return;
			}
			autoShutter.intLimit = strtod( token, NULL );
			msgOK("intLimit");
		}
		else if( !strcasecmp( "flushLog", token ) ) {
			fclose(logFD);
			sprintf( line, "/tmp/cam%02dd.log", cameraModule.cameraNumber );
			logFD = fopen( line, "w" );
			if( NULL == logFD ) {
				perror("init log open");
			}
			msgOK("flushLog");
		}

		else if( !strcasecmp( "dumpPixlist", token ) ) {
			/* dump all pixlists to log */
			for( i=0; i<CONCURRENT_ACTIONS; i++ ) {
				p = &diskdata[i];
				if( p->fd == -1 ) continue;
				if( p->pixlist == NULL ) continue;
				fprintf( logFD, "Slot %d current pixlist\n", i );
				for( j=0; j<p->pixelCount; j++ ) 
					fprintf( logFD, "  %5d:  %7d\n", j, p->pixlist[j] );
			}
			msgOK( "dumpPixlist" );
		}
		
		else if( !strcasecmp( "cleanSlot", token ) ) {
			token = strtok( NULL, " " );
			if( token == NULL ) {
				msgBAD("missing value for cleanSlot");
				return;
			}
			i = strtol( token, NULL, 0 );
			if( i<0 || i>CONCURRENT_ACTIONS ) {
				msgBAD("invalid slot");
				return;
			}
			p = &diskdata[i];
			if( p->fd ) 
				close(p->fd);
			freeAndClear( p->pixlist );
			freeAndClear( p->pixOutLine );
			memset( p, 0, sizeof(struct _diskdata_) );
			p->fd = -1;
			msgOK( "cleanSlot" );
		}
		
		else if( !strcasecmp( "AEspot", token ) ) {
			token = strtok( NULL, " " );
			if(token == NULL) {
				msgBAD("missing spot");
				return;
			}
			i = strtol( token, NULL, 0 );
			sonyAEspot( i );
			msgOK("AEspot");
		}
			
		else if( !strcasecmp( "dumpAutoList", token ) ) {
			fprintf( logFD, "Autoshutter pixlist:\n" );
			for( i=0; autoShutter.pixList[i]; i++ )
				fprintf(logFD, " %4d: %d\n", i, autoShutter.pixList[i]);
			msgOK("dumped autolist");
		}

		else if( !strcasecmp( "status", token ) ) {

			sprintf( line, "camera %d active and alive, MSC at %ld",
				cameraModule.cameraNumber,
				MSC );
			msgSTATUS( line );
			
			sprintf( line, "current gain: %d shutter: %d intTime: %.6f",
				cameraModule.gain, 
				cameraModule.shutter,
				cameraModule.intTime );
			msgSTATUS( line );
			
			sprintf( line, "autoShutter is %s, limits %d:%d int limit: %f, skip %d",
				autoShutter.high? "active" : "inactive",
				autoShutter.high,
				autoShutter.low,
				autoShutter.intLimit,
				autoShutter.skip );
			msgSTATUS( line );
			
			for( i=0; i<CONCURRENT_ACTIONS; i++ ) {
				if( diskdata[i].fd == -1 ) continue;
				sprintf( line, 
					"slot %2d: %5d more (skip %d) (start %f) for pid %5d to %s%s",
					i,
					diskdata[i].count,
					diskdata[i].skip,
					diskdata[i].begin,
					diskdata[i].requestor,
					diskdata[i].name,
					diskdata[i].pixlist==NULL? "" : " with pixlist" );
				msgSTATUS( line );
			}
			msgDone("end");
			return;
		}
		else if( !strcasecmp( "lastSeen", token ) ) {
		
			sprintf( line, "lastSeen: %d %06d", 
					lastFrameSeen.tv_sec,
					lastFrameSeen.tv_usec );
			msgOK( line );
		} 
		else {
			sprintf( line, "bad command: %s", token );
			msgBAD( line );
		}		
	}
}

void _saveResults( struct _processModule_ *me )
{

}


void _processFrame( struct _processModule_ *me, struct _frame_ *frame )
{
struct timeval start;
struct timeval end;
unsigned char *ptr;
int err;
int i;
int maxY;
int histY;
long shutter;
int showChange;
double now;
static double previous;
static double reportTime;
char line[128];
struct _diskdata_ *p;
struct _imageHeader_ ih;
	MSC++;
	showChange = 0;
	
	err = gettimeofday( &start, NULL );
	if( err ) 
		perror( me->module );
		
	ptr = frame->dataPtr;
	lastFrameSeen = frame->when;
	now = 1.0 * frame->when.tv_sec + (frame->when.tv_usec/1000000.0);

	if( previous == 0 )
		previous = now;
		
	if( ((now - previous) > .75) && ((now-reportTime)>600) ) {
	
		fprintf( logFD, 
			"**** late frame: at %f MSC %d\n",
			now, MSC );

		fprintf( logFD, 
			"current gain: %d shutter: %d intTime: %.6f",
			cameraModule.gain, 
			cameraModule.shutter,
			cameraModule.intTime );
		fprintf( logFD, 
			"autoShutter is %s, limits %d:%d int limit: %f",
			autoShutter.high? "active" : "inactive",
			autoShutter.high,
			autoShutter.low,
			autoShutter.intLimit );
			
		reportTime = now;
	}

	/* for each of the diskdata actions... */
	for( i=0; i<CONCURRENT_ACTIONS; i++ ) {
	
		p = &diskdata[i];
	
		/* nothing here to do, move along...*/
		if( -1 == p->fd )  continue;
		
		/* check for begin time */
		if( now < p->begin ) continue;
		
		if( p->pixlist && !p->pixDone ) continue;
		
		/* check for skip */
		if( p->skip ) 
			if( (p->MSC++)%(p->skip+1) ) continue;

		if( p->pixlist ) {
			stackWrite( p, frame );
		}
		else {
			ih.when = frame->when;
			ih.shutter = cameraModule.shutter;
			ih.gain = cameraModule.gain;
		
/*			for( i=0; autoShutter.pixList[i]; i++ ) 
				ptr[autoShutter.pixList[i]]+=100;
*/			
			/*  ok, write a frame */
			write( p->fd, &ih, sizeof( ih ) ); 
			write( p->fd, ptr, p->size );
		}
		
		/* count it */
		if( 0 == --p->count ) {
		
			/* all done! */
			close( p->fd );
			p->fd = -1;
			freeAndClear( p->pixlist );
			freeAndClear( p->pixOutLine );
			
			/* tell the requestor I'm done */
			if( p->requestor && p->msgBack ) {
				msgAnswer = p->msgBack;
				msgDone( "all done, go ahead" );
			}
		}
		
	}
		
	previous = now;
	/*me->remaining--;*/

	/* experiment! */
	if( autoShutter.high ) {
	
		maxY = 0; histY = 0;
#ifdef OLDWAY
		for( i=1; i<cameraModule.bufferLength; i += autoShutter.skip ) {
			if( ptr[i] > maxY ) maxY = ptr[i];
			if( ptr[i] == 255 ) histY++;
		}
#else
		for( i=0; autoShutter.pixList[i]; i++ ) {
			if( ptr[autoShutter.pixList[i]] > maxY ) 
				maxY = ptr[autoShutter.pixList[i]];
			if( ptr[autoShutter.pixList[i]] == 255 ) histY++;
		}
#endif

		
		/* if not bright enough, and gain isn't at max -- */
		if( (histY < autoShutter.low) && (cameraModule.gain < 2228) ) { 
			/* try adjusting shutter, first */
			shutter = cameraModule.shutter;
			/* limit the int time to .3 seconds or thereabouts */
			if( cameraModule.intTime < autoShutter.intLimit )
				(*cameraModule.changeShutter)(1.05);
			/* if the shutter has maxed (no change when tried) -- */
			if( cameraModule.shutter == shutter ) {
				/* pump up the gain */
				(*cameraModule.changeGain)(+1.0);
			}
			showChange++;
		}	
		/* if it is too bright -- */
		if( histY > autoShutter.high ) {
			if( cameraModule.gain > 0x800 ) {
				(*cameraModule.changeGain)(-1.0);
			}
			else {
				(*cameraModule.changeShutter)(0.95);
			}
			showChange++;
		}
		if( showChange )
			fprintf(logFD, 
				"%010d autoShutter: maxY: %d histY: %d new gain: %d shutter: %d (%.6f sec)\n", 
				start.tv_sec,
				maxY, 
				histY,
				cameraModule.gain,
				cameraModule.shutter,
				cameraModule.intTime );
	}
	
	
	err = gettimeofday( &end, NULL );
	if( err ) {
		perror( me->module );
		me->processMicros = 9999999;
		return;
	}
	
	me->processMicros = (end.tv_sec - start.tv_sec) * 1000000
		+ (end.tv_usec - start.tv_usec );
		
	getCommand(); fflush(logFD);
		
	return;
}

void _processPixel( struct _processModule_ *me, struct _pixel_ *pix )
{
	return;
}

void _closeFrame( struct _processModule_ *me )
{
	return;
}

void _init()
{
unsigned long 		size;
struct _processModule_ 	*me;
char 			*module = "disk";
struct _commandFileEntry_ *cmd;
int i;
char line[128];
	
	/* problem -- how do I find ME in the process list? */
	/* well, I will be the last member of the list! */
	me = firstProcessModule;
	while( me->next ) 
		me = me->next;
	
	strcpy( me->module, module );
	
	for( i=0; i<CONCURRENT_ACTIONS; i++ ) {
		diskdata[i].size = cameraModule.x * cameraModule.y * 2;
		diskdata[i].fd = -1;
		diskdata[i].requestor = 0;
		diskdata[i].pixlist = NULL;
		diskdata[i].pixOutLine = NULL;
	}

	/*getLongParam( &me->remaining, "diskFrames", module );*/
	me->remaining = 1;
	getLongParam( &me->verbose, "diskVerbose", module );

	VERBOSE(VERB_INIT) {
		KIDID; 
		printf("cam %d: %s: _init\n", 
				cameraModule.cameraNumber,
				module );
	}
	
	/* make me a message queue */
	msgIn = cameraModule.cameraNumber;
	msgId = msgget( msgIn, IPC_CREAT | S_IRWXU | S_IRWXG | S_IRWXO );
	if( msgId < 0 ) {
		perror( "msg queue create" );
		exit(-1);
	}
	
	me->processFrame = _processFrame;
	me->processPixel = NULL;
	me->closeFrame = NULL;
	me->saveResults = _saveResults;
	
	MSC = 0;
	autoShutter.high = 30;
	autoShutter.low = 20;
	autoShutter.skip = 900;
	autoShutter.intLimit = 0.25; /* .25 second int time */
	/* note to self: self, at this int time, the granularity is */
	/* so large that the actual limit is .26666667. With .30, */
	/* it comes out to .33333, which appears to be a wee bit too */
	/* long to get 2Hz out of the camera.  steps are .07 seconds */
	newAutoList();
	
	sprintf( line, "/tmp/cam%02dd.log", cameraModule.cameraNumber );
	logFD = fopen( line, "w" );
	if( NULL == logFD ) {
		perror("init log open");
	}
			
}
	
	
