/* savePNG -- save result data as PNG images */
/* stolen from 'm' */

/* 
 *  $Log: savePNG.c,v $
 *  Revision 1.1  2016/03/24 23:42:28  stanley
 *  Initial revision
 *
 *  Revision 1.5  2010/03/30 23:36:33  stanley
 *  changed image size to deal with format 7 sub-images
 *
 *  Revision 1.4  2004/12/06 21:50:40  stanley
 *  changed colorspace to YCbCr
 *
 *  Revision 1.3  2003/10/05 23:04:02  stanley
 *  removed more //
 *
 *  Revision 1.2  2003/10/05 23:03:14  stanley
 *  removed // comments
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

static const char rcsid[] = "$Id: savePNG.c 172 2016-03-25 21:03:51Z stanley $";


/* save[24|8] -- save an 8 or 24 bit image to disk in rasterfile format */

#include "hotm.h"

#include <png.h>

/* save data. by now, we are RGB format. name is already created for us */
void
savePNG(struct _pixel_ * data, char *file, int bpp)
{

    unsigned char  *raw;
    unsigned char  *cptr;
    int             i;
    int             size;
    int is16bit;

    png_structp png_ptr;
    png_infop   info_ptr;

    png_bytep  *rowPtrs;


    FILE           *outfile;
    void           *ptr;
    uint16_t *rdata;

    is16bit = (cameraModule.format == formatMONO16 );
    
    /* open */
    outfile = fopen(file, "wb");
    if (outfile == NULL) {
	printf("SavePNG: Error opening file for output: %s\n", file);
	perror("SavePNG");
	return;
    }

    png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, 
				NULL, NULL, NULL );
    if(!png_ptr){
	printf("failure png_ptr\n");
	exit;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr){
	printf("failure info_ptr\n");
	exit;
    }

    png_init_io( png_ptr, outfile );	

    png_set_IHDR( png_ptr, info_ptr, 
		cameraModule.cameraWidth, 
		cameraModule.cameraHeight, 
		bpp, 
		bpp==8? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_GRAY,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT );

    rowPtrs = (png_bytep *)
		malloc( cameraModule.cameraHeight*sizeof(png_bytep *));

    rdata = (uint16_t *)data;

    for( i=0; i<cameraModule.cameraHeight;i++) 
	if(is16bit)
		rowPtrs[i] = (png_bytep)&rdata[i*cameraModule.cameraWidth];
	else
		rowPtrs[i] = (png_bytep)&data[i*cameraModule.cameraWidth];

    png_set_rows( png_ptr, info_ptr, rowPtrs );

    png_write_png( png_ptr, info_ptr, PNG_TRANSFORM_SWAP_ENDIAN, NULL );
    png_write_end( png_ptr, NULL );

    png_free_data( png_ptr, info_ptr, PNG_FREE_ALL, -1 );

    fclose(outfile);


}


