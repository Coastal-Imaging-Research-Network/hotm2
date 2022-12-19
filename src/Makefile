
HOTMSOURCES = parseCommandLine.c \
	loadCommandFile.c \
	findCommand.c \
	loadCameraModule.c \
	appendList.c \
	getBoolean.c \
	getLong.c \
	getDouble.c \
	tagCommand.c \
	dumpCommandFile.c \
	getString.c \
	commonCommand.c \
	memory.c \
	loadProcessModules.c \
	processLoop.c \
	saveResults.c \
	dc1394Decode.c \
	message.c \
	loadTimer.c 

MODULESOURCES = \
	modeToFormat.c \
	whateverToRGB.c \
	savex.c \
	savePNG.c \
	mapCameraToUID.c \
	mapCameraNameToData.c \
	framerateToDouble.c \
	inserttext.c \
	inserttext16.c \
	initRaw.c \
	demonPostProc.c \
	demonPostDump.c \
	deBayer.c \
	deBayerRGGB.c \
	deBayerBGGR.c \
	alarm.c 

PACKAGE = hotm \
	postproc \
	postprocGigE \
	postplay \
	postshow \
	gige.so \
	micropix.so \
	cam1 \
	cameraData \
	cameraMapping \
	cdisk \
	dc1394ParamList \
	dict.xml \
	hotm2.pdf \
	rawFormat.txt
	
HOTMOBJECTS = ${HOTMSOURCES:.c=.o}
MODULEOBJECTS = ${MODULESOURCES:.c=.o} -lc

INCLUDES = hotm.h alarm.h camera.h demon2.h 

SDLLIBPATH = -lSDL -lpthread

COMMONLIBS = -ldl -lm  -L/usr/local/lib  -ljpeg -lpng 
FIRELIBS = -ldc1394 -lraw1394 

MODULELIBS = $(COMMONLIBS) 

CCgcc = gcc
CFLAGSgcc = -pedantic  -g -Wfatal-errors -I/usr/local/include -I/usr/local/include/SDL -L/usr/local/lib -ggdb 
CFLAGSgcccheck = -ggdb3 -O -pedantic-errors  -Wuninitialized -W -Wparentheses -Wsequence-point -I/usr/local/include -I/usr/local/include/SDL -L/usr/local/lib
CFLAGS = $(CFLAGSgcc) -fPIC

TARGETS = \
	demon2.so \
	hotm \
	pleora.so \
	postproc \
	postprocGigE \
	postshow \
	postplay \
	gige.so \
	gigeSpin.so \
	micropix.so \


default : gigeSpin support

all : $(TARGETS)

gigeSpin :
	CFLAGS="$(CFLAGSgcc)" CC="$(CCgcc)" make hotm demon2.so gigeSpin.so

gige : 
	CFLAGS="$(CFLAGSgcc)" CC="$(CCgcc)" make hotm demon2.so gige.so 

micropix :
	CFLAGS="$(CFLAGSgcc)" CC="$(CCgcc)" make hotm demon2.so micropix.so

pleora : 
	CFLAGS="$(CFLAGSgcc)" CC="$(CCgcc)" make hotm demon2.so pleora.so

support : $(MODULEOBJECTS)
	CFLAGS="$(CFLAGSgcc)" CC="$(CCgcc)" make postproc postplay postshow postwam postprocGigE

gcc :
	CFLAGS="$(CFLAGSgcc)" CC="$(CCgcc)" make all
check :
	CFLAGS="$(CFLAGSgcccheck)" CC="$(CCgcc)" make all
	
hotm: hotm.c $(INCLUDES) $(HOTMOBJECTS) 
	$(CC) $(CFLAGS) $(LDFLAGS) -rdynamic -Bstatic -g -o hotm hotm.c $(HOTMOBJECTS) $(MODULELIBS)
	
$(HOTMOBJECTS) : $(INCLUDES)
$(MODULEOBJECTS) : $(INCLUDES)

gigeSpin.so : gigeSpin.c hotm.h gigeSpin-camera.h alarm.h $(MODULEOBJECTS)
	#$(CC) -E -I/usr/include/spinnaker -I/usr/include/spinnaker/spinc $(CFLAGS) -c gigeSpin.c > gigeSpin.i
	$(CC) -I/usr/include/spinnaker -I/usr/include/spinnaker/spinc $(CFLAGS) -c gigeSpin.c
	$(LD) $(MODULELIBS) -shared -o gigeSpin.so gigeSpin.o $(MODULEOBJECTS) -L/usr/lib/spinnaker -lSpinnaker  -lSpinnaker_C

gige.so : gige.c hotm.h gige-camera.h alarm.h $(MODULEOBJECTS)
	$(CC) -I/usr/include/flycapture $(CFLAGS) -c gige.c
	$(LD) $(MODULELIBS) -shared -o gige.so gige.o -L/usr/local/lib/C -lflycapture -lflycapture-c $(MODULEOBJECTS)

micropix.so : micropix.c hotm.h camera.h alarm.h $(MODULEOBJECTS) mapUIDToPortAndNode.o
	$(CC) $(CFLAGS) -c micropix.c
	$(LD) $(MODULELIBS) $(FIRELIBS) -shared -o micropix.so micropix.o $(MODULEOBJECTS) mapUIDToPortAndNode.o

pleora.so : pleora.c hotm.h camera.h 
	$(CC) $(CFLAGS) -c pleora.c
	$(LD) -shared -o pleora.so pleora.o -lpng -ljpeg $(MODULEOBJECTS)

# warn: dmStack currently does not work properly when optimized by the
# intel compiler. bad data in first byte of output stack
dmStack.o : dmStack.c demon2.h rast.h
	$(CC) $(CFLAGS) -c -I/usr/local/include dmStack.c

newStack.o : newStack.c CDFunctions.c CD.h demon2.h rast.h
	$(CC) $(CFLAGS) -c -I/usr/local/include newStack.c

initRaw.o : initRaw.c demon2.h 
	$(CC) $(CFLAGS) -c initRaw.c

demon2.so : demon2.o demon2.c initRaw.o dmStack.o newStack.o demon2.h demonPostProc.c hotm.h deBayerCode.h deBayer.o deBayerRGGB.o deBayerBGGR.o inserttext16.o inserttext.o savex.o savePNG.o $(MODULEOBJECTS)
	#$(CC) $(CFLAGS) -fPIC -Wa,-aln=foo.s -c demon2.c
	$(LD) $(MODULELIBS) -shared -o demon2.so demon2.o dmStack.o newStack.o -lpng -ljpeg  -lm -lnetcdf $(MODULEOBJECTS)

postproc : postproc.c hotm.h demon2.h inserttext.o savex.o whateverToRGB.o deBayerCode.h deBayer.o deBayerRGGB.o deBayerBGGR.o savePNG.o $(MODULESOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o postproc postproc.c inserttext.o whateverToRGB.o savex.o deBayer.o deBayerRGGB.o deBayerBGGR.o savePNG.o -lpng -ljpeg -lm

postprocGigE : postprocGigE.c hotm.h demon2.h inserttext.o savex.o whateverToRGB.o deBayerCode.h deBayer.o deBayerRGGB.o deBayerBGGR.o $(MODULESOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o postprocGigE postprocGigE.c \
	modeToFormat.o \
	whateverToRGB.o \
	savex.o \
	savePNG.o \
	inserttext.o \
	inserttext16.o \
	initRaw.o \
	demonPostProc.o \
	deBayer.o \
	deBayerRGGB.o \
	deBayerBGGR.o \
	$(COMMONLIBS)

postshow : postshow.c hotm.h demon.h inserttext16.o inserttext.o whateverToRGB.o deBayerCode.h deBayer.o deBayerRGGB.o deBayerBGGR.o $(MODULESOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o postshow postshow.c inserttext16.o inserttext.o whateverToRGB.o deBayer.o deBayerRGGB.o deBayerBGGR.o -lSDL -lrt

postwam : postwam.c hotm.h demon.h inserttext16.o inserttext.o whateverToRGB.o deBayerCode.h deBayer.o deBayerRGGB.o deBayerBGGR.o $(MODULESOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o postwam postwam.c inserttext16.o inserttext.o whateverToRGB.o deBayer.o deBayerRGGB.o deBayerBGGR.o savex.o -lSDL -lrt -ljpeg -g

postplay : postplay.c hotm.h demon2.h inserttext.o whateverToRGB.o deBayerCode.h deBayer.o deBayerRGGB.o deBayerBGGR.o $(MODULESOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o postplay postplay.c inserttext.o whateverToRGB.o deBayer.o deBayerRGGB.o deBayerBGGR.o -B static -lSDL -lrt

clean :
	rm -f *.o *.so hotm postproc postshow postplay postwam postprocGigE

package :
	tar cvf hotm.tar $(PACKAGE)

src-package :
	tar cvf hotm.src.tar `cat MANIFEST`
