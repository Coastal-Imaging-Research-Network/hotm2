/* serial -- functions to hand serial port
*/

/* 
 *  $Log: timer.c,v $
 *  Revision 1.2  2003/05/01 20:16:54  stanley
 *  made module a define
 *
 *  Revision 1.1  2002/08/14 21:29:00  stanleyl
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

static const char rcsid[] = "$Id: timer.c 172 2016-03-25 21:03:51Z stanley $";


#include "hotm.h"

#define DEFAULTPORT "/dev/ttyS1"
#define DEFAULTCODE "hc11.code"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <termios.h>
#include <fcntl.h>

#define VERB_INIT 1
#define VERB_PORT 8
#define VERB_CODE 16
#define VERB_ANSWER 32
#define VERBOSE(x) if( verbose & x )

int serialFD = 0;
char serialPort[128];
#define MODULE "serialTimer"

void sendAndGet( char *line, int verbose )
{

		VERBOSE(VERB_CODE) {
			KIDID;
			printf("%s: code: %s", MODULE, line );
		}
		write( serialFD, line, strlen(line)-1 );
		write( serialFD, "\r", 1 );
		sleep(1);
		VERBOSE(VERB_ANSWER) {
			lseek( serialFD, 0, SEEK_END );
			line[read( serialFD, line, 120 )] = 0;
			KIDID;
			printf("%s: answer: %s", MODULE, line );
		}
}



void _init()
{

	struct termios  termstat;
    unsigned long   turnOff;
    unsigned long   turnOn;
	char line[128];
	long verbose = 0;
	char hc11[128];
	long baud = 9600;
	FILE *in;
	long period = 500;
	long tweak = 4;

	getLongParam( &verbose, "timerverbose", MODULE );
	VERBOSE(VERB_INIT) {
		KIDID;
		printf( "%s: _init\n", MODULE );
	}

	if( serialFD > 0 ) 
		close( serialFD );
		
	strcpy( serialPort, DEFAULTPORT );
	getStringParam( serialPort, "timerport", MODULE );
	getLongParam( &baud, "timerbaud", MODULE );
	strcpy( hc11, DEFAULTCODE );
	getStringParam( hc11, "timercode", MODULE );
	
	VERBOSE(VERB_PORT) {
		KIDID;
		printf("%s: opening port %s, speed %d, code file %s\n",
				MODULE, 
				serialPort,
				baud,
				hc11 );
	}
	
	in = fopen( hc11, "r" );
	if( NULL == in ) {
		KIDID;
		perror( "serial code open" );
		exit(-1);
	}
	
	serialFD = open( serialPort, O_RDWR | O_NOCTTY | O_SYNC );
	
	if( serialFD < 0 ) {
		KIDID;
		perror( "serial open" );
		exit(-1);
	}
	
	if( tcgetattr(serialFD, &termstat) ) {
		KIDID;
		perror("serial getattr");
		exit(-1);
	}
	
	/* map baud to termstat value */
	switch( baud ) {
		case 9600:
			baud = B9600;
			break;
		case 1200:
			baud = B1200;
			break;
		case 2400:
			baud = B2400;
			break;
		case 4800:
			baud = B4800;
			break;
		case 19200:
			baud = B19200;
			break;
		default:
			KIDID;
			printf("%s: speed %d not supported, using 9600\n",
					MODULE, 
					baud );
			baud = B9600;
	}
	
	/* turn on and off the correct things */
    turnOff = 0;
    turnOn = ICRNL | IGNBRK | IGNPAR;
    termstat.c_iflag &= ~turnOff;
    termstat.c_iflag |= turnOn;

    turnOff = 0;
	turnOn = ONLCR; 
    termstat.c_oflag &= ~turnOff;
    termstat.c_oflag |= turnOn; 

    turnOff = ISIG | ICANON | ECHO;
    termstat.c_lflag &= ~turnOff;  

    turnOff = CSIZE | PARENB | CBAUD | CIBAUD;
    turnOn = CS8 | baud | (baud << 16);
    termstat.c_cflag &= ~turnOff;
    termstat.c_cflag |= turnOn;

    termstat.c_cc[VMIN] = 0;
    termstat.c_cc[VTIME] = 20;

 	if( tcsetattr( serialFD, TCSAFLUSH, &termstat ) ) {
		perror( "serial setattr" );
		exit(-1);
	}
	
	while( NULL != fgets( line, 120, in ) ) {
		sendAndGet( line, verbose );
	}
	
	getLongParam( &period, "timerperiod", MODULE );
	getLongParam( &tweak, "timertweak", MODULE );
	VERBOSE(VERB_INIT) {
		KIDID;
		printf("%s: tweak set to %d, period %d\n",
				MODULE,
				tweak,
				period );
	}
		
	sprintf( line, "%d T %d MS\r", tweak, period );
	sendAndGet( line, verbose );
	
	return;
	
}

	
	
