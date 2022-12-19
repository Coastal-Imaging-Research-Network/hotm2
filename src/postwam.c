

/* postwam -- create wave averaged movies from SHM interface */

/* theory of ops: average frames for N seconds, overlapping by N/2 seconds */
/* add them as they come in. When hitting N, average and write as jpeg. */
/* swapping two buffers */

static char *RCSId = "$Id: postwam.c $";

#define _MAIN_

#include "demon2.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <SDL/SDL.h>
#include <sys/time.h>

SDL_Event event;

#define clearSEM ihSHM.ih->sem = dataSHM.data->sem = 0

struct
{
     SDL_Surface *screen;
     SDL_Surface *image;
     SDL_Color colors[256];
     int modulo;
     int isRGB;
} showData;

double deBayer( struct _pixel_ *, uint16_t *, struct _cameraModule_ * );

void
goaway( int j )
{
     printf( "exiting at user request\n" );
     exit( -1 );
}
     struct
     {
          key_t key;
          int shmid;
          struct ihPTR
          {
               int sem;
               struct _imageHeader_ ih;
          } *ih;
     } ihSHM;

     struct
     {
          key_t key;
          int shmid;
          struct dataPTR
          {
               int sem;
               uint16_t data;
          } *data;
     } dataSHM;

     struct
     {
          key_t key;
          int shmid;
          struct _cameraModule_ *cm;
     } cmSHM;


     unsigned long dataStart[2];
     unsigned long dataEnd[2];

int
main( int argc, char **argv )
{

     struct _imageHeader_ ih;
     long ihsize;               /* old is shorter. */

     uint16_t *data[2];
     int frameLength = 30;      /* in seconds */

     int dataCount[2];

     struct smalltimeval frameTime;

     unsigned long frameCount = 1800;

     char *ptr;
     struct _pixel_ *RGB;
     struct _pixel_ *RGB2;

     unsigned long nowEpoch;
     unsigned char *dataPtr;

     uint16_t *tmpDataPtr;

     unsigned long inDataBytes;
     unsigned long pixCount;
     unsigned long pixBytes;
     unsigned long i;

     double lastTime;
     double firstTime;
     double period;

     int j;
     int ii;
     int in;
     int count;
     int camnum = -1;
     char line[256];
     char fileName[256];
     struct timeval xxstart;
     struct timeval xxend;
     long ltemp;
     int doDeBayer = 0;
     int modulo = 1;
     int showAnImage = 0;

     double maxSD = 0;
     int beNoisy = 1;

     int video = 0;

     signal( SIGINT, &goaway );

     for( j = 0;; j++ ) {

          if( 0 == argv[j] )
               break;

          if( !strcmp( argv[j], "-q" ) ) {
               beNoisy = 0;
               continue;
          }

          if( !strncmp( argv[j], "-m", 2 ) ) {
               modulo = atoi( argv[j + 1] );
               printf( "using modulo %d assumes video\n", modulo );
               j++;
               video++;
               continue;
          }
          if( !strncmp( argv[j], "-c", 2 ) ) {
               camnum = atoi( argv[j + 1] );
               printf( "using camera number %d\n", camnum );
               j++;
               continue;
          }
          if( !strncmp( argv[j], "-v", 2 ) ) {
               video = 1;
               printf( "using video\n" );
               continue;
          }

          if( !strncmp( argv[j], "-fc", 3 ) ) {
               frameCount = atol( argv[j + 1] );
               printf( "doing %ld total frames\n", frameCount );
               j++;
               continue;
          }

          if( !strncmp( argv[j], "-at", 3 ) ) {
               frameLength = atof( argv[j + 1] );
               printf( "movie frames from %d seconds\n", frameLength );
               j++;
               if( frameLength * 2 > 255 ) {
                    /* uint16 data accumulators */
                    printf
                         ( "OOPS! cannot do that long an average. setting to 125s\n" );
                    frameLength = 125;
               }
               continue;
          }


     }

     ihSHM.key = 0x100 + camnum;
     dataSHM.key = 0x200 + camnum;
     cmSHM.key = 0x300 + camnum;

     ihSHM.shmid = shmget( ihSHM.key, sizeof( struct ihPTR ), 0644 );
     if( ihSHM.shmid == -1 ) {
          printf( "Error getting shared memory segment. Did you\n" );
          printf( "send an openSHM to the camera?\n" );
          perror( "postshow: shmget header" );
          exit( 0 );
     }
     ihSHM.ih = ( struct ihPTR * ) shmat( ihSHM.shmid, 0, 0 );

     cmSHM.shmid = shmget( cmSHM.key, sizeof( struct _cameraModule_ ), 0644 );
     cmSHM.cm = ( struct _cameraModule_ * ) shmat( cmSHM.shmid, 0, 0 );

     dataSHM.shmid = shmget( dataSHM.key, cameraModule.bufferLength, 0644 );
     dataSHM.data = ( struct dataPTR * ) shmat( dataSHM.shmid, 0, 0 );

     /* may have old image in frame, clear semaphore and wait for new */
     /* do it twice, make sure we have fresh data */
     clearSEM;
     while( 0 == dataSHM.data->sem ) {
          usleep( 5000 );
     }

     clearSEM;
     while( 0 == dataSHM.data->sem ) {
          usleep( 5000 );
     }

     memcpy( &cameraModule, cmSHM.cm, sizeof( cameraModule ) );
     memcpy( &ih, &ihSHM.ih->ih, sizeof( ih ) );

     showData.isRGB = ( cameraModule.format == formatRGB8 );

     pixCount = cameraModule.x * cameraModule.y;
     inDataBytes = cameraModule.bufferLength;

     data[0] = ( uint16_t * ) calloc( pixCount, sizeof( uint16_t ) );
     data[1] = ( uint16_t * ) calloc( pixCount, sizeof( uint16_t ) );
     if( data[0] == NULL || data[1] == NULL ) {
          printf( "cannot calloc space for data!\n" );
          exit( -1 );
     }


     /* prep buffers and flags */
     dataCount[0] = dataCount[1] = 0;
     dataStart[0] = ih.when.tv_sec;     /* now */
     dataStart[1] = dataStart[0] + frameLength / 2;     /* half average offset */

     dataEnd[0] = dataStart[0] + frameLength;
     dataEnd[1] = dataStart[1] + frameLength;

     if( beNoisy ) {
          printf
               ( "setting up 0 start: %ld 0 end: %ld \n",
                 dataStart[0], dataEnd[0] );
          printf
               ( "           1 start: %ld 1 end: %ld\n",
                 dataStart[1], dataEnd[1] );
     }

     RGB = malloc( pixCount * sizeof( struct _pixel_ ) );

     if( video ) {
          if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
               fprintf( stderr,
                        "%s: cannot init SDL: %s\n", "postshow",
                        SDL_GetError(  ) );
               return -1;
          }

          showData.screen = SDL_SetVideoMode( cameraModule.x / modulo,
                                              cameraModule.y / modulo,
                                              0, SDL_SWSURFACE );

          for( i = 0; i < 256; i++ )
               showData.colors[i].r = showData.colors[i].g =
                    showData.colors[i].b = i;

          if( NULL == showData.screen ) {
               fprintf( stderr, "%s: cannot video mode SDL\n", "postshow" );
               return -1;
          }

     }


     while( frameCount ) {

          /* if frameCount is 1, then we are on the last frame. force a 
             save of data by setting end time  to 0 */
          if( frameCount == 1 )
               dataEnd[1] = dataEnd[0] = 0;

          /* data sem is set last by demon, check first here. */
          /* if set, then ih is valid, too */
          while( 0 == dataSHM.data->sem )
               usleep( 5000 );

          memcpy( &cameraModule, cmSHM.cm, sizeof( cameraModule ) );
          memcpy( &ih, &ihSHM.ih->ih, sizeof( ih ) );

          frameTime = ih.when;
          nowEpoch = ih.when.tv_sec;

          if( beNoisy ) {
               printf( "working on frame %ld %u %u %lu dc1: %d dc2: %d\n",
                       frameCount, nowEpoch, dataStart[0], dataStart[1], dataCount[0], dataCount[1] );
          }

#define doMe(x) if( nowEpoch > dataStart[x] )

          /* copy data */
          dataPtr = ( uint8_t * ) & dataSHM.data->data;
          for( i = 0; i < pixCount; i++ ) {
               doMe( 0 ) ( data[0] )[i] += *dataPtr;
               doMe( 1 ) ( data[1] )[i] += *dataPtr;
               dataPtr++;
          }
          doMe( 0 ) dataCount[0]++;
          doMe( 1 ) dataCount[1]++;

          /* test for end of average */
          for( ii = 0; ii < 2; ii++ ) {

               if( nowEpoch > dataEnd[ii] ) {

                    /* average. don't forget to average, john */
                    for( i = 0; i < pixCount; i++ )
                         ( data[ii] )[i] = ( data[ii] )[i] / dataCount[ii];

                    /* debayer for save, display */
                    deBayer( RGB, data[ii], &cameraModule );

                    /* print info for save */
                    sprintf( line,
                             "C: %ld %u Gain: %.2fdB IntTime: %lf FrameCount: %ld AvgTime: %d V:2.0",
                             cameraModule.cameraNumber,
                             dataStart[ii], ih.gain, ih.shutter, frameCount, frameLength );
                    line[cameraModule.x / 8] = 0;
                    insertText( 0, RGB, line );
                    sprintf( fileName,
                             "%ld.c%u.wam.jpg",
                             dataStart[ii], cameraModule.cameraNumber );
                    save24( RGB, fileName, 90 );

                    /* subsample */
                    if( modulo > 1 ) {
                         RGB2 = RGB;
                         for( i = 0; i < cameraModule.y; i += modulo )
                              for( j = 0; j < cameraModule.x - ( modulo - 1 );
                                   j += modulo )
                                   *RGB2++ = RGB[i * cameraModule.x + j];
                    }

                    cameraModule.cameraWidth /= modulo;
                    insertText( 0, RGB, line );

                    if( video ) {
                         /* at this point, image is ready in RGB, now display */

                         showData.image = SDL_CreateRGBSurfaceFrom( RGB,
                                                                    cameraModule.
                                                                    x /
                                                                    modulo,
                                                                    cameraModule.
                                                                    y /
                                                                    modulo,
                                                                    24,
                                                                    3 *
                                                                    ( cameraModule.
                                                                      x /
                                                                      modulo ),
                                                                    0xff,
                                                                    0xff00,
                                                                    0xff0000,
                                                                    0 );
                         SDL_BlitSurface( showData.image, NULL,
                                          showData.screen, NULL );
                         SDL_UpdateRect( showData.screen, 0, 0,
                                         showData.image->w,
                                         showData.image->h );
                         sprintf( line, "Cam %ld: %13d.%06d",
                                  cameraModule.cameraNumber, frameTime.tv_sec,
                                  frameTime.tv_usec );
                         SDL_WM_SetCaption( line, line );

                    }

                    /* set new frame time limits */
                    dataStart[ii] = dataEnd[ii];
                    dataEnd[ii] = dataStart[ii] + frameLength;
                    dataCount[ii] = 0;

                    memset( data[ii], 0, sizeof( uint16_t ) * pixCount );

                    if( beNoisy )
                         printf( "end of frame data %d: %s %s\n", ii, line,
                                 fileName );
               }


          }
          clearSEM;
          frameCount--;
     }
}
