
/* hotmomma -- mother to hotm 
 *
 *   function: call the right number of hotm's with the right 
 *    parameters, wait for them to sync with me saying they are
 *     ready to go, then get them going
 *

 *   1. read params for command file name
 *   2. loop in command file:
 *   3.    fork 
 *   4.parent pause
 *   4.child  exec hotm with -p (parent) flag      
 *   5. loop foreach child pid
 *         kill -usr1 to unpause

 */

/* 
 *  $Log: hotmomma.c,v $
 *  Revision 1.4  2003/10/05 23:05:47  stanley
 *  remove //
 *
 *  Revision 1.3  2002/10/03 23:47:55  stanley
 *  added # as comment in default.hotmomma file
 *
 *  Revision 1.2  2002/03/23 00:24:18  stanley
 *  message passing
 *
 *  Revision 1.1  2002/03/15 21:57:00  stanley
 *  Initial revision
 *
 */

/*  
 *  Jrv Elshoff
 *  WL | Delft Hydraulics
 *  P.O. Box 177
 *  2600 MH Delft
 *  Nederland
 *
 * $Copyright 2002 Argus Gebruikers Groep$
 *
 */

static const char rcsid[] = "$Id: hotmomma.c 172 2016-03-25 21:03:51Z stanley $";

#include "hotm.h"

#define MAXNUMKIDS 32
#define MAXNUMARGS 32

int mqid;

/* what to do atexit. */
void exitHandler() 
{
	printf ("**** Hotmomma is dying...\n"); fflush (stdout);
	FreeMessageQueue (mqid);
}

main( int argc, char **argv ) 
{
char command[] = "default.hotmomma";
FILE *in;
char line[128];
pid_t *kids;
int kid;
char **args;
int i;
pid_t pid;
int pidPtr = 0;
char dashP[] = "-p";

char mqs [16], kis[16];     /* string versions of mqid and kid */

	/* register a cleanup routine */
	atexit( exitHandler );

	kids = calloc( MAXNUMKIDS, sizeof(pid_t) );
	args = calloc( MAXNUMARGS, sizeof(char *) );

	in = fopen( command, "r" );
	if( NULL == in ) {
		printf("cannot open command file: %s\n", command );
		exit(1);
	}
	
	/* set my process group so I can kill all my buds without */
	/* making multiple calls */
	setpgrp();

    	/* setup message queue for talking to kids */
    	mqid = AllocMessageQueue ();
    	sprintf (mqs, "%d", mqid);

	printf ("Hotmomma forking kids\n");
	while( fgets( line, 128, in ) ) {
		
		if( line[0] == '#' ) 
			continue;
			
	    kid = pidPtr++;
    	sprintf (kis, "%d", kid);

		line[strlen(line)-1] = 0;
		
		/* parse the line for the execv */
		args[0] = strtok( line, " " );
		args[1] = dashP;
		args[2] = mqs;
		args[3] = kis;
		
		for( i=4; i<MAXNUMARGS; i++ ) {
			args[i] = strtok( NULL, " ");
			if( NULL == args[i] ) break;
		}

		if( MAXNUMARGS == i ) {
			printf("Hotmomma: too many args in comand line, skipping\n");
			printf("  line was: %s\n", line ); 
			continue;
		}
		
		/* do the fork */
		pid = fork();				

		if(pid) { /* I'm the parent, do what I say! */
			/* record my kid's pid so I can kill him later */
			kids[kid] = pid;
			printf("Hotmomma: forked pid %d\n", pid );
			/* wait for kid to complete initialization */
    	    	    	if (ExpectState (mqid, kid, MOMMA, stateReady) < 0) {
			    /* kid has died; remove its pid from table */
			    kids[kid] = 0;
			printf ("#### kid %d is ready\n", kid);
			}
			    
		}
		else { /* bratty little kid */
			/*printf("Hotmomma(%d) starting %s\n", args[0] );*/
			execv( args[0], (char **)&args[0] );
		}
		
		/* back from child, or back from kill from child */
		if( 0 == pid ) { /* back from child, error! */
			printf("execv failed! line: %s\n", line );
		}
	}
	
	/* tell kids to start image collection and wait for them to finish */
	printf ("#### Hotmomma telling kids to start collecting\n"); fflush (stdout);
	for (i = 0 ; i < pidPtr ; i++)
    	    	if (kids[i])
		    	AssertState (mqid, MOMMA, i, stateStart);
	printf ("#### Hotmomma waiting for kids to stop collecting\n"); fflush (stdout);
	for (i = 0 ; i < pidPtr ; i++)
	    	if (kids[i]) {
		    	printf ("#### waiting for kid %d to stop collecting\n", i); fflush (stdout);
		    	if (ExpectState (mqid, i, MOMMA, stateDone) < 0)
		    	    	kids[i] = 0;
		    	printf ("#### kid %d has stopped collecting\n", i); fflush (stdout);
			}


    	/* tell kids to complete processing (image file generation) */
	printf ("#### Hotmomma telling kids to finish up\n"); fflush (stdout);
	for (i = 0 ; i < pidPtr ; i++)
	    	if (kids[i]) 
    	    	    	AssertState (mqid, MOMMA, i, stateFinish);
		
	/* all done */
	while( pid = *kids++ ) {
		waitpid( pid, NULL, 0);
	}	
}
		


