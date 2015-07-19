#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include <psp2/gxm.h>
#include <jpeglib.h>
#include "vita2d.h"

static vita2d_texture *_vita2d_load_JPEG_generic(struct jpeg_decompress_struct *jinfo, struct jpeg_error_mgr *jerr)
{
	int row_bytes;
	switch (jinfo->out_color_space) {
	case JCS_RGB:
		row_bytes = jinfo->image_width * 3;
		break;
	default:
		goto exit_error;
	}

	vita2d_texture *texture = vita2d_create_empty_texture_format(
		jinfo->image_width,
		jinfo->image_height,
		SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_RGBA);

	void *texture_data = vita2d_texture_get_datap(texture);

	JSAMPARRAY buffer = (JSAMPARRAY)malloc(sizeof(JSAMPROW));
	buffer[0] = (JSAMPROW)malloc(sizeof(JSAMPLE) * row_bytes);

	unsigned int i, color, *tex_ptr;
	unsigned char *jpeg_ptr;
	void *row_ptr = texture_data;
	jpeg_start_decompress(jinfo);

	int stride = vita2d_texture_get_width(texture) * 4;

	while (jinfo->output_scanline < jinfo->output_height) {
		jpeg_read_scanlines(jinfo, buffer, 1);
		tex_ptr = (row_ptr += stride);
		for (i = 0, jpeg_ptr = buffer[0]; i < jinfo->output_width; i++) {
			color = *(jpeg_ptr++);
			color |= *(jpeg_ptr++)<<8;
			color |= *(jpeg_ptr++)<<16;
			*(tex_ptr++) = color | 0xFF000000;
		}
	}

	free(buffer[0]);
	free(buffer);

	return texture;

exit_error:
	return NULL;
}


vita2d_texture *vita2d_load_JPEG_file(const char *filename)
{
	FILE *fp;
	if ((fp = fopen(filename, "rb")) < 0) {
		return NULL;
	}

	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;

	jinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&jinfo);
	jpeg_stdio_src(&jinfo, fp);
	jpeg_read_header(&jinfo, 1);

	vita2d_texture *texture = _vita2d_load_JPEG_generic(&jinfo, &jerr);

	jpeg_finish_decompress(&jinfo);
	jpeg_destroy_decompress(&jinfo);

	fclose(fp);
	return texture;
}


vita2d_texture *vita2d_load_JPEG_buffer(const void *buffer, unsigned long buffer_size)
{
	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;

	jinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&jinfo);
	jpeg_mem_src(&jinfo, (void *)buffer, buffer_size);
	jpeg_read_header(&jinfo, 1);

	vita2d_texture *texture = _vita2d_load_JPEG_generic(&jinfo, &jerr);

	jpeg_finish_decompress(&jinfo);
	jpeg_destroy_decompress(&jinfo);

	return texture;
}
