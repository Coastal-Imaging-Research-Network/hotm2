
/* 
 *  $Log: message.c,v $
 *  Revision 1.1  2002/03/23 00:24:35  stanley
 *  Initial revision
 *
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

static const char rcsid[] = "$Id: message.c 172 2016-03-25 21:03:51Z stanley $";

#include "hotm.h"

int
AllocMessageQueue (
    ) {

    int     mqid;

    if ((mqid = msgget (IPC_PRIVATE, 0600)) < 0) {
	perror ("Cannot allocate message queue");
	exit (1);
	}

    return mqid;
    }


void
FreeMessageQueue (
    int     mqid
    ) {

    if ((msgctl (mqid, IPC_RMID, NULL)) < 0) {
	perror ("Cannot deallocate message queue");
	exit (1);
	}
    }


int
ExpectState (
    int     mqid,
    int     sender,
    int     receiver,
    int     state
    ) {

    long    channel = ((sender & 0xFF) << 8) | (receiver & 0xFF);
    struct _msgbuf_ msgbuf;
    size_t  s;

    s = msgrcv (mqid, &msgbuf, sizeof (int), channel, 0);
    if (sizeof (int) != s) {
	perror ("Got short message");
	exit (1);
	}

    if (channel != msgbuf.channel) {
	perror ("Whoops, UNIX made a booboo!");
	exit (1);
	}

    if (msgbuf.state != state) {
    	if (stateExit == state)
	    return -1;

	perror ("Got unexpected state in message");
	exit (1);
	}
    
    return 1;
    }


void
AssertState (
    int     mqid,
    int     sender,
    int     receiver,
    int     state
    ) {

    long    channel = ((sender & 0xFF) << 8) | (receiver & 0xFF);
    struct _msgbuf_ msgbuf;
    size_t  s;

    msgbuf.channel = channel;
    msgbuf.state = state;

    if (msgsnd (mqid, &msgbuf, sizeof (int), 0) != 0) {
	perror ("Cannot send message");
	exit (1);
	}
    }


