
/* saveResults -- step through all process modules, call the right stuff */

/* 
 *  $Log: saveResults.c,v $
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

static const char rcsid[] = "$Id: saveResults.c 172 2016-03-25 21:03:51Z stanley $";

#include "hotm.h"

void saveResults()
{
struct _processModule_ 	*this = firstProcessModule;

	while( this ) {
		(*this->saveResults)(this);
		this = this->next;
	}
}





