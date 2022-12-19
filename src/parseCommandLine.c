/* accept the command line parameters */

/* 
 *  $Log: parseCommandLine.c,v $
 *  Revision 1.8  2004/12/09 18:21:49  stanley
 *  bumped version number to 2
 *
 *  Revision 1.7  2002/10/31 20:52:52  stanley
 *  added camera number on command line
 *
 *  Revision 1.6  2002/08/08 23:12:36  stanleyl
 *  changed verbose from 0/1 to value
 *
 *  Revision 1.5  2002/03/23 00:24:27  stanley
 *  message passing code
 *
 *  Revision 1.4  2002/03/22 20:11:39  stanley
 *  added timelimit initialization
 *  loop on command files on command line
 *
 *  Revision 1.3  2002/03/18 19:21:25  stanley
 *  fix output format. dummy.
 *
 *  Revision 1.2  2002/03/16 02:18:21  stanley
 *  move baseEpoch determination here
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

static const char rcsid[] = "$Id: parseCommandLine.c 352 2018-08-15 23:42:25Z stanley $";

#include "hotm.h"
#define DEBUG (hotmParams.debug)
#define VERBOSE(x) (hotmParams.verbose & x)

void parseCommandLine( int argc, char **argv )
{

	char *module = "parseCommandLine";
	struct timeval start;

	hotmParams.debug = 
	hotmParams.verbose = 0;
	
	strcpy( hotmParams.commandFile, "" );
	strcpy( hotmParams.sitename, "unknown" );
	strcpy( hotmParams.programVersion, "hotm-2.0" );
	hotmParams.baseEpoch = 0;
	hotmParams.baseName[0] = 0;
	hotmParams.discard = 0;
	hotmParams.skip = 0;
	hotmParams.timeLimit = 0;
	
	hotmParams.mqid = 0;
	hotmParams.kidid = -1;
	hotmParams.cameraNumber = -1;

	strcpy( hotmParams.myName, *argv++ );
		
	while( *argv ) {
	
		switch (**argv) {
		
			case '-':
			
				if(!strcmp(*argv, "-d") ) {
					hotmParams.debug++;
					if DEBUG 
						printf("debug flag enabled\n");
				}
				else if(!strcmp(*argv, "-v") ) {
					argv++;
					sscanf( *argv, "%i", &hotmParams.verbose);
					if DEBUG
						printf("verbose flag set to 0x%x\n",
							   hotmParams.verbose);
				}
				else if(!strcmp(*argv, "-version" )) {
					argv++;
					strcpy( hotmParams.programVersion, *argv );
				}
				else if(!strcmp(*argv, "-s" ) ) {
					argv++;
					strcpy( hotmParams.sitename, *argv );
				}
				/* starting epoch time for outputs */
				else if(!strcmp(*argv, "-e" ) ) {
					argv++;
					hotmParams.baseEpoch = atol( *argv );
				}

				else if(!strcmp(*argv, "-p" ) ) {
					argv++;
					hotmParams.mqid = atoi (*argv);
					argv++;
					hotmParams.kidid = atoi (*argv);
					printf("hotm: will wait for hotmomma\n");
				}
				else if(!strcmp(*argv, "-c" ) ) {
					argv++;
					hotmParams.cameraNumber = atoi (*argv);
					if DEBUG
						printf("hotm: camera ID is %d\n",
								hotmParams.cameraNumber);
				}
				break;
					
			default:
				strcpy( hotmParams.commandFile, *argv );
				if DEBUG 
					printf("command file set to %s\n",
						hotmParams.commandFile ); 
				if VERBOSE(HVERB_COMMANDLINE) 
					printf("%s: loading command file: %s\n",
						module, hotmParams.commandFile );
				loadCommandFile();
				
		}
		
		argv++;
		
	}

	/* get starting epoch time, if not set by command line */
	if( 0 == hotmParams.baseEpoch ) {
		gettimeofday( &start, NULL );
		hotmParams.baseEpoch = start.tv_sec;
	}

	
	if VERBOSE(HVERB_COMMANDLINE) {
		printf("%s: My name is %s\n", module, hotmParams.myName );
		printf("%s: Verbose flag is 0x%x\n", module, hotmParams.verbose );
		printf("%s: Debug flag is %s\n", module, DEBUG? "set":"clear" );
		printf("%s: Starting epoch is %lu\n", module, hotmParams.baseEpoch );
	}
}
