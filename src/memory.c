
/* memory.c -- malloc and lock and init the memory needed by
    data collection arrays. */
	
/* 
 *  $Log: memory.c,v $
 *  Revision 1.2  2002/08/09 00:16:03  stanleyl
 *  parameterize verbosity report
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

static const char rcsid[] = "$Id: memory.c 353 2018-08-15 23:42:37Z stanley $";

    
#include "hotm.h"
    
unsigned long *memoryLock( unsigned long size, char *name, char *module )
{

unsigned long *ptr;
int stat; 

	ptr = (unsigned long *)malloc( size );
	if( ptr == NULL ) {
		fprintf(stderr, 
			"failed to malloc %s -- giving up!\n",
			name );
		exit(-1);
	}
	
	stat = mlock( ptr, size );
	if( stat & (errno != 1) ) {
		fprintf(stderr,
			"%s: failed to lock %s (%d)\n",
			module, 
			name,
			errno );
		perror( module );
	}
	
	memset( ptr, 0, size );

	if(HVERBOSE(HVERB_MEMORY)) {
		KIDID;
		printf("memorylock: %s: variable %s: length: %ld addr: %08lx\n", 
				module, 
				name, 
				size, 
				(unsigned long) ptr );
	}
	
	return ptr;

}
	
