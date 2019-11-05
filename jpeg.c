#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jpeglib.h>

int
compressjpg(unsigned char *image, int width, int height)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1];
	unsigned char *buf;
	size_t buflen = 0;
	int length;

	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_compress(&cinfo);

	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = 3;

	length = width * height * 3;

	jpeg_mem_dest( &cinfo, &buf, &buflen );
	jpeg_set_defaults( &cinfo );

	jpeg_start_compress( &cinfo, TRUE );
	while( cinfo.next_scanline < cinfo.image_height ) {
		row_pointer[0] = &image[ cinfo.next_scanline * cinfo.image_width * cinfo.input_components];

		jpeg_write_scanlines( &cinfo, row_pointer, 1 );
	}

	jpeg_finish_compress( &cinfo );

	if (buflen < length)
		length = buflen;
	memcpy(image, buf, length);

	jpeg_destroy_compress( &cinfo );
	free(buf);

	return length;
}

