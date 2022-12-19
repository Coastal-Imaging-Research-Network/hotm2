
/* insert bitmapped text into an image. stolen from 'm' */

/* 
 *  $Log: inserttext16.c,v $
 *  Revision 1.1  2016/03/24 23:13:01  stanley
 *  Initial revision
 *
 *  Revision 1.2  2010/03/30 23:35:56  stanley
 *  changed width/height to deal with format 7 sub-sized
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

static const char rcsid[] = "$Id: inserttext16.c 172 2016-03-25 21:03:51Z stanley $";

#include "hotm.h"

/*****************************************************************************
* Ascii 8 by 8 regular font - only first 128 characters are supported.	     *
*****************************************************************************/
static unsigned char AsciiTable[][8] =
{
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	/* Ascii 0  */
    {0x3c, 0x42, 0xa5, 0x81, 0xbd, 0x42, 0x3c, 0x00},	/* Ascii 1  */
    {0x3c, 0x7e, 0xdb, 0xff, 0xc3, 0x7e, 0x3c, 0x00},	/* Ascii 2  */
    {0x00, 0xee, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00},	/* Ascii 3  */
    {0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00},	/* Ascii 4  */
    {0x00, 0x3c, 0x18, 0xff, 0xff, 0x08, 0x18, 0x00},	/* Ascii 5  */
    {0x10, 0x38, 0x7c, 0xfe, 0xfe, 0x10, 0x38, 0x00},	/* Ascii 6  */
    {0x00, 0x00, 0x18, 0x3c, 0x18, 0x00, 0x00, 0x00},	/* Ascii 7  */
    {0xff, 0xff, 0xe7, 0xc3, 0xe7, 0xff, 0xff, 0xff},	/* Ascii 8  */
    {0x00, 0x3c, 0x42, 0x81, 0x81, 0x42, 0x3c, 0x00},	/* Ascii 9  */
    {0xff, 0xc3, 0xbd, 0x7e, 0x7e, 0xbd, 0xc3, 0xff},	/* Ascii 10 */
    {0x1f, 0x07, 0x0d, 0x7c, 0xc6, 0xc6, 0x7c, 0x00},	/* Ascii 11 */
    {0x00, 0x7e, 0xc3, 0xc3, 0x7e, 0x18, 0x7e, 0x18},	/* Ascii 12 */
    {0x04, 0x06, 0x07, 0x04, 0x04, 0xfc, 0xf8, 0x00},	/* Ascii 13 */
    {0x0c, 0x0a, 0x0d, 0x0b, 0xf9, 0xf9, 0x1f, 0x1f},	/* Ascii 14 */
    {0x00, 0x92, 0x7c, 0x44, 0xc6, 0x7c, 0x92, 0x00},	/* Ascii 15 */
    {0x00, 0x00, 0x60, 0x78, 0x7e, 0x78, 0x60, 0x00},	/* Ascii 16 */
    {0x00, 0x00, 0x06, 0x1e, 0x7e, 0x1e, 0x06, 0x00},	/* Ascii 17 */
    {0x18, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x18},	/* Ascii 18 */
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x00},	/* Ascii 19 */
    {0xff, 0xb6, 0x76, 0x36, 0x36, 0x36, 0x36, 0x00},	/* Ascii 20 */
    {0x7e, 0xc1, 0xdc, 0x22, 0x22, 0x1f, 0x83, 0x7e},	/* Ascii 21 */
    {0x00, 0x00, 0x00, 0x7e, 0x7e, 0x00, 0x00, 0x00},	/* Ascii 22 */
    {0x18, 0x7e, 0x18, 0x18, 0x7e, 0x18, 0x00, 0xff},	/* Ascii 23 */
    {0x18, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},	/* Ascii 24 */
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x18, 0x00},	/* Ascii 25 */
    {0x00, 0x04, 0x06, 0xff, 0x06, 0x04, 0x00, 0x00},	/* Ascii 26 */
    {0x00, 0x20, 0x60, 0xff, 0x60, 0x20, 0x00, 0x00},	/* Ascii 27 */
    {0x00, 0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xff, 0x00},	/* Ascii 28 */
    {0x00, 0x24, 0x66, 0xff, 0x66, 0x24, 0x00, 0x00},	/* Ascii 29 */
    {0x00, 0x00, 0x10, 0x38, 0x7c, 0xfe, 0x00, 0x00},	/* Ascii 30 */
    {0x00, 0x00, 0x00, 0xfe, 0x7c, 0x38, 0x10, 0x00},	/* Ascii 31 */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	/* */
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x30, 0x00},	/* ! */
    {0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	/* " */
    {0x6c, 0x6c, 0xfe, 0x6c, 0xfe, 0x6c, 0x6c, 0x00},	/* # */
    {0x10, 0x7c, 0xd2, 0x7c, 0x86, 0x7c, 0x10, 0x00},	/* $ */
    {0xf0, 0x96, 0xfc, 0x18, 0x3e, 0x72, 0xde, 0x00},	/* % */
    {0x30, 0x48, 0x30, 0x78, 0xce, 0xcc, 0x78, 0x00},	/* & */
    {0x0c, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},	/* ' */
    {0x10, 0x60, 0xc0, 0xc0, 0xc0, 0x60, 0x10, 0x00},	/* ( */
    {0x10, 0x0c, 0x06, 0x06, 0x06, 0x0c, 0x10, 0x00},	/* ) */
    {0x00, 0x54, 0x38, 0xfe, 0x38, 0x54, 0x00, 0x00},	/* * */
    {0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00},	/* + */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x70},	/* , */
    {0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00},	/* - */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00},	/* . */
    {0x02, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x00},	/* / */
    {0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00},	/* 0x */
    {0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x3c, 0x00},	/* 1 */
    {0x7c, 0xc6, 0x06, 0x0c, 0x30, 0x60, 0xfe, 0x00},	/* 2 */
    {0x7c, 0xc6, 0x06, 0x3c, 0x06, 0xc6, 0x7c, 0x00},	/* 3 */
    {0x0e, 0x1e, 0x36, 0x66, 0xfe, 0x06, 0x06, 0x00},	/* 4 */
    {0xfe, 0xc0, 0xc0, 0xfc, 0x06, 0x06, 0xfc, 0x00},	/* 5 */
    {0x7c, 0xc6, 0xc0, 0xfc, 0xc6, 0xc6, 0x7c, 0x00},	/* 6 */
    {0xfe, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x60, 0x00},	/* 7 */
    {0x7c, 0xc6, 0xc6, 0x7c, 0xc6, 0xc6, 0x7c, 0x00},	/* 8 */
    {0x7c, 0xc6, 0xc6, 0x7e, 0x06, 0xc6, 0x7c, 0x00},	/* 9 */
    {0x00, 0x30, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00},	/* : */
    {0x00, 0x30, 0x00, 0x00, 0x00, 0x30, 0x20, 0x00},	/* }, */
    {0x00, 0x1c, 0x30, 0x60, 0x30, 0x1c, 0x00, 0x00},	/* < */
    {0x00, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00},	/* = */
    {0x00, 0x70, 0x18, 0x0c, 0x18, 0x70, 0x00, 0x00},	/* > */
    {0x7c, 0xc6, 0x0c, 0x18, 0x30, 0x00, 0x30, 0x00},	/* ? */
    {0x7c, 0x82, 0x9a, 0xaa, 0xaa, 0x9e, 0x7c, 0x00},	/* @ */
    {0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00},	/* A */
    {0xfc, 0xc6, 0xc6, 0xfc, 0xc6, 0xc6, 0xfc, 0x00},	/* B */
    {0x7c, 0xc6, 0xc6, 0xc0, 0xc0, 0xc6, 0x7c, 0x00},	/* C */
    {0xf8, 0xcc, 0xc6, 0xc6, 0xc6, 0xcc, 0xf8, 0x00},	/* D */
    {0xfe, 0xc0, 0xc0, 0xfc, 0xc0, 0xc0, 0xfe, 0x00},	/* E */
    {0xfe, 0xc0, 0xc0, 0xfc, 0xc0, 0xc0, 0xc0, 0x00},	/* F */
    {0x7c, 0xc6, 0xc0, 0xce, 0xc6, 0xc6, 0x7e, 0x00},	/* G */
    {0xc6, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x00},	/* H */
    {0x78, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00},	/* I */
    {0x1e, 0x06, 0x06, 0x06, 0xc6, 0xc6, 0x7c, 0x00},	/* J */
    {0xc6, 0xcc, 0xd8, 0xf0, 0xd8, 0xcc, 0xc6, 0x00},	/* K */
    {0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xfe, 0x00},	/* L */
    {0xc6, 0xee, 0xfe, 0xd6, 0xc6, 0xc6, 0xc6, 0x00},	/* M */
    {0xc6, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0xc6, 0x00},	/* N */
    {0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00},	/* O */
    {0xfc, 0xc6, 0xc6, 0xfc, 0xc0, 0xc0, 0xc0, 0x00},	/* P */
    {0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x06},	/* Q */
    {0xfc, 0xc6, 0xc6, 0xfc, 0xc6, 0xc6, 0xc6, 0x00},	/* R */
    {0x78, 0xcc, 0x60, 0x30, 0x18, 0xcc, 0x78, 0x00},	/* S */
    {0xfc, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00},	/* T */
    {0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00},	/* U */
    {0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00},	/* V */
    {0xc6, 0xc6, 0xc6, 0xd6, 0xfe, 0xee, 0xc6, 0x00},	/* W */
    {0xc6, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0xc6, 0x00},	/* X */
    {0xc3, 0xc3, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x00},	/* Y */
    {0xfe, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0xfe, 0x00},	/* Z */
    {0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00},	/* [ */
    {0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x00},	/* \ */
    {0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00},	/* ] */
    {0x00, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00},	/* ^ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff},	/* _ */
    {0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},	/* ` */
    {0x00, 0x00, 0x7c, 0x06, 0x7e, 0xc6, 0x7e, 0x00},	/* a */
    {0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xe6, 0xdc, 0x00},	/* b */
    {0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0x7e, 0x00},	/* c */
    {0x06, 0x06, 0x7e, 0xc6, 0xc6, 0xce, 0x76, 0x00},	/* d */
    {0x00, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0x7e, 0x00},	/* e */
    {0x1e, 0x30, 0x7c, 0x30, 0x30, 0x30, 0x30, 0x00},	/* f */
    {0x00, 0x00, 0x7e, 0xc6, 0xce, 0x76, 0x06, 0x7c},	/* g */
    {0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0x00},	/* */
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00},	/* i */
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0xf0},	/* j */
    {0xc0, 0xc0, 0xcc, 0xd8, 0xf0, 0xd8, 0xcc, 0x00},	/* k */
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00},	/* l */
    {0x00, 0x00, 0xcc, 0xfe, 0xd6, 0xc6, 0xc6, 0x00},	/* m */
    {0x00, 0x00, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0x00},	/* n */
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x00},	/* o */
    {0x00, 0x00, 0xfc, 0xc6, 0xc6, 0xe6, 0xdc, 0xc0},	/* p */
    {0x00, 0x00, 0x7e, 0xc6, 0xc6, 0xce, 0x76, 0x06},	/* q */
    {0x00, 0x00, 0x6e, 0x70, 0x60, 0x60, 0x60, 0x00},	/* r */
    {0x00, 0x00, 0x7c, 0xc0, 0x7c, 0x06, 0xfc, 0x00},	/* s */
    {0x30, 0x30, 0x7c, 0x30, 0x30, 0x30, 0x1c, 0x00},	/* t */
    {0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00},	/* u */
    {0x00, 0x00, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00},	/* v */
    {0x00, 0x00, 0xc6, 0xc6, 0xd6, 0xfe, 0x6c, 0x00},	/* w */
    {0x00, 0x00, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0x00},	/* x */
    {0x00, 0x00, 0xc6, 0xc6, 0xce, 0x76, 0x06, 0x7c},	/* y */
    {0x00, 0x00, 0xfc, 0x18, 0x30, 0x60, 0xfc, 0x00},	/* z */
    {0x0e, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0e, 0x00},	/* { */
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},	/* | */
    {0xe0, 0x30, 0x30, 0x1c, 0x30, 0x30, 0xe0, 0x00},	/* } */
    {0x00, 0x00, 0x70, 0x9a, 0x0e, 0x00, 0x00, 0x00},	/* ~ */
    {0x00, 0x00, 0x18, 0x3c, 0x66, 0xff, 0x00, 0x00}	/* Ascii 127 */
};

/* 16 bit mono data */
void
insertText16(int line, uint16_t * data, char *textLine)
{
    int             i;
    int             j;
    int             k;
    int             mask;
    int             outIndex;
    char            ch;
    char            b;
    long            offset;
    long	    width;
    long            height;
    int             charWidth = 8;
    int             charHeight = 8;
    
    width = cameraModule.cameraWidth;
    height = cameraModule.cameraHeight;

    offset = line * width * charHeight;

    /* black background, set color to none */
    for (k = 0; k < charHeight; k++) {
	for (i = 0; i < strlen(textLine) * charWidth; i++) {
	    data[i + k * width + offset] = 0; 
	}
    }

    for (i = 0; textLine[i]; i++)
	if (textLine[i] < ' ')
	    textLine[i] = ' ';

    for (i = 0; i < strlen(textLine); i++) {

	ch = textLine[i];
	for (j = 0; j < charHeight; j++) {

	    b = AsciiTable[ch][j];

	    for (k=0, mask = 128; mask; k++, mask >>= 1) {

		if (mask & b)
			data[(i * 8) + (j * width) + offset + k] = 65535;
	    }
	}
    }
}


