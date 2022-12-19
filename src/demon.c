/* demon.c -- write data to disk when asked
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
 * $Log: demon.c,v $
 * Revision 1.18  2011/11/10 00:42:51  root
 * larger pix list for AI for 5MP, write in chunks to cover failure to write atomically
 *
 * Revision 1.17  2010/09/16 16:55:55  stanley
 * changed shmget to use struct instead of array parts. for 64 bit os
 *
 * Revision 1.16  2010/04/02 19:14:55  stanley
 * removed dc1394 dependence, changed module name from disk to demon
 *
 * Revision 1.15  2010/01/13 00:22:13  stanley
 * added new stack switching ISNEWSTACK
 *
 * Revision 1.14  2010/01/12 01:57:58  stanley
 * add error check on input UV -- clip to edges.
 *
 * Revision 1.13  2009/06/16 21:00:59  stanley
 * added shm
 * ,
 *
 * Revision 1.12  2009/02/20 21:10:11  stanley
 * many changes, fix readAOI issue of not closing file, add other stuff.
 *
 * Revision 1.11  2006/04/14 23:36:32  stanley
 * added stop/gostack
 *
 * Revision 1.10  2005/03/25 19:31:08  stanley
 * upped shutter change to 10% to avoid sticking at .003 sec or so
 *
 * Revision 1.9  2005/03/22 21:33:34  stanley
 * fixed cleanslot
 *
 * Revision 1.8  2005/03/17 20:26:18  stanley
 * added 'masquerade' so one camera can be another
 *
 * Revision 1.7  2005/01/21 20:48:59  stanley
 * micropix/flea/dragon has x width, actually x wide lines, fixed offsetTest
 *
 * Revision 1.6  2004/12/17 21:25:16  stanley
 * changed min-gain for micropix bug
 *
 * Revision 1.5  2004/12/11 00:59:33  stanley
 * modify to deal with _setCamera/_getCamera and real shutter gain
 *
 * Revision 1.4  2004/12/06 21:46:07  stanley
 * changed autogain calculation
 *
 * Revision 1.3  2004/03/17 00:52:27  stanley
 * added log to header
 *
 *
 */

static const char rcsid[] =
     "$Id: demon.c 172 2016-03-25 21:03:51Z stanley $";

#include "demon.h"

extern int errno;

/*#define XXX(x)  fprintf(logFD, x);fflush(logFD);*/
#define XXX(x)

struct _diskdata_ diskdata[CONCURRENT_ACTIONS];

extern void stackInit (struct _diskdata_ *);
extern void stackWrite (struct _diskdata_ *, struct _frame_ *);

struct timeval lastFrameSeen;
double frameInterval;

key_t msgIn;
int msgId;
int msgAnswer;                  /* for acks to transient commands */

struct
{
     long type;
     key_t replyKey;            /* sent as part of the message by client */
     char bufc[2048];
} msgBuf;

#include <sys/shm.h>
struct ihPTR
{
     int sem;
     struct _imageHeader_ ih;
};

struct
{
     key_t key;
     int shmid;
     struct ihPTR *ih;
} ihSHM;

struct dataPTR
{
     int sem;
     char data;
};
struct
{
     key_t key;
     int shmid;
     struct dataPTR *data;
} dataSHM;

struct
{
     key_t key;
     int shmid;
     struct _cameraModule_ *cm;
} cmSHM;

unsigned long MSC;              /* fake MSC to help skip frames */
struct
{
     long high;
     long low;
     long skip;
     double intLimit;
     int noAutoOnStacks;
     int noAutoWhenCollecting;
     unsigned long pixList[6000000];
} autoShutter;

unsigned long doOffsetTest;
double blockAverage;
long doThisManyRaw;

FILE *logFD;

void
msgOUT (int msgQue, long type, char *text)
{
     struct
     {
          long type;
          key_t replyKey;
          char bufc[2048];
     } myBuf;

     myBuf.type = type;
     strcpy (myBuf.bufc, text);
     myBuf.replyKey = msgIn;

     msgsnd (msgQue, &myBuf, strlen (myBuf.bufc) + sizeof (key_t),
             IPC_NOWAIT);
     fprintf (logFD, "%s\n", text);

}

#define msgOK(y) (msgOUT(msgAnswer, 1, y))
#define msgDone(y) (msgOUT(msgAnswer, 2, y))
#define msgBAD(y) (msgOUT(msgAnswer,999,y))
#define msgSTATUS(y) (msgOUT(msgAnswer, 3, y))

#define freeAndClear(x)  if(x) { free(x); x = NULL; }

void
newAutoList (void)
{

     unsigned int i;
     unsigned int j;

     for (j = 0, i = 1;
          (i < cameraModule.bufferLength) & (j <
                                             sizeof (autoShutter.pixList) /
                                             sizeof (unsigned long) - 1);
          i += autoShutter.skip, j++) {
          autoShutter.pixList[j] = i;
     }
     autoShutter.pixList[j] = 0;
     return;
}

void
AElist (int minX, int minY, int maxX, int maxY, int skip)
{

     int x = minX;
     int y = minY;
     unsigned long ptr;
     int i;
     int mult;
     int offset;


     if (skip == -1) {          /* autoskip */

          ptr = (maxX - minX) * (maxY - minY);
          skip = ptr / 1500;
          if (skip == 0)
               skip = 1;

     }

     for (i = 0; i < 2047; i++) {

          ptr = cameraModule.x * y + x;
          switch (cameraModule.format) {

               case formatRGB8:
                    mult = 3;
                    offset = 1; /* rGb */
                    break;
               case formatYUV444:
                    mult = 3;
                    offset = 0; /* YCbCr */
                    break;
               case formatYUV422:
                    mult = 2;
                    offset = 1; /* CbYCrY */
                    break;
               case formatRGB16:       /* unsupported, but keep from crash */
                    mult = 6;
                    offset = 2;
                    break;
               case formatMONO8:
                    mult = 1;
                    offset = 1;
                    break;
               case formatMONO16:      /* unsupported, but keep from crash */
                    mult = 2;
                    offset = 0;
                    break;
          }

          autoShutter.pixList[i] = ptr * mult + offset;

          x += skip;
          while (x > maxX) {
               y++;
               x = minX + x - maxX;
          }
          if (y > maxY)
               break;

     }
     autoShutter.pixList[i] = 0;
     return;
}

void
sonyAEspot (int spot)
{

     switch (spot) {

          case 0:              /* all screen */
               AElist (1, 1, cameraModule.x - 1, cameraModule.x - 1, -1);
               break;
          case 1:              /* center small */
               AElist ((cameraModule.x / 2) - 40,
                       (cameraModule.y / 2) - 40,
                       (cameraModule.x / 2) + 40,
                       (cameraModule.y / 2) + 40, -1);
               break;
          case 2:              /* left down */
               AElist (1,
                       (cameraModule.y / 2),
                       (cameraModule.x / 2), cameraModule.y - 1, -1);
               break;
          case 3:              /* right down */
               AElist ((cameraModule.x / 2),
                       (cameraModule.y / 2),
                       cameraModule.x - 1, cameraModule.y - 1, -1);
               break;
          case 4:              /* lower center */
               AElist ((cameraModule.x / 2) - 40,
                       cameraModule.y - 80,
                       (cameraModule.x / 2) + 40, cameraModule.y - 1, -1);
               break;
          case 5:              /* left up */
               AElist (1, 1, (cameraModule.x / 2), (cameraModule.y / 2), -1);
               break;
          case 6:              /* right up */
               AElist ((cameraModule.x / 2),
                       (cameraModule.y / 2),
                       cameraModule.x - 1, cameraModule.y - 1, -1);
               break;
          case 7:              /* center big */
               AElist ((cameraModule.x / 2) - 100,
                       (cameraModule.y / 2) - 100,
                       (cameraModule.x / 2) + 100,
                       (cameraModule.y / 2) + 100, -1);
               break;
          default:
               break;
     }
}

/* add switching for new/old stack format */
#define ISNEWSTACK  strcmp( "ras", &p->name[strlen(p->name)-3] )

void
getCommand (void)
{

     char line[1024];
     int i;
     int j;
     double dtemp;
     long ltemp;
     char *token;
     char *ttemp;
     char *ttemp2;
     int tempFD;
     int msgSize;
     struct _diskdata_ *p;
     unsigned short *pi;
     unsigned short U, V;
     double dU, dV;
     char openAOImyself = 0;
     FILE *aoi;
     struct _pixlist_ *pPtr;

     /* handle pending responses, like 'offsetTest' */
     /* warning: assumes we have had NO intermediate commands! */
     /* i.e., msgAnswer is still valid */
#ifdef OFFSETYES
     if (doOffsetTest) {
          sprintf (line, "offsetTest: %d", doOffsetTest);
          msgOK (line);
          doOffsetTest = 0;
     }
#endif

     if (blockAverage) {
          sprintf (line, "blockAverage: %f", blockAverage);
          msgOK (line);
          blockAverage = 0;
     }

     msgSize = msgrcv (msgId, &msgBuf, 2052, 0, IPC_NOWAIT);
     if (msgSize > 0) {

          msgBuf.bufc[msgSize - sizeof (key_t)] = 0;
          /* create a response key for ack */
          msgAnswer = msgget (msgBuf.replyKey, 0);

          /* sort out the message types */
          if (msgBuf.type == 6) {
               /* type 6 -- autoshutter pixel list */
               /* a set of unsigned long indexes into image */
               if (msgSize == sizeof (key_t))   /* no list, replace old */
                    newAutoList ();
               else {
                    memcpy (autoShutter.pixList, msgBuf.bufc,
                            msgSize - sizeof (key_t));
                    /* ensure a 0 at the end */
                    autoShutter.pixList[(msgSize / 4)] = 0;
               }
               msgOK ("new autoshutter list");
               return;
          }

          if (msgBuf.type == 5) {
               /* type 5 is 'pixlist input'. will have raw binary */

               /* find the slot for this data */
               for (i = 0; i < CONCURRENT_ACTIONS; i++) {
                    p = &diskdata[i];
                    if (p->fd == -1)
                         continue;
                    if (p->msgBack == msgAnswer)
                         break;
               }
               if (i == CONCURRENT_ACTIONS) {
                    sprintf (line,
                             "getCommand type 5 no slot matches key %x",
                             msgAnswer);
                    msgBAD (line);
                    return;
               }

               /* ok, p points to slot, get count of input UV */
               i = (msgSize - sizeof (key_t)) / sizeof (unsigned short);
               if (i == 0) {    /* pixel list done! */
                    if (p->pixDone == 0) {
                         p->pixDone = 1;
                         msgDone ("pixlist accepted");
                         ISNEWSTACK ? newStackInit (p) : stackInit (p);
                    }
                    return;
               }
               if (i % 2) {
                    msgBAD ("odd number of numbers in UV");
                    return;
               }

               pi = (unsigned short *) msgBuf.bufc;
               for (; i > 0; i -= 2) {
                    U = *pi++;
                    V = *pi++;
                    if (p->stackDeBayer) {
#define DOTYPE 	if( U%2 ) { if(V%2) p->pPtr->type = 5; else p->pPtr->type = 6; } \
		else      { if(V%2) p->pPtr->type = 4; else p->pPtr->type = 7; }
                         DOTYPE;
                    }
                    p->pPtr->pixel = (U - 1) + (V - 1) * cameraModule.x;
                    p->pPtr++;
               }
               msgOK ("pixlist go ahead");

               return;
          }

          if (msgBuf.type != 1) {
               sprintf (line, "unknown type: %d for msg: %s\n",
                        msgBuf.type, msgBuf.bufc);
               msgBAD (line);
               return;
          }

          fprintf (logFD, "getCommand read %d bytes, as: %s\n",
                   strlen (msgBuf.bufc), msgBuf.bufc);

          /* have command, lets parse it */
          /* 'add outputname numberofframes skip aoifile pid' */
          token = strtok (msgBuf.bufc, " ");
          if (!strcasecmp ("add", token)) {

               /* first, find empty diskdata struct to use */
               for (i = 0; i < CONCURRENT_ACTIONS; i++) {
                    if (diskdata[i].fd == -1)
                         break;
               }
               if (i == CONCURRENT_ACTIONS) {
                    msgBAD ("too many actions going on, ignoring");
                    return;
               }
               p = &diskdata[i];

               /* get output destination */
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing output from add command");
                    return;
               }
               strcpy (p->name, token);

               /* get number of frames */
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing number of frames from add command");
                    return;
               }
               p->count = strtol (token, NULL, 0);

               /* get skip count */
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing skip from add command");
                    return;
               }
               p->skip = strtol (token, NULL, 0);

               /* get start time */
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing start time from add command");
                    return;
               }
#ifdef STUPID2HZASSUMPTIONDARNMEG
               p->begin = strtol (token, NULL, 0)
                    +
                    (((lastFrameSeen.tv_usec + 250000) % 500000) / 1000000.0);
#else
               {
                    long pics;
                    p->begin = strtol (token, NULL, 0);
                    pics = 1 +
                         ((p->begin - lastFrameSeen.tv_sec) -
                          lastFrameSeen.tv_usec / 1000000.0)
                         / frameInterval;
                    p->begin = 1.0 * lastFrameSeen.tv_sec +
                         lastFrameSeen.tv_usec / 1000000.0;
                    p->begin += pics * frameInterval - frameInterval / 2;
               }
#endif

               p->MSC = 0;

               /* get aoifile, says this is stack if not '0' */
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing pixcount from add command");
                    return;
               }

               p->pixDone = 0;
               p->pixelCount = strtol (token, NULL, 0);
               if (p->pixelCount) {

                    if (p->pixelCount < 0) {
                         openAOImyself = 1;
                         p->pixelCount = -1 * p->pixelCount;
                    }
                    if (p->pixelCount % 2)      /* odd length */
                         p->pixelCount++;       /* make even -- rasterfile */
                    p->pixlist = p->pPtr =
                         calloc (p->pixelCount, sizeof (struct _pixlist_));
                    if (p->pixlist == NULL) {
                         msgBAD ("cannot malloc pixlist");
                         return;
                    }
                    p->pixOutLine = calloc (p->pixelCount * 3 + 8, 1);
                    if (p->pixOutLine == NULL) {
                         msgBAD ("cannot malloc pixOutput");
                         freeAndClear (p->pixlist);
                         return;
                    }
                    /* later type 5 messages will fill in the pixlist */


                    /* get aoifilename -- so pixel tools can use this stack */
                    token = strtok (NULL, " ");
                    if (token == NULL) {
                         msgBAD ("no aoifilename");
                         freeAndClear (p->pixlist);
                         freeAndClear (p->pixOutLine);
                         return;
                    }
                    strcpy (p->aoiFilename, token);

                    /* last, debayer stack flag */
                    token = strtok (NULL, " ");
                    if (token == NULL)
                         p->stackDeBayer = 0;
                    else
                         p->stackDeBayer = 1;

                    /* now, open aoi file if asked to */
                    if (openAOImyself) {
                         aoi = fopen (p->aoiFilename, "r");
                         if (NULL == aoi) {
                              msgBAD ("cannot open aoifile");
                              freeAndClear (p->pixlist);
                              freeAndClear (p->pixOutLine);
                              return;
                         }
                         i = p->pixelCount;
                         while (i--) {
                              fscanf (aoi, "%lf %lf", &dU, &dV);
                              /* protect myself against bogus pixels */
                              /* leave in place but clip to edge */
                              if (dU < 1)
                                   dU = 1;
                              if (dV < 1)
                                   dV = 1;
                              if (dU > cameraModule.x)
                                   dU = cameraModule.x;
                              if (dV > cameraModule.y)
                                   dV = cameraModule.y;
                              U = dU;
                              V = dV;
                              if (p->stackDeBayer) {
                                   DOTYPE;
                              }
                              p->pPtr->pixel =
                                   (U - 1) + (V - 1) * cameraModule.x;
                              p->pPtr++;
                         }
                         p->pixDone = 1;
                         fclose (aoi);
                    }

               }

               /* pid of requestor is replyKey from message */
               p->requestor = msgBuf.replyKey;
               p->msgBack = msgAnswer;
               p->size = cameraModule.bufferLength;

               /* open output */
               p->fd =
                    open (p->name, O_WRONLY | O_CREAT | O_NDELAY,
                          S_IRWXU | S_IRWXG);
               if (p->fd == -1) {
                    freeAndClear (p->pixlist);
                    freeAndClear (p->pixOutLine);
                    perror ("add command open file");
                    msgBAD ("cannot open output");
                    return;
               }

               if (p->pixlist && openAOImyself)
                    ISNEWSTACK ? newStackInit (p) : stackInit (p);

               /* only write the header if we aren't a stack! */
               if (p->pixlist == NULL) {
                    cameraModule.frameCount = p->count;

#ifdef IWEGEIJ
                    ltemp = sizeof (hotmParams);
                    write (p->fd, &ltemp, sizeof ltemp);
                    write (p->fd, &hotmParams, sizeof hotmParams);
                    ltemp = sizeof (cameraModule);
                    write (p->fd, &ltemp, sizeof ltemp);
                    write (p->fd, &cameraModule, sizeof cameraModule);
#endif
                    initRaw (p->fd, p);
               }

               sprintf (line,
                        "added output to file %s for %d frames of size %d requestor %d at %f\n",
                        p->name, p->count, p->size, p->requestor, p->begin);
               msgOK (line);

          }
          else if (!strcasecmp ("endAll", token)) {
               for (i = 0; i < CONCURRENT_ACTIONS; i++)
                    diskdata[i].count = 1;
               msgOK ("endAll");
          }

          else if (!strcasecmp ("openSHM", token)) {

               /* enable shared memory, three areas */
               ihSHM.key = 0x100 + cameraModule.cameraNumber;
               dataSHM.key = 0x200 + cameraModule.cameraNumber;
               cmSHM.key = 0x300 + cameraModule.cameraNumber;
               ihSHM.shmid = shmget (ihSHM.key,
                                     sizeof (struct ihPTR), 0644 | IPC_CREAT);
               dataSHM.shmid = shmget (dataSHM.key,
                                       cameraModule.bufferLength +
                                       sizeof (int), 0644 | IPC_CREAT);
               cmSHM.shmid =
                    shmget (cmSHM.key, sizeof (struct _cameraModule_),
                            0644 | IPC_CREAT);
               ihSHM.ih = (struct ihPTR *) shmat (ihSHM.shmid, 0, 0);
               dataSHM.data = (struct dataPTR *) shmat (dataSHM.shmid, 0, 0);
               cmSHM.cm = (struct _cameraModule_ *) shmat (cmSHM.shmid, 0, 0);

               ihSHM.ih->sem = 0;
               dataSHM.data->sem = 0;
               memcpy ((struct _cameraModule_ *) cmSHM.cm, &cameraModule,
                       sizeof (struct _cameraModule_));
               msgOK ("openSHM");

          }

          else if (!strcasecmp ("closeSHM", token)) {
               if (ihSHM.shmid) {
                    shmdt (ihSHM.ih);
                    shmctl (ihSHM.shmid, IPC_RMID, NULL);
                    ihSHM.shmid = 0;
               }
               if (dataSHM.shmid) {
                    shmdt (dataSHM.data);
                    shmctl (dataSHM.shmid, IPC_RMID, NULL);
                    dataSHM.shmid = 0;
               }
               if (cmSHM.shmid) {
                    shmdt (cmSHM.cm);
                    shmctl (cmSHM.shmid, IPC_RMID, NULL);
                    cmSHM.shmid = 0;
               }
               msgOK ("closeSHM");
          }

          else if (!strcasecmp ("stopStack", token)) {
               for (i = 0; i < CONCURRENT_ACTIONS; i++)
                    if (diskdata[i].pixDone == 1)
                         diskdata[i].pixDone = 2;
               msgOK ("stopStack");
          }

          else if (!strcasecmp ("goStack", token)) {
               for (i = 0; i < CONCURRENT_ACTIONS; i++)
                    if (diskdata[i].pixDone == 2)
                         diskdata[i].pixDone = 1;
               msgOK ("goStack");
          }

          else if (!strcasecmp ("exit", token)) {
               msgOK ("exiting at your command");
               if (ihSHM.shmid) {
                    shmdt (ihSHM.ih);
                    shmctl (ihSHM.shmid, IPC_RMID, NULL);
                    ihSHM.shmid = 0;
               }
               if (dataSHM.shmid) {
                    shmdt (dataSHM.data);
                    shmctl (dataSHM.shmid, IPC_RMID, NULL);
                    dataSHM.shmid = 0;
               }
               if (cmSHM.shmid) {
                    shmdt (cmSHM.cm);
                    shmctl (cmSHM.shmid, IPC_RMID, NULL);
                    cmSHM.shmid = 0;
               }
               exit (0);
          }

          else if (!strcasecmp ("ping", token)) {
               msgOK ("pong");
          }

          else if (!strcasecmp ("noautoonstacks", token)) {
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing parameter on noautoonstacks");
                    return;
               }
               autoShutter.noAutoOnStacks = atol (token);
               msgOK ("noautoonstacks");
          }
          else if (!strcasecmp ("masquerade", token)) {
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing parameter on masquerade");
                    return;
               }
               cameraModule.cameraNumber = atol (token);
               msgOK ("masquerade");
          }
          else if (!strcasecmp ("setcamera", token)) {
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing action for setcamera");
                    return;
               }
               ttemp = strtok (NULL, " ");
               if (ttemp == NULL) {
                    msgBAD ("missing feature for setcamera");
                    return;
               }
               ttemp2 = strtok (NULL, " ");
               if (ttemp2 == NULL) {
                    msgBAD ("missing value for setcamera");
                    return;
               }
               (*cameraModule.setCamera) (token, ttemp, atof (ttemp2));
               msgOK ("setcamera");
          }
          else if (!strcasecmp ("restart", token)) {
               (*cameraModule.stopCamera) (&cameraModule);
               (*cameraModule.initCamera) (&cameraModule);
               msgOK ("restart");
          }
          else if (!strcasecmp ("autoShutter", token)) {
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing low value for autoShutter");
                    return;
               }
               autoShutter.low = strtol (token, NULL, 0);
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing high value for autoShutter");
                    return;
               }
               autoShutter.high = strtol (token, NULL, 0);
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing skip value for autoShutter");
                    return;
               }
               autoShutter.skip = strtol (token, NULL, 0);
               newAutoList ();
               msgOK ("autoShutter");
          }
          else if (!strcasecmp ("intLimit", token)) {
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing value for intLimit");
                    return;
               }
               autoShutter.intLimit = strtod (token, NULL);
               msgOK ("intLimit");
          }
          else if (!strcasecmp ("flushLog", token)) {
               fclose (logFD);
               sprintf (line, "/tmp/cam%02dd.log", cameraModule.cameraNumber);
               logFD = fopen (line, "w");
               if (NULL == logFD) {
                    perror ("init log open");
               }
               msgOK ("flushLog");
          }

          else if (!strcasecmp ("dumpPixlist", token)) {
               /* dump all pixlists to log */
               for (i = 0; i < CONCURRENT_ACTIONS; i++) {
                    p = &diskdata[i];
                    if (p->fd == -1)
                         continue;
                    if (p->pixlist == NULL)
                         continue;
                    fprintf (logFD, "Slot %d current pixlist\n", i);
                    for (j = 0; j < p->pixelCount; j++)
                         fprintf (logFD, "  %5d:  %7d %2d\n", j,
                                  p->pixlist[j].pixel, p->pixlist[j].type);
               }
               msgOK ("dumpPixlist");
          }

          else if (!strcasecmp ("cleanSlot", token)) {
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing value for cleanSlot");
                    return;
               }
               i = strtol (token, NULL, 0);
               if (i < 0 || i > CONCURRENT_ACTIONS) {
                    msgBAD ("invalid slot");
                    return;
               }
               p = &diskdata[i];
               if (p->fd)
                    close (p->fd);
               freeAndClear (p->pixlist);
               freeAndClear (p->pixOutLine);
               memset (p, 0, sizeof (struct _diskdata_));
               p->fd = -1;
               msgOK ("cleanSlot");
          }

          else if (!strcasecmp ("noAutoWhenCollecting", token)) {
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing value for noAutoWhenCollecting");
                    return;
               }
               autoShutter.noAutoWhenCollecting = strtol (token, NULL, 0);
               msgOK ("noAutoWhenCollecting");
          }

          else if (!strcasecmp ("takeRaw", token)) {
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing value for takeRaw");
                    return;
               }
               doThisManyRaw = strtol (token, NULL, 0);
               msgOK ("takeRaw");
          }

          else if (!strcasecmp ("pauseRaw", token)) {
               doThisManyRaw = 0;
               msgOK ("pauseRaw");
          }

          else if (!strcasecmp ("AEspot", token)) {
               token = strtok (NULL, " ");
               if (token == NULL) {
                    msgBAD ("missing spot");
                    return;
               }
               i = strtol (token, NULL, 0);
               sonyAEspot (i);
               msgOK ("AEspot");
          }

#ifdef OFFSETYES
          else if (!strcasecmp ("offsettest", token)) {
               doOffsetTest++;
          }
#endif

          else if (!strcasecmp ("blockaverage", token)) {
               blockAverage++;
          }

          else if (!strcasecmp ("dumpAutoList", token)) {
               fprintf (logFD, "Autoshutter pixlist:\n");
               for (i = 0; autoShutter.pixList[i]; i++)
                    fprintf (logFD, " %4d: %d\n", i, autoShutter.pixList[i]);
               msgOK ("dumped autolist");
          }

          else if (!strcasecmp ("status", token)) {

               sprintf (line, "camera %d active and alive, MSC at %ld",
                        cameraModule.cameraNumber, MSC);
               msgSTATUS (line);

               sprintf (line,
                        "current gain: %d gainVal: %.6f shutter: %d intTime: %.6f",
                        cameraModule.gain, cameraModule.realGain,
                        cameraModule.shutter, cameraModule.intTime);
               msgSTATUS (line);

               sprintf (line,
                        "autoShutter is %s, limits %d:%d int limit: %f, skip %d",
                        autoShutter.high ? "active" : "inactive",
                        autoShutter.high, autoShutter.low,
                        autoShutter.intLimit, autoShutter.skip,
                        frameInterval);
               msgSTATUS (line);

               sprintf (line, "frameInterval: %f %s %s",
                        frameInterval,
                        autoShutter.noAutoOnStacks ? "noAutoOnStacks" : "",
                        autoShutter.
                        noAutoWhenCollecting ? "noAutoWhenCollecting" : "");
               msgSTATUS (line);

               if (doThisManyRaw == -1) {
               }
               else {
                    sprintf (line, "doThisManyRaw: %d", doThisManyRaw);
                    msgSTATUS (line);
               }

               msgSTATUS ("slotlist follows");

               for (i = 0; i < CONCURRENT_ACTIONS; i++) {
                    if (diskdata[i].fd == -1)
                         continue;
                    sprintf (line,
                             "slot %2d: %5d more (skip %d) (start %f) for pid %5d to %s%s",
                             i,
                             diskdata[i].count,
                             diskdata[i].skip,
                             diskdata[i].begin,
                             diskdata[i].requestor,
                             diskdata[i].name,
                             diskdata[i].pixlist ==
                             NULL ? "" : " with pixlist");
                    msgSTATUS (line);
               }
               msgDone ("end");
               return;
          }
          else if (!strcasecmp ("lastSeen", token)) {

               sprintf (line, "lastSeen: %d %06d",
                        lastFrameSeen.tv_sec, lastFrameSeen.tv_usec);
               msgOK (line);
          }
          else {
               sprintf (line, "bad command: %s", token);
               msgBAD (line);
          }
     }
}

void
_saveResults (struct _processModule_ *me)
{

}


void
_processFrame (struct _processModule_ *me, struct _frame_ *frame)
{
     struct timeval start;
     struct timeval end;
     unsigned char *ptr;
     int err;
     int i;
     int maxY;
     long histY;
     long shutter;
     int showChange;
     double now;
     static double previous;
     static double reportTime;
     char line[128];
     struct _diskdata_ *p;
     struct _imageHeader_ ih;
     int doingStack;
     int doingAnything;
     long oldGain;

     XXX ("entering processFrame\n");

     MSC++;
     showChange = 0;
     doingAnything = doingStack = 0;

     err = gettimeofday (&start, NULL);
     if (err)
          perror (me->module);

     ptr = frame->dataPtr;

     lastFrameSeen = frame->when;
     now = 1.0 * frame->when.tv_sec + (frame->when.tv_usec / 1000000.0);

     /* test for a frame that is more than almost two frame intervals */
     /* from last. This means at least one frame is dropped. */
     if (((now - previous) > (1.8 * frameInterval))
         && (previous > 0)) {

          /* ok, first, how many lost? */
          i = ((now - previous) + (frameInterval / 2.0)) / frameInterval - 1;

          /* whine about it */
          fprintf (logFD,
                   "**** DROPPED FRAME: missed %d frames prior to %f\n",
                   i, now);
     }


     if (previous == 0) {
          previous = now;
          frameInterval = .5;   /* assume 2Hz, corrected after two frames arrive */
     }
     else {
          frameInterval = now - previous;
     }


     /* for each of the diskdata actions... */
     for (i = 0; i < CONCURRENT_ACTIONS; i++) {

          p = &diskdata[i];

          /* nothing here to do, move along... */
          if (-1 == p->fd)
               continue;

          /* check for begin time */
          if (now < p->begin)
               continue;

          if (p->pixlist && (p->pixDone != 1))
               continue;

          XXX ("after test for pixdone\n");

          /* check for stacks -- no autoshutter maybe */
          if (p->pixlist)
               doingStack++;

          /* are we doing anything? no autoshutter, maybe */
          doingAnything++;

          /* check for skip */
          if (p->skip)
               if ((p->MSC++) % (p->skip + 1))
                    continue;

          /* doThisManyRaw: -1: free collection */
          /*  0: collection is paused */
          /*  +n: collect n frames and then pause */
          if (0 == doThisManyRaw)
               continue;


          if (p->pixlist) {
               XXX ("calling stackwrite\n");
               ISNEWSTACK ? newStackWrite (p, frame) : stackWrite (p, frame);
          }
          else {
               ih.when = frame->when;
               ih.shutter = cameraModule.intTime;
               ih.gain = cameraModule.realGain;
               ih.encoding = 0; /* unencoded */
               ih.length = p->size;

               /*  ok, write a frame */
               write (p->fd, &ih, sizeof (ih));
               {
                    unsigned long toBeWritten = ih.length;
                    unsigned long thisWrite;

                    while (toBeWritten) {
                         thisWrite =
                              write (p->fd, &ptr[ih.length - toBeWritten],
                                     ih.length);
                         if (-1 == thisWrite) {
                              perror ("raw data write");
                              exit (-1);
                         }
                         toBeWritten -= thisWrite;
                    }
               }


          }

          if (doThisManyRaw > 0)
               doThisManyRaw--;

          /* count it */
          if (0 == --p->count) {

               if (p->pixlist)
                    ISNEWSTACK ? newStackDone (p) : 0;

               /* all done! */
               close (p->fd);
               p->fd = -1;
               freeAndClear (p->pixlist);
               freeAndClear (p->pixOutLine);

               /* tell the requestor I'm done */
               if (p->requestor && p->msgBack) {
                    msgAnswer = p->msgBack;
                    msgDone ("all done, go ahead");
               }
          }

     }

     /* now do shared mem, if necessary */
     if (ihSHM.shmid) {
          ih.when = frame->when;
          ih.shutter = cameraModule.intTime;
          ih.gain = cameraModule.realGain;
          ih.encoding = 0;      /* unencoded */
          ih.length = p->size;
          if (0 == ihSHM.ih->sem) {
               memcpy ((struct ihPTR *) &ihSHM.ih->ih, (void *) &ih,
                       sizeof (ih));
               ihSHM.ih->sem = 1;
          }
     }
     if (dataSHM.shmid) {
          if (0 == dataSHM.data->sem) {
               memcpy ((void *) &dataSHM.data->data, (void *) ptr, ih.length);
               dataSHM.data->sem = 1;
          }
     }

     previous = now;
     /*me->remaining--; */

     if (blockAverage) {
          unsigned long x;
          blockAverage = 0;
          for (x = 100; x < cameraModule.bufferLength; x++) {
               blockAverage += frame->dataPtr[x];
          }
          blockAverage = blockAverage / (cameraModule.bufferLength - 100);
     }

#ifdef OFFSETYES
     if (doOffsetTest) {

          long x, y;
          long buff[2048];
          unsigned char *myptr = frame->dataPtr;
          /* sum columns of Y, diff across sum, report max */
          /*myptr++; */
          for (x = 0; x < cameraModule.x; buff[x++] = 0);

          for (y = 0; y < cameraModule.y; y++) {
               for (x = 0; x < cameraModule.x; x++) {
                    buff[x] += *myptr++;        /*myptr++; */
               }
          }

          y = 0;
          for (x = 1; x < cameraModule.x; x++)
               if ((buff[x] - buff[x - 1]) > y)
                    y = buff[x] - buff[x - 1];

          fprintf (logFD, "OffsetTest: diff %d\n", y);
          fflush (logFD);
          doOffsetTest = y ? y : 1;     /* how we answer the question */

     }
#endif

     /* experiment! */
     if (autoShutter.noAutoOnStacks && doingStack) {
          /* nothing!  don't autoShutter when stacking */
     }
     else if (autoShutter.noAutoWhenCollecting && doingAnything) {
          /* also nothing, don't auto when collecting */
     }
     else if (autoShutter.high) {

          maxY = 0;
          histY = 0;

          /* average intensity method */
          for (i = 0; autoShutter.pixList[i]; i++)
               histY += ptr[autoShutter.pixList[i]];
          histY = histY / i;

          /* if not bright enough, and gain isn't at max -- */
          if ((histY < autoShutter.low)) {
               /* try adjusting shutter, first */
               /* limit the int time to .3 seconds or thereabouts */
               if (cameraModule.intTime < autoShutter.intLimit)
                    (*cameraModule.setCamera) ("CAM_SCALE", "FEATURE_SHUTTER",
                                               1.1);
               /* if the shutter has maxed (no change when tried) -- */
               else
                    /* pump up the gain */
                    (*cameraModule.setCamera) ("CAM_ADD", "FEATURE_GAIN",
                                               1.0);

               showChange++;
          }
          /* if it is too bright -- */
          if (histY > autoShutter.high) {
               if (cameraModule.realGain > 1.01)
                    (*cameraModule.setCamera) ("CAM_ADD", "FEATURE_GAIN",
                                               -1.0);
               else {
                    if (cameraModule.realGain > 0.01)
                         (*cameraModule.setCamera) ("CAM_SET", "FEATURE_GAIN",
                                                    0.01);
                    else
                         (*cameraModule.setCamera) ("CAM_SCALE",
                                                    "FEATURE_SHUTTER", 0.9);
               }
               showChange++;
          }
          if (showChange)
               fprintf (logFD,
                        "%010d autoShutter: maxY: %d histY: %d new gain: %d (%.6fdB) shutter: %d (%.6f sec)\n",
                        start.tv_sec,
                        maxY,
                        histY,
                        cameraModule.gain,
                        cameraModule.realGain,
                        cameraModule.shutter, cameraModule.intTime);
     }


     err = gettimeofday (&end, NULL);
     if (err) {
          perror (me->module);
          me->processMicros = 9999999;
          return;
     }

     me->processMicros = (end.tv_sec - start.tv_sec) * 1000000
          + (end.tv_usec - start.tv_usec);

     getCommand ();
     fflush (logFD);

     return;
}

void
_processPixel (struct _processModule_ *me, struct _pixel_ *pix)
{
     return;
}

void
_closeFrame (struct _processModule_ *me)
{
     return;
}

void
_init ()
{
     unsigned long size;
     struct _processModule_ *me;
     char *module = "demon";
     struct _commandFileEntry_ *cmd;
     int i;
     char line[128];

     /* problem -- how do I find ME in the process list? */
     /* well, I will be the last member of the list! */
     me = firstProcessModule;
     while (me->next)
          me = me->next;

     strcpy (me->module, module);

     for (i = 0; i < CONCURRENT_ACTIONS; i++) {
          diskdata[i].size = cameraModule.bufferLength;
          diskdata[i].fd = -1;
          diskdata[i].requestor = 0;
          diskdata[i].pixlist = NULL;
          diskdata[i].pixOutLine = NULL;
          diskdata[i].stackDeBayer = 0;
     }

     /*getLongParam( &me->remaining, "diskFrames", module ); */
     me->remaining = 1;
     getLongParam (&me->verbose, "diskVerbose", module);

     sprintf (line, "/tmp/cam%02dd.log", cameraModule.cameraNumber);
     logFD = fopen (line, "w");
     if (NULL == logFD) {
          perror ("init log open");
     }

     if (!isatty (1)) {
          fprintf (logFD, "stdout not a tty: clearing verbose\n");
          me->verbose = 0;
     }

     VERBOSE (VERB_INIT) {
          KIDID;
          printf ("cam %d: %s: _init\n", cameraModule.cameraNumber, module);
     }

     /* make me a message queue */
     msgIn = cameraModule.cameraNumber;
     msgId = msgget (msgIn, IPC_CREAT | S_IRWXU | S_IRWXG | S_IRWXO);
     if (msgId < 0) {
          perror ("msg queue create");
          exit (-1);
     }

     me->processFrame = _processFrame;
     me->processPixel = NULL;
     me->closeFrame = NULL;
     me->saveResults = _saveResults;

     MSC = 0;
     autoShutter.high = 100;
     autoShutter.low = 80;
     autoShutter.skip = 900;
     autoShutter.intLimit = 0.1;        /* .1 second int time */
     /* next line avoids gain/int time corrections from raw files */
     autoShutter.noAutoWhenCollecting = 1;      /* don't autoshutter when taking data */
     newAutoList ();

     doOffsetTest = 0;          /* means don't do it. */
     doThisManyRaw = -1;        /* free running! */

}
