/* CD (CIL Data) functions -- create and write new data (stack) format */
/*
 *  $Log: CDFunctions.c,v $
 *  Revision 1.3  2010/04/02 19:13:34  stanley
 *  added environment variable for dictionary
 *
 *  Revision 1.2  2010/01/13 00:32:12  stanley
 *  fixed rcsid
 *
 *  Revision 1.1  2010/01/13 00:24:37  stanley
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
                                                                                
/* "$Id: CDFunctions.c 172 2016-03-25 21:03:51Z stanley $ */


#include "CD.h"
#include <stdio.h>

/* a common temporary buffer for string output */
char line[1024];

/* simple state maintenance for multiple output streams */
/* id is the file descriptor. */
/* all it maintains now is the level of "tag", top level xml */

/* find the state data for the fd, return index or -1 */
int
CDfdToIndex( int fd )
{
int i;
	for(i=0; i<sizeof(CDstate); i++ ) {
		if( fd == CDstate[i].fd ) 
			return i;
	}
	return -1;
}

/* create a new state entry for the fd. */
void
CDnewState( int fd )
{
int i, j;

	/* get open slot -- shortcut: fd==0 */
	i = CDfdToIndex( 0 );
	if( -1 == i ) {
		fprintf( stderr, "CDnewState: no room for new state\n" );
		return; /* no space, no state */
	}

	CDstate[i].fd = fd;
	CDstate[i].tagDepth = 0;

	for(j=0; j<MAX_TAGS; j++) 
		CDstate[i].tagNames[j].name[0] = 0;

	for(j=0; j<MAX_DICT_ENTRIES; j++) {
		CDstate[i].dict[j].name[0] = 0;
		CDstate[i].dict[j].value = -1;
		CDstate[i].dict[j].type = CDunknown;
	}

	return;
}

/* clear out state info for this fd */
void
CDclearState( int fd )
{
int i;
	i = CDfdToIndex( fd );
	if( -1 == i ) return;
	CDstate[i].fd = 0;
	CDstate[i].tagDepth = 0;
	return;
}


	
/* CDOpenHeader -- start a new XML data header */
/* creates the <root> tag, allocates a state */
/* root tag is NOT contained in state, so we can */
/* later close all tags just prior to writing dict */
void
CDOpenHeader( int fd )
{
	CDnewState( fd );
	sprintf( line, "<root>\n" );
	write( fd, line, strlen(line) );
}

/* write an XML toolbox style root header entry */
/* fd is the output file descriptor
 * type is the matlab type of the INPUT data
 * name is the name of the header (will become the variable name)
 * value is a pointer to the data, Nx1
 * length is the number of entries
 *
 *    length IGNORED for character class 
 */
void
CDHeader( int fd, mxClassID type, char *name, void *value, long length )
{
long i;
char *form;
int issigned = 0;

	switch( type ) {

	case mxCHAR_CLASS:
		sprintf( line, "<%s type=\"char\">%s</%s>\n", name, (char *)value, name );
		write( fd, line, strlen(line) );
		break;
	case mxINT8_CLASS:
		issigned++;
	case mxUINT8_CLASS:
		sprintf( line, "<%s type=\"double\">", name );
		write( fd, line, strlen(line) );
		for( i=0; i<length; i++ ) {
			if( issigned ) 
			sprintf( line, "%d ", ((char *)value)[i] );
			else
			sprintf( line, "%u ", ((unsigned char *)value)[i] );
			write( fd, line, strlen(line) );
		}
		sprintf( line, "</%s>\n", name );
		write( fd, line, strlen(line) );
		break;
	case mxINT16_CLASS:
		issigned++;
	case mxUINT16_CLASS:
		sprintf( line, "<%s type=\"double\">", name );
		write( fd, line, strlen(line) );
		for( i=0; i<length; i++ ) {
			if( issigned )
			sprintf( line, "%d ", ((short *)value)[i] );
			else
			sprintf( line, "%u ", ((unsigned short *)value)[i] );
			write( fd, line, strlen(line) );
		}
		sprintf( line, "</%s>\n", name );
		write( fd, line, strlen(line) );
		break;
	case mxINT32_CLASS:
		issigned++;
	case mxUINT32_CLASS:
		sprintf( line, "<%s type=\"double\">", name );
		write( fd, line, strlen(line) );
		for( i=0; i<length; i++ ) {
			if( issigned )
			sprintf( line, "%d ", ((long *)value)[i] );
			else
			sprintf( line, "%u ", ((unsigned long *)value)[i] );
			write( fd, line, strlen(line) );
		}
		sprintf( line, "</%s>\n", name );
		write( fd, line, strlen(line) );
		break;
	default:
		sprintf( line, "<%s type=\"double\">", name );
		write( fd, line, strlen(line) );
		for( i=0; i<length; i++ ) {
			sprintf( line, "%f ", ((double *)value)[i] );
			write( fd, line, strlen(line) );
		}
		sprintf( line, "</%s>\n", name );
		write( fd, line, strlen(line) );
		break;
	}

}


/* Open a tag (root, params, etc.) */
void
CDOpenTag( int fd, char *tag )
{
int i, j;
	i = CDfdToIndex( fd );
	if( i>=0 ) {
		j = ++CDstate[i].tagDepth;
		if( j > MAX_TAGS ) {
			fprintf( stderr, "CDOpenTag: Too many open tags!\n" );
			return;
		}
		strcpy( CDstate[i].tagNames[j].name, tag );
	}
	sprintf( line, "<%s>\n", tag );
	write( fd, line, strlen(line) );
}

/* write a closing tag (/root, /params, /etc.) */
void
CDCloseTag( int fd )
{
int i, j;
	i = CDfdToIndex( fd );
	if( i>=0 ) {
		j = CDstate[i].tagDepth--;
		if( j >= 0 ) {
			sprintf( line, "</%s>\n", CDstate[i].tagNames[j].name );
			write( fd, line, strlen(line) );
		}
	}
}

/* close all open tags */
void
CDCloseAllTags( int fd )
{
int i, j;
	i = CDfdToIndex( fd );
	if( i < 0 ) return;
	for( j=CDstate[i].tagDepth; j>0; j-- ) {
		sprintf( line, "</%s>\n", CDstate[i].tagNames[j].name );
		write( fd, line, strlen(line) );
	}
	CDstate[i].tagDepth = 0;
}
		

/* copy the default dictionary into the data file */
/* The default defines epoch, shutter, gain and data */
/* it is stored in the file dict.xml */
void
CDDefaultDictionary( int fd, char *fname )
{
FILE *nfd;
char dfile[256];
char *ptr;
int i;
int j;

	i = CDfdToIndex( fd );
	j = 0;

	if( NULL == fname ) {
		if( NULL == getenv("STACK_DICT") )
			strcpy( dfile, DEFAULT_DICTIONARY );
		else
			strcpy( dfile, getenv("STACK_DICT") );
	}
	else
		strcpy( dfile, fname );
	
	nfd = fopen( dfile, "r" );
	if( NULL == nfd ) {
		printf("No default dictionary found\n");
		return;
	}
	while( fgets( line, sizeof(line), nfd )  ) {
		write( fd, line, strlen(line) );
		if( i >= 0 ) {
			/* find second > in line */
			ptr = line;
			ptr = index( ptr, '>' );
			ptr = index( ++ptr, '>' );
			if( NULL == ptr ) continue;
			sscanf( ++ptr, "%d", &CDstate[i].dict[j].value );
			ptr = index( ++ptr, '>' );
			ptr = index( ++ptr, '>' );
			sscanf( ++ptr, "%[a-z]", line );
			ptr = index( ++ptr, '>' );
			ptr = index( ++ptr, '>' );
			sscanf( ++ptr, "%[a-zA-Z0-9]", CDstate[i].dict[j].name );
	 		if( !strcmp( line, "compressed" ) ) CDstate[i].dict[j].type = CDcompressedType;
	 		if( !strcmp( line, "double" ) ) CDstate[i].dict[j].type = CDdoubleType;
	 		if( !strcmp( line, "blob" ) ) CDstate[i].dict[j].type = CDblobType;
			j++;
		}
	}
	fclose( nfd );

}


/* write the closing to the header -- /root and then start the */
/* data with <data>. At that point, we leave XML and switch to */
/* binary, so there will not be a /data to close this. */
	
void 
CDCloseHeader( int fd )
{
int i;
	i = CDfdToIndex(fd);
	if( i>0 )
	    if( CDstate[i].tagDepth ) 
		printf("You have unclosed tags!\n");

/*	CDclearState(fd); */
	sprintf( line, "</root>\n<data>\n" );
	write( fd, line, strlen(line) );
}

/* fd     = file descriptor to write to 
   tag    = what data this is, enumerated in CD.h, defined by dict.xml
   type   = type of data in MATLAB terms (mxDOUBLE_CLASS, etc.)
   ndims  = number of dims (min=2, max=1000)
   dims   = ndims longs with dimension data (M, N, ...) 
   length = number of elements. if > 1, data are written as blob
             and length is ignored (actual numelements is calculated
	     from dim data
   data   = pointer to raw data */

int
CDWriteData( int fd, 
		char *name,
		long type, 
		long ndims, 
		long *dims, 
		unsigned long length, 
		void *data )
{
unsigned long aLong;
unsigned long longData[1024];
unsigned char aChar;
int myLen;
unsigned long totalLen;
int i;
int state;
int tag;

	state = CDfdToIndex( fd );

	/* find dictionary tag from name */
	for( i=0; i<MAX_DICT_ENTRIES; i++ ) 
		if( !strcmp( CDstate[state].dict[i].name, name ) )
			break;

	if( i == MAX_DICT_ENTRIES ) {
		fprintf(stderr, "CDWriteData: cannot find tag %s\n", name );
		return;
	}

	tag = CDstate[state].dict[i].value;


	/* get length of one element of associated type */
	myLen = typeLen[type];
	if( 0 == myLen ) {
		fprintf( stderr, 
			"Unsupported data type %d in CDWriteData\n", 
			type );
		return -1;
	}

	/* simple cases first */
	/* length == 1, no blob */
	if( length == 1 ) {
		/* output the data tag */
		aChar = tag;
		write( fd, &aChar, 1 );
		/* and write ONE of the things */
		write( fd, data, myLen );
		return 0;
	}

	/* more than one thing to write, we blob it! */
	/* THE CALLER MUST COOPERATE BY USING A TAG WITH BLOB TYPE */
        /* tag datalength type ndims dim1 ... dimn data */

	/*if( tag < MIN_BLOB_TAG ) {
		fprintf( stderr,
			"Cannot write blob data with scalar tag %d\n",
			tag );
		return -1;
	}
	*/

	aChar = tag; /* one byte tag value */

	/* poke the matlab type */
	poke4(type, &longData[1]);

	/* poke the number of dimensions */
	poke4( ndims, &longData[2]);

	/* calculate the number of elements from the dimension data */
	totalLen = myLen;
	for( i=0; i<ndims; i++ ) {
		totalLen *= dims[i];
		/* and poke the dimension data */
		poke4(dims[i],&longData[i+3]);
	}
	/* go back to front and save total length of blob in bytes */
	longData[0] = totalLen + ndims*4 + 4 + 4;

	/* write it all out */
	write( fd, &aChar, 1 );
	write( fd, longData, ndims*4+4+4+4 );
	write( fd, data, totalLen );

}

/* have kept state information open all through data writes */
/* since dictionary is in state now. must close out state at */
/* end of data writing to release that state for reuse. */

void
CDEndData( int fd )
{

	CDclearState( fd );

}


