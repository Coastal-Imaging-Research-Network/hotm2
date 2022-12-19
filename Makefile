
THINGS = \
	hotm \
	demon.so \
	micropix.so \
	show.so \
	focus.so \
	cam01.startup \
	cam1 \
	cameraData \
	cameraMapping \
	cdisk \
	cleanmsg \
	dc1394ParamList \
	demon.so \
	doCam \
	ipcstat \
	lastRunTime \
	lastS \
	postproc \
	process \
	send.pl \
	startup.pl \


package: 
	tar cvfh hotm.pack.tar $(THINGS)
	
