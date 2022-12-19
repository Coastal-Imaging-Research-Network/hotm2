
/* processLoop -- step through all process modules, call the right stuff */

/* 
 *  $Log: processLoop.c,v $
 *  Revision 1.5  2003/10/05 23:01:34  stanley
 *  removed endif text
 *
 *  Revision 1.4  2002/08/08 23:49:35  stanleyl
 *  parameterize verbosity, report kidid
 *
 *  Revision 1.3  2002/03/22 20:39:23  stanley
 *  added timelimit
 *
 *  Revision 1.2  2002/03/16 02:18:01  stanley
 *  move baseEpoch determination to parsecommandline
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

static const char rcsid[] = "$Id: processLoop.c 419 2018-11-20 18:13:35Z stanley $";

#include "hotm.h"

void processLoop()
{
struct _processModule_ 	*this;
struct _pixel_ 		pix;
struct _frame_ 		*frame;
int 			active = 1;
struct timeval		start, end;
struct timeval		limitStart;
long			ptr;
unsigned long		mySize;
unsigned long		consumed;
struct timeval		lastFrame;
unsigned long           diff;
long			skip;

	mySize = cameraModule.x * cameraModule.y;

	gettimeofday( &lastFrame, NULL );
	
	/* get the starting time of the processing for timeLimit */
	limitStart = lastFrame;

	/* loop through while active */
	while( active ) {
	
		/* reset active */
		active = 0;
			
		skip = hotmParams.skip;
		
		while( skip && !hotmParams.discard ) {
			frame = (*cameraModule.getFrame)(&cameraModule);
			(*cameraModule.releaseFrame)(); 
			skip--;
		}

		/* get a frame! */
		frame = (*cameraModule.getFrame)(&cameraModule);

		diff = (frame->when.tv_sec-lastFrame.tv_sec) * 1000000
		    +   (frame->when.tv_usec-lastFrame.tv_usec);

		if(HVERBOSE(HVERB_PROCESSIN)) {
			 KIDID;
			 printf("Frame: cam %ld: time: %ld.%06ld diff: %lu us\n", 
					cameraModule.cameraNumber,
					frame->when.tv_sec,
					frame->when.tv_usec,
					diff );
		}
		
		memcpy( &lastFrame, &frame->when, sizeof(struct timeval));
			
		/* if discarding frames, just toss them now! */
		if( hotmParams.discard ) { 
			(*cameraModule.releaseFrame)(); 
			active++; 
			hotmParams.discard--;
			continue; 
		}
		
		/* time the processing loop */
		gettimeofday( &start, NULL );
		
		/* loop through frame processes */
		this = firstProcessModule;
		
		while( this ) {
		
			if( this->processFrame && this->remaining ) {
			
				/* special flag: -1 remaining: don't count this as active */
				if( this->remaining > 0 ) active++;	
			
				(*this->processFrame)(this, frame);
			
				if( HVERBOSE(HVERB_PROCESSTIMES) ) {
					KIDID;
					printf("cam %ld: %s: time: %lu micros remain: %ld\n",
						cameraModule.cameraNumber,
						this->module,
						this->processMicros,
						this->remaining );
				}
					
			}
			
			this = this->next;
		}
		
#ifdef maybesomday
		/* now loop through pixel processes */
		for( ptr = 0; ptr < mySize; ptr++ ) {

			pix = frame->dataPtr[ptr];
			this = firstProcessModule;
		
			while( this ) {
				if( this->processPixel && this->remaining ) {
					active++;
					(*this->processPixel)(this, pix, ptr );
				}
				this = this->next;
			}
		}
		
		/* one last pass, closeFrame for anyone needing it */
		this = firstProcessModule;
		while( this ) {
			if( this->closeFrame && this->remaining ) {
				(*this->closeFrame)(this);
				if( this->verbose ) {
					printf("%s: time: %d ms\n",
						this->module,
						this->processMicros/1000 );
					this->processMicros = 0;
				}
			}
			this = this->next;
		}
#endif 
		
		(*cameraModule.releaseFrame)();	
				
		gettimeofday( &end, NULL );
		consumed = (end.tv_sec - start.tv_sec) * 1000000
			+ (end.tv_usec - start.tv_usec);
		if( HVERBOSE(HVERB_PROCESSOUT) ) {
			KIDID;
			printf("processLoop: cam %ld: consumed %lu micros\n",
					cameraModule.cameraNumber,
					consumed );
		}
			
		/* if we have a limit on time to run ...*/
		if( hotmParams.timeLimit ) {
			
			/* see if we have reached it */
			if( (end.tv_sec-limitStart.tv_sec) > hotmParams.timeLimit ) {
				active = 0;
				KIDID;
				printf("processLoop: time limit (%ld) exceeded, ending\n",
						hotmParams.timeLimit );
			}
		}
						
	}
}


