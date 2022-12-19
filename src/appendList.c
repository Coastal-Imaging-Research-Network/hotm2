
/*  appendList -- simple linked list management 
 *  append the 'item' to the list headed by 'head'
 */

/* 
 *  $Log: appendList.c,v $
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

static const char rcsid[] = "$Id: appendList.c 172 2016-03-25 21:03:51Z stanley $";

#include <stdio.h>
 
struct GENERICLIST {
 	void *next;
} ;
 
void appendList( 	struct GENERICLIST **list, 
 					struct GENERICLIST *item )
{
 
struct GENERICLIST *ptr = *list;
 
 	/* null list means I'm entering the first item */
 	if( NULL == ptr ) {
 			(*list) = item;
 			(*list)->next = NULL;
 			return;		
 	}
 	
 	/* if 'next' is null, I'm at the end of the list */
 	while( NULL != ptr->next )
		ptr = ptr->next;
		
	/* at end now, add item */
	ptr->next = item;
	
	/* all done */
	
}
 		
