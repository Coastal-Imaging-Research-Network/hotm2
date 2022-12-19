/* deal with alarms while collecting data */

/*
 * $Log: alarm.c,v $
 * Revision 1.3  2003/01/07 21:43:30  stanley
 * coerce type of handler in signal call
 *
 * Revision 1.2  2002/11/05 23:25:38  stanley
 * added rcs tags
 *
 */

static const char rcsid[] = "$Id: alarm.c 388 2018-09-20 21:21:40Z stanley $";

#include <unistd.h>
#include <signal.h>
#include "hotm.h"

char message[128];
char module[32];
int isFatal;

void alarmHandler( int i )
{

	KIDID;
	printf( "cam %ld: %s ALARM from %s\n", 
		cameraModule.cameraNumber, 	
		isFatal? "FATAL":"WARNING", 
		module );
	KIDID;
	printf( "cam %ld:  ALARM message: %s\n", 
		cameraModule.cameraNumber, message );
	
	if( isFatal ) {
		if( cameraModule.stopCamera ) (*cameraModule.stopCamera);
		_exit(-1);
	}
	if( isFatal ) exit( EXIT_FAILURE );
	
}

void clearAlarm( )
{
	alarm( 0 ) ;
	message[0] = module[0] = 0;
}


void setAlarm( int fatal, void *handler, int timeout, char *what, char *where ) 
{
	clearAlarm();
	isFatal = fatal;
	strncpy( message, what, sizeof(message)-1 );
	strncpy( module, where, sizeof(module)-1 );
	if( NULL == handler ) {
		signal( SIGALRM, &alarmHandler );
	}
	else {
                signal( SIGALRM, (__sighandler_t) handler );
	}
	alarm( timeout );

}

void defaultAlarm( char *what, char *where )
{
	setAlarm( 1, 0, 5, what, where );
}

