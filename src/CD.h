/*
 *  $Log: CD.h,v $
 *  Revision 1.1  2010/01/13 00:29:55  stanley
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
                                                                                
/* $Id: CD.h,v 1.1 2010/01/13 00:29:55 stanley Exp $ */
                                                                                


/* defines for CD data format */

/* copy MATLAB mex data class definitions */
typedef enum {
        mxUNKNOWN_CLASS = 0,
        mxCELL_CLASS,
        mxSTRUCT_CLASS,
        mxLOGICAL_CLASS,
        mxCHAR_CLASS,
        mxVOID_CLASS,
        mxDOUBLE_CLASS,
        mxSINGLE_CLASS,
        mxINT8_CLASS,
        mxUINT8_CLASS,
        mxINT16_CLASS,
        mxUINT16_CLASS,
        mxINT32_CLASS,
        mxUINT32_CLASS,
        mxINT64_CLASS,
        mxUINT64_CLASS,
        mxFUNCTION_CLASS,
        mxOPAQUE_CLASS,
        mxOBJECT_CLASS
} mxClassID;

/* data length in bytes associated with each matlab type */
/* 0 means we don't know and cannot support it here */
int typeLen[] = { 0, 0, 0, 1, 1, 0, 8, 4, 1, 1, 2, 2, 4, 4, 8, 8, 0, 0, 0};

/* where is my default data dictionary */
#define DEFAULT_DICTIONARY "dict.xml"

/* all scalars have tags < MIN_BLOB_TAG so we can test */
/*#define MIN_BLOB_TAG 100
typedef enum {
	compressedTAG = 0,
	epochTAG,
	shutterTAG,
	gainTAG,
	dataTAG=MIN_BLOB_TAG
} tagVal; */

typedef enum {
	CDunknown,
	CDcompressedType,
	CDdoubleType,
	CDblobType
} tagType;

/* state information, based on fd number */
/* up to ten open stacks, should be enough */
#define MAX_TAGS 20
#define MAX_TAG_NAME 128
#define MAX_DICT_ENTRIES 100
#define MAX_DICT_NAME 128
#define MAX_STATES 10

struct _CDstate_ {
	int fd;
	long tagDepth;
	struct {
		char name[MAX_TAG_NAME];
	} tagNames[MAX_TAGS];
	struct {
		char name[MAX_DICT_NAME];
		int value;
		tagType type;
	} dict[MAX_DICT_ENTRIES];
} CDstate[MAX_STATES];


/* sigh. The only wrong part of the creation of blobs for MySQL was */
/* storing dimension, type, and length info for the blob in non-x86 */
/* format. So I must poke the values one byte at a time ... */
/* FORTUNATELY, the data itself is x86 byte order! */
#define poke4(x,y) { \
                ((unsigned char *)y)[0] = (x & 0xff000000) >> 24; \
                ((unsigned char *)y)[1] = (x & 0x00ff0000) >> 16; \
                ((unsigned char *)y)[2] = (x & 0x0000ff00) >> 8; \
                ((unsigned char *)y)[3] = (x & 0x000000ff); }

	
