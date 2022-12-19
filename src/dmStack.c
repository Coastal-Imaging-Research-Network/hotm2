/* stack.c -- the stack functions for the demon module */

/* 
 *  $Log: dmStack.c,v $
 *  Revision 1.5  2016/03/24 23:55:08  stanley
 *  reweight RGB->I, fix cmap loop
 *
 *  Revision 1.3  2009/02/20 21:11:28  stanley
 *  added debayering on-the-fly for stack pixels
 *
 *  Revision 1.2  2005/01/29 00:35:47  stanley
 *  added packed data at end of line, version 8
 *
 *  Revision 1.1  2004/12/06 21:46:48  stanley
 *  Initial revision
 *
 *  Revision 1.3  2003/01/07 22:02:30  stanley
 *  cleanups to make intel compiler happy
 *
 *  Revision 1.2  2002/08/09 00:32:40  stanleyl
 *  change verbose parameterization
 *
 *  Revision 1.1  2002/03/22 19:50:26  stanley
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

static const char rcsid[] = "$Id: dmStack.c 172 2016-03-25 21:03:51Z stanley $";


#include "demon2.h"
#include <netinet/in.h>
#include "rast.h"
#include "math.h"

/* stackPack -- new routine to pack various data into the line */
/*   i.e., take intTime, realGain from cameraModule and convert 
 *   to float and send back the number of bytes to write to the end
 *   of the data line. The plan is that this is the only function to
 *   change when new stuff is added to the end of the line.
 *
 *   input: hmm, use info from cameraModule. 
 *   output: count of bytes to write from static packedData array.
 * 
 *   also put info into packedU and packedV array. So, we call 
 *   stackPack when creating the header to get count of added 
 *   columns and U and V for those columns, with meaningless data,
 *   and then for each line to get the data. Less likely for header
 *   and data to get out of sync than having two functions. I.e., 
 *   one can easily forget to update header data when adding row data.
 *
 */

#define MAXpackedLength 128
unsigned char packedData[MAXpackedLength];
unsigned char packedDataULow[MAXpackedLength];
unsigned char packedDataUHigh[MAXpackedLength];
unsigned char packedDataVLow[MAXpackedLength];
unsigned char packedDataVHigh[MAXpackedLength];

#define packedIntTime  0xf000
#define packedRealGain 0xf001

int stackPack( )
{

int i;
int ptr = 0;
union {
	float fdata;
	unsigned char uchar[4];
} data;

#define packUp(x, y) \
	data.fdata = x;\
	for( i=0; i<4; i++ ) { \
		packedData[ptr] = data.uchar[i]; \
		packedDataULow[ptr] = y & 0xff; \
		packedDataUHigh[ptr] = (y & 0xff00) >> 8; \
		packedDataVLow[ptr] = i; \
		packedDataVHigh[ptr] = 0; \
		ptr++; \
	} 

	packUp( cameraModule.intTime, packedIntTime );
	packUp( cameraModule.realGain, packedRealGain );

	if( ptr > MAXpackedLength ) {
		printf("packed data code broken, recompile with bigger packed data arrays\n" );
		exit(-1);
	}

	return ptr;

}  /* wow. How simple that was! */


void stackWrite( struct _diskdata_ *p, struct _frame_ *frame )
{
struct timeval start;
struct timeval end;
struct timeval now;
int err;
register unsigned char *ptr;
register unsigned char *data;
uint32_t i;
unsigned char *outPtr;
uint16_t *usPtr;
unsigned char r,g,b;
uint32_t temp;
struct _pixel_ pix;
int packedLen;
uint32_t w = cameraModule.cameraWidth;
	
	outPtr = (unsigned char *) p->pixOutLine;
	ptr = frame->dataPtr;
	usPtr = (uint16_t *)ptr;
	now = frame->when;

	/* milliseconds since starting this stack */
	temp = (now.tv_usec / 1000) + ((now.tv_sec-lrint(p->begin)) * 1000);
	temp = htonl(temp);
	write( p->fd, &temp, 4 );
	temp = htonl(((cameraModule.gain&0xffff)<<16) | 
					(cameraModule.shutter&0xffff));
	write( p->fd, &temp, 4 );

	/* ok, here we grab the stack data */
	/* in a marvelous twist of irony, I want it in YUV! */
	/* but I do not want to convert it all to YUV to save time */
	/* so, I'll do my own math to get the right bytes from the */
	/* char * ptr. */

	for( i=0; i<p->pixelCount; i++ ) {

		switch( cameraModule.format ) {
	
			case formatYUV422:
				data = ptr + (p->pixlist[i].pixel * 2) + 1;
				*outPtr++ = *data;
				break;

			case formatYUV444:
				data = ptr + (p->pixlist[i].pixel * 3) + 1;
				*outPtr++ = *data;
				break;

			case formatRGB8:
				/* point to R */
				data = ptr + (p->pixlist[i].pixel * 3);
				*outPtr++ = 16 + ((*data++) *  66 
					           +  (*data++) * 129 
					           +  (*data)   *  25) / 256;
				break;

			case formatMONO16:  /* IR camera */
				/* what to do to pack 16 bits into 8? */
				/* i'm gonna kludge! drop four low bits */
				*outPtr++ = ((usPtr[p->pixlist[i].pixel])>>4)&0xff;
				break;
				
			case formatMONO8:
				data = ptr + (p->pixlist[i].pixel);

			   switch( p->pixlist[i].type ){

				case 0:
					*outPtr++ = *data;
					break;

				case 6: /* green pixel, blue right */
					temp = 129 * *data;
					temp += 33 * *(data-w);
					temp += 33 * *(data+w);
					temp += 12 * *(data+1);
					temp += 13 * *(data-1);
					/* oh snap, weighting isn't 256, it
                                           is only 220! */
					temp /= 220;
					*outPtr++ = temp;
					break;
				case 4: /* green pixel, RED right */
					temp = 129 * *data;
					temp += 12 * *(data-w);
					temp += 13 * *(data+w);
					temp += 33 * *(data+1);
					temp += 33 * *(data-1);
					temp /= 220;
					*outPtr++ = temp;
					break;
				case 5: /* red pixel */
					temp = 66 * *data; /* red */
					temp += 32 * *(data-1);
					temp += 32 * *(data+1);
					temp += 32 * *(data-w);
					temp += 33 * *(data+w);
					temp += 6 * *(data+w+1);
					temp += 6 * *(data+w-1);
					temp += 6 * *(data-w+1);
					temp += 7 * *(data-w-1); 
					temp /= 220;
					*outPtr++ = temp;
					break;
				case 7: /* blue pixel */
					temp = 25 * *data; /* red */
					temp += 32 * *(data-1);
					temp += 32 * *(data+1);
					temp += 32 * *(data-w);
					temp += 33 * *(data+w);
					temp += 16 * *(data+w+1);
					temp += 16 * *(data+w-1);
					temp += 17 * *(data-w+1);
					temp += 17 * *(data-w-1); 
					temp /= 220;
					*outPtr++ = temp;
					break;
			   	default:
					*outPtr++ = *data;
					break;
			   }
			   break;

			default:
				/* not implemented yet */
				break;
		}
	}
	
	write( p->fd, p->pixOutLine, p->pixelCount );

	/* handle the packed camera data */
	packedLen = stackPack();
	write( p->fd, packedData, packedLen );
		
	return;
}


void stackInit( struct _diskdata_ *p )
{
uint32_t 		size;
struct _processModule_ 	*me;
char 			*module = "stack";
struct _commandFileEntry_ *cmd;
FILE *in;
char line[128];
double U, V;
uint32_t temp;
int iu, iv;
int i;
struct timeval now;
struct rasterfile r;
char cmap[256];
uint32_t *UV;
int32_t fakeUST;
int packedLen;

struct {
	uint32_t when;
	char cam;
	char gain;
	char offset;
	char version;
} line1;

struct {
	int32_t            pixelCount;
	char            hz;
	char            color;
	unsigned short  count;
} line2;

struct {
	char            name[8];
} line34;


	/* ain't life fun? we're on a good-endian system, but since */
	/* rasterfiles are based on Suns -- bad endian -- everyone */
	/* treats the header as wrong-endian and so must I */	

	/* call stackPack now, get length of packed data */
	packedLen = stackPack();
	

    /* magic numbers */
    r.ras_magic =     htonl(RAS_MAGIC);
	r.ras_width =     htonl(p->pixelCount + 8 + packedLen);
	r.ras_height =    htonl(p->count + 8);
    r.ras_length =    htonl((p->pixelCount+8+packedLen) * (p->count+8));
    r.ras_depth =     htonl(8);
    r.ras_type =      htonl(RT_STANDARD);
    r.ras_maptype =   htonl(RMT_EQUAL_RGB);
    r.ras_maplength = htonl(3 * 256);
	
    /* put in a color map */
    for (i = 0; i < 256; cmap[i] = i,i++);

	/* the following write puts out wrong data when using the intel
	   compiler and -ip -xW */
    write(p->fd, &r, 32);
    write(p->fd, cmap, 256);
    write(p->fd, cmap, 256);
    write(p->fd, cmap, 256);

	/* THANK GOODNESS WE ARE BACK ON A SYSTEM WITH THE RIGHT ENDIAN */
	line1.when = floor(p->begin);
	line1.cam  = cameraModule.cameraNumber;
	line1.gain = cameraModule.gain;
	line1.offset = cameraModule.shutter;
	line1.version = 9;  /* 6 for firewire, 7? for Irv, 8 for packed */

	line2.pixelCount = p->pixelCount; /* change in ver8 */
	line2.hz = 1;
	line2.color = p->stackDeBayer? 2 : 0;
	line2.count = p->count;
	 
	for( i=0; i<p->pixelCount; i++ )
		p->pixOutLine[i] = ((p->pixlist[i].pixel%cameraModule.x)+1)&0xff;
		
	write(p->fd, &line1, sizeof(line1) );
	write(p->fd, p->pixOutLine, p->pixelCount );
	write(p->fd, packedDataULow, packedLen );
		 
	for( i=0; i<p->pixelCount; i++ )
		p->pixOutLine[i] = (((p->pixlist[i].pixel%cameraModule.x)+1)&0xff00) >> 8;
		
	write(p->fd, &line2, sizeof(line2) );
	write(p->fd, p->pixOutLine, p->pixelCount );
	write(p->fd, packedDataUHigh, packedLen );
	
	strncpy( &line34.name[0], hotmParams.sitename, 8 );

	for( i=0; i<p->pixelCount; i++ ) 
		p->pixOutLine[i] = ((p->pixlist[i].pixel/cameraModule.x)+1) & 0xff;
		
	write(p->fd, &line34, sizeof(line34) );
	write(p->fd, p->pixOutLine, p->pixelCount );
	write(p->fd, packedDataVLow, packedLen );
	
	strncpy( &line34.name[0], p->aoiFilename, 8 );

	for( i=0; i<p->pixelCount; i++ )
		p->pixOutLine[i] = (((p->pixlist[i].pixel/cameraModule.x)+1)&0xff00)>> 8;
		
	write(p->fd, &line34, sizeof(line34) );
	write(p->fd, p->pixOutLine, p->pixelCount );
	write(p->fd, packedDataVHigh, packedLen );
	
	/* fake UST -- set to 0 -- pretend we just rebooted */
	fakeUST = 0;
	write(p->fd, &fakeUST, 4);;
	write(p->fd, &fakeUST, 4);
	strncpy( (char *)p->pixOutLine, 
				p->aoiFilename,
				p->pixelCount ); 
	write(p->fd, p->pixOutLine, p->pixelCount );
	write(p->fd, packedDataVHigh, packedLen );

	/* line 6 */	
	now.tv_sec = htonl(lrint(p->begin));
	now.tv_usec = htonl(0); 
	write(p->fd, &now, 8);
	write(p->fd, p->pixOutLine, p->pixelCount );
	write(p->fd, packedDataVHigh, packedLen );

	/* 7 and 8 are unused, reserved for future expansion */
	write(p->fd, &now, 8);
	write(p->fd, p->pixOutLine, p->pixelCount );
	write(p->fd, packedDataVHigh, packedLen );
	write(p->fd, &now, 8);
	write(p->fd, p->pixOutLine, p->pixelCount );
	write(p->fd, packedDataVHigh, packedLen );
	
	/* one last task, must fix pixel types IF deBayering stack */
	/* depending on the pixel order. BGGR is "other way" */
	if( (cameraModule.rawOrder[0] == 'B') 
	  && (p->stackDeBayer ) )
		for( i=0; i<p->pixelCount; p->pixlist[i++].type ^= 0x02 );	

        /* and GBRG (Flea2G) is yet another way. sigh. */
        if(  (cameraModule.rawOrder[0] == 'G')
          && (cameraModule.rawOrder[1] == 'B')
          && (p->stackDeBayer ) )
                for( i=0; i<p->pixelCount; i++ ) {
                        p->pixlist[i].type++;
                        if( p->pixlist[i].type > 7)
                                p->pixlist[i].type = 4;
                }

}
	
	
