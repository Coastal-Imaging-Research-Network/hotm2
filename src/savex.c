/* saveX -- save result data as jpeg images */
/* stolen from 'm' */

/* 
 *  $Log: savex.c,v $
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

static const char rcsid[] = "$Id: savex.c 391 2018-09-20 21:23:43Z stanley $";


/* save[24|8] -- save an 8 or 24 bit image to disk in rasterfile format */

#include "hotm.h"

#include <jpeglib.h>

/* save data. by now, we are RGB format. name is already created for us */
void
save24(struct _pixel_ * data, char *file, int qual )
{

    unsigned char  *raw;
    unsigned char  *cptr;
    int             i;
    int             size;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE           *outfile;
    void           *ptr;
    
    /* open */
    outfile = fopen(file, "wb");
    if (outfile == NULL) {
	//printf("Save24: Error opening file for output: %s\n", file);
	//perror("Save24");
	return;
    }

    /* make a temp array to gather up the luminance data */
    raw = malloc(3 * cameraModule.cameraWidth);
    if (raw == NULL) {
	//printf("Save24: can't malloc space for temp output\n");
	return;
    }

    /* jpeg things, from example.c */
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = cameraModule.cameraWidth;
    cinfo.image_height = cameraModule.cameraHeight;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, qual, TRUE);
    jpeg_set_colorspace( &cinfo, JCS_YCbCr );

    jpeg_start_compress(&cinfo, TRUE);

    /* loop through the jpeg stuff line by line */
    while (cinfo.next_scanline < cinfo.image_height) {

	cptr = raw;
	/* gather all the data in one place */
	for (i = 0; i < cameraModule.cameraWidth; i++) {

	    *cptr++ = data->r;
	    *cptr++ = data->g;
	    *cptr++ = data->b;
	    data++;
	}

	jpeg_write_scanlines(&cinfo, (JSAMPARRAY)&raw, 1);

    }

    jpeg_finish_compress(&cinfo);

    jpeg_destroy_compress(&cinfo);

    fclose(outfile);

    free(raw);

}


