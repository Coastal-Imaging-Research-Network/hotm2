/* newStack.c -- the stack functions for the demon module */
/* new version replaces CIL XML version -- netcdf! */

/* 
 *  $Log: newStack.c,v $
 *  Revision 1.3  2016/03/24 23:17:16  root
 *  changed CIL XML to real netcdf
 *
 *  Revision 1.2  2014/08/13 18:20:55  root
 *  updated to 16 bit.
 *
 *  Revision 1.1  2010/01/13 00:26:07  stanley
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

static const char rcsid[] = "$Id: newStack.c 172 2016-03-25 21:03:51Z stanley $";


#include "demon2.h"
#include <netinet/in.h>
#include "math.h"
#include "rast.h"
#include <netcdf.h>

extern FILE *logFD; /* opened by demon2, we are linked into him */

#define TS if( status != NC_NOERR ) { \
		fprintf( logFD, \
			"NCERR: line %d %d %s\n", \
			__LINE__, \
			status, \
			nc_strerror(status)); \
		}


/* must be global, needed by init and write */
int epvar;
int gainvar;
int intTimevar;
int pixvar;

int is16bit;

void writeNetCDFLine( int fd, 
			double myTime, 
			const size_t lineCount,
			double gain,
			double intTime,
			double count,
			int sixteenbit,  /* to keep from using the global */
			void *outline ) {

size_t start[2];
size_t dcount[2];
int status;

	/* write stuff out. */
	status = nc_put_var1_double( fd, gainvar, &lineCount, &gain ); TS;
	status = nc_put_var1_double( fd, intTimevar, &lineCount, &intTime ); TS;
	status = nc_put_var1_double( fd, epvar, &lineCount, &myTime ); TS;

	start[0] = lineCount;  /* line number */
	start[1] = 0;          /* starting at left edge */

	dcount[0] = 1;      /* one row */
	dcount[1] = count;  /* of 'count' pixels */

	if( sixteenbit ) {
		status = nc_put_vara_ushort( fd, pixvar, start, dcount, outline ); TS;
	}
	else {
		status = nc_put_vara_ubyte( fd, pixvar, start, dcount, outline ); TS;
	}

	status = nc_sync( fd );

}

/* newStackWrite -- output a line to the new stack format */
void newStackWrite( struct _diskdata_ *p, struct _frame_ *frame )
{
struct timeval now;
register unsigned char *ptr;
register unsigned char *data;
unsigned long i;
unsigned char *outPtr;
uint16_t *usOutPtr;
uint16_t *usPtr;
unsigned char r,g,b;
unsigned long temp;
struct _pixel_ pix;
unsigned long w = cameraModule.cameraWidth;
double myTime;
double myShutter;
double myGain;
long dims[2];
long ndims;
int is16bit;
	
	is16bit = (cameraModule.format == formatMONO16)? 1 : 0;

	outPtr = (unsigned char *) p->pixOutLine;
	usOutPtr = (uint16_t *)outPtr;

	ptr = frame->dataPtr;
	usPtr = (uint16_t *)ptr;

	now = frame->when;

	/* epoch time of frame */
	myTime = (double) now.tv_sec + (double) now.tv_usec/ 1000000;

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

			case formatMONO16:
				*usOutPtr++ = usPtr[p->pixlist[i].pixel];
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
					temp /= 256;
					*outPtr++ = temp;
					break;
				case 4: /* green pixel, RED right */
					temp = 129 * *data;
					temp += 12 * *(data-w);
					temp += 13 * *(data+w);
					temp += 33 * *(data+1);
					temp += 33 * *(data-1);
					temp /= 256;
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
					temp /= 256;
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
					temp /= 256;
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
	
	/* I need a line counter. p->count counts DOWN! */
	temp = p->nFrames - p->count;

	writeNetCDFLine( p->fd,  /* not an fd anymore, netcdf ncid! */
			myTime, 
			temp,
			cameraModule.realGain, 
			cameraModule.intTime,
			p->pixelCount,
			is16bit,
			p->pixOutLine );
			
		
	return;
}


void newStackInit( struct _diskdata_ *p )
{
unsigned long 		size;
struct _processModule_ 	*me;
char 			*module = "stack";
struct _commandFileEntry_ *cmd;
char line[128];
double U, V;
unsigned long temp;
int iu, iv;
int i;
unsigned long *UV;
int is16bit;
int pixdim;
int epdim;
int dimids[10];
int status;

	is16bit = cameraModule.format==formatMONO16 ? 1 : 0;

	/* reopen the file -- I don't want a file descriptor anymore */
	/* I need to let netcdf manage the file for me */
	close( p->fd );
	status = nc_create( p->name, NC_NETCDF4, &p->fd );
	if( status != NC_NOERR ) {
		fprintf( logFD, 
			"NetCDF create error: %s %d %s\n", 
			p->name,
			status,
			nc_strerror(status) );
		p->fd = 0;  /* cannot write anywhere, this is how I tell others */
		return;    
	}

	/* from this point out, p->fd is a netcdf id */
			
	/* stack magic number. for historical reasons. stack loading routines */
	/* look for this to know this is a proper stack file. */
	temp = RAS_MAGIC;
	status = nc_put_att_long( p->fd, NC_GLOBAL, "magic", NC_DOUBLE, 1, (long *)&temp ); TS;

	status = nc_put_att_double( p->fd, NC_GLOBAL, "when", NC_DOUBLE, 1, &p->begin ); TS;
	status = nc_put_att_int( p->fd, 
				NC_GLOBAL, 
				"camera", 
				NC_INT, 
				1, 
				(int *)&cameraModule.cameraNumber ); TS;
	
	temp = 10;
	status = nc_put_att_int( p->fd, NC_GLOBAL, "version", NC_INT, 1, (int *)&temp ); TS;

	temp = 0;
	status = nc_put_att_int( p->fd, NC_GLOBAL, "isColor", NC_INT, 1, (int *)&temp ); TS;
	
	status = nc_put_att_int( p->fd, NC_GLOBAL, "pixels", NC_INT, 1, (int *)&p->pixelCount ); TS;
	status = nc_put_att_int( p->fd, NC_GLOBAL, "lines", NC_INT, 1, (int *)&p->count ); TS;

	status = nc_put_att_text( p->fd, 
				NC_GLOBAL, 
				"where", 
				strlen( hotmParams.sitename ),
				hotmParams.sitename ); TS;

	status = nc_put_att_text( p->fd, 
				NC_GLOBAL, 
				"aoifile", 
				strlen( p->aoiFilename ),
				p->aoiFilename ); TS;

	status = nc_put_att_int( p->fd, 
				NC_GLOBAL, 
				"pixDebayer", 
				NC_INT, 
				1,
				(int *)&p->stackDeBayer ); TS;

	i = 1; /* pixel depth is "number of elements per pixel", not bytes! */
	status = nc_put_att_int( p->fd, 
				NC_GLOBAL,
				"pixelDepth",
				NC_INT,
				1,
				(int *)&i ); TS;

	if( is16bit ) strcpy( line, "uint16" );
	else strcpy(line, "uint8");
	status = nc_put_att_text( p->fd, 
				NC_GLOBAL,
				"pixelType",
				strlen(line),
				line ); TS;
				
	/* what fun, must DECOMPOSE UV list from one-D pixel list. */
	UV = (unsigned long *)malloc( p->pixelCount*sizeof(unsigned long));

	for( i=0; i<p->pixelCount; i++ )
		UV[i] = (p->pixlist[i].pixel%cameraModule.x)+1;

	status = nc_put_att_int( p->fd, 
				NC_GLOBAL,
				"U",
				NC_SHORT,
				p->pixelCount,
				(int *)UV ); TS;
		
	for( i=0; i<p->pixelCount; i++ ) 
		UV[i] = (p->pixlist[i].pixel/cameraModule.x)+1;

	status = nc_put_att_int( p->fd, 
				NC_GLOBAL,
				"V",
				NC_SHORT,
				p->pixelCount,
				(int *)UV ); TS;

	/* we've now stuffed in the header, let's define the data that follows */

	/* two dimensions, pixels over time (epochs) */
	status = nc_def_dim( p->fd, "pixels", p->pixelCount, &pixdim ); TS;
	status = nc_def_dim( p->fd, "epochs", NC_UNLIMITED,  &epdim ); TS;
	dimids[0] = epdim;
	dimids[1] = pixdim;
	dimids[2] = 0;

#define INTUNITS "s"
#define PIXUNITS "pixel int (AU)"
#define EPUNITS "seconds since 1970-1-1"
#define GAINUNITS "gain (dB)"

	/* epoch time follows epochs dimension */
	status = nc_def_var( p->fd, "epoch", NC_DOUBLE, 1, &epdim, &epvar ); TS;
	status = nc_put_att_text( p->fd, epvar, "units", strlen(EPUNITS), EPUNITS ); TS;

	/* gain follows epochs dimension */
	status = nc_def_var( p->fd, "gain", NC_FLOAT, 1, &epdim, &gainvar ); TS;
	status = nc_put_att_text( p->fd, gainvar, "units", strlen(GAINUNITS), GAINUNITS ); TS;
	
	/* integration time follows epochs dimension */
	status = nc_def_var( p->fd, "intTime", NC_FLOAT, 1, &epdim, &intTimevar ); TS;
	status = nc_put_att_text( p->fd, intTimevar, "units", strlen(INTUNITS), INTUNITS ); TS;

	/* define pixels. have to consider bits */
	if( is16bit ) {
		status = nc_def_var( p->fd, "pix", NC_USHORT, 2, dimids, &pixvar ); TS;
	}
	else {
		status = nc_def_var( p->fd, "pix", NC_UBYTE, 2, dimids, &pixvar ); TS;
	}
	status = nc_put_att_text( p->fd, pixvar, "units", strlen(PIXUNITS), PIXUNITS ); TS;
	status = nc_put_att_text( p->fd, 
				pixvar, 
				"long_name", 
				strlen("pixel values"),
				"pixel values" ); TS;

	temp = 0;
	status = nc_put_att_int( p->fd, pixvar, "valid_min", NC_INT, 1, (int *)&temp ); TS;
	if( is16bit )
		temp = 65535;
	else
		temp = 255;
	status = nc_put_att_int( p->fd, pixvar, "valid_max", NC_INT, 1, (int *)&temp ); TS;

	/* whew! */
	status = nc_enddef( p->fd ); TS;
	status = nc_sync( p->fd ); TS;
	
	/* one last task, must fix pixel types IF deBayering stack */
	/* depending on the pixel order. BGGR is "other way" */
	if( (cameraModule.rawOrder[0] == 'B') 
	  && (p->stackDeBayer ) )
		for( i=0; i<p->pixelCount; p->pixlist[i++].type ^= 0x02 );	

}
	
	
void newStackDone( struct _diskdata_ *p ) 
{
int status;

	/* close the netcdf file */
	status = nc_close( p->fd ); TS;
	p->fd = 0;
	
}
