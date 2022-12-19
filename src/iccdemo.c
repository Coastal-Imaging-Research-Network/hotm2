
#include <fcntl.h>
#include <stdio.h>
#include "rast.h"
#include <netinet/in.h>

int main()
{

struct rasterfile r;
int fd;
char cmap[256];
int i;



fd = open( "demo.ras", O_WRONLY | O_CREAT | O_NDELAY );

if( fd < 0 ) perror("open");

    r.ras_magic =     (RAS_MAGIC);
	r.ras_width =     (200 + 8);
	r.ras_height =    (200 + 8);
    r.ras_length =    ((200+8) * (200+8));
    r.ras_depth =     (8);
    r.ras_type =      (RT_STANDARD);
    r.ras_maptype =   (RMT_EQUAL_RGB);
    r.ras_maplength = (3 * 256);
	
    /* put in a color map */
    for (i = 0; i < 256; cmap[i] = i++);

	/* the following write puts out wrong data when using the intel
	   compiler and -ip -xW */
    write(fd, &r, 32);
    write(fd, cmap, 256);
    write(fd, cmap, 256);
    write(fd, cmap, 256);
	
	exit(1);
	
	printf("skipped the exit!\n");

close(fd);

}
