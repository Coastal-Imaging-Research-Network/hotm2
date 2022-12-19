/* the common parts of cameraData that all cameras need */
/* this has to be done this way to allow all cameras to */
/* load their own access to this even if the fiddly bits */
/* are different. These fiddly bits are all the same, and */
/* should NEVER be accessed outside the loaded camera */
/* module! */
/* this include makes them all the same name, same offset, */
/* same-o same-o for all users. */

	int	mySize;                    /* how big am I, maximum */
	int	number;
	char	UID[128];
	struct	_frame_ testFrame;
	long	mode;
	long	format;
	long	frameRate;
	long	myFeatures[NUM_FEATURES];
	long	speed; /*  100/200/400 */
	struct timeval lastFrame;
	int	myNode;
	long    tsource;
	long    interval;
        long    offset;	/* software trigger offset */

	char    modelName[257];

	/* these two values set during camera init -- timestamp from
	   camera and gettimeofday timeval so I can convert */
	int64_t		referenceTimestamp;
        struct timeval  referenceTimeval;

	int64_t	frameID; /* sequential number from camera */
	int64_t	lastframeID; /* sequential number from camera */

