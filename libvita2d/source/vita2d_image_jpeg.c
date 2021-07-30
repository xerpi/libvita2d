#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include <psp2/gxm.h>
#include <jpeglib.h>
#include "vita2d.h"

// Following official documentation max width or height of the texture is 4096
#define MAX_TEXTURE 4096

static vita2d_texture *_vita2d_load_JPEG_generic(struct jpeg_decompress_struct *jinfo, struct jpeg_error_mgr *jerr)
{
	unsigned int longer = jinfo->image_width > jinfo->image_height ? jinfo->image_width : jinfo->image_height;
	float downScale = (float)longer / (float)MAX_TEXTURE;
	jinfo->scale_denom = ceil(downScale);

	jpeg_start_decompress(jinfo);

	SceGxmTextureFormat out_format;

	if (jinfo->out_color_space == JCS_GRAYSCALE) {
		out_format = SCE_GXM_TEXTURE_FORMAT_U8_R111;
	} else {
		out_format = SCE_GXM_TEXTURE_FORMAT_U8U8U8_BGR;
	}

	vita2d_texture *texture = vita2d_create_empty_texture_format(
			jinfo->output_width,
			jinfo->output_height,
			out_format);

	if (!texture) {
		jpeg_abort_decompress(jinfo);
		return NULL;
	}

	void *texture_data = vita2d_texture_get_datap(texture);
	unsigned int row_stride = vita2d_texture_get_stride(texture);
	unsigned char *row_pointer = texture_data;

	while (jinfo->output_scanline < jinfo->output_height) {
		jpeg_read_scanlines(jinfo, (JSAMPARRAY)&row_pointer, 1);
		row_pointer += row_stride;
	}

	jpeg_finish_decompress(jinfo);

	return texture;
}


vita2d_texture *vita2d_load_JPEG_file(const char *filename)
{
	FILE *fp;
	if ((fp = fopen(filename, "rb")) <= 0) {
		return NULL;
	}

	unsigned int magic = 0;
	fread(&magic, 1, sizeof(unsigned int), fp);
	fseek(fp, 0, SEEK_SET);

	if (magic != 0xE0FFD8FF && magic != 0xE1FFD8FF && magic != 0xDBFFD8FF) {
		fclose(fp);
		return NULL;
	}

	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;

	jinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&jinfo);
	jpeg_stdio_src(&jinfo, fp);
	jpeg_read_header(&jinfo, 1);

	vita2d_texture *texture = _vita2d_load_JPEG_generic(&jinfo, &jerr);

	jpeg_destroy_decompress(&jinfo);

	fclose(fp);
	return texture;
}


vita2d_texture *vita2d_load_JPEG_buffer(const void *buffer, unsigned long buffer_size)
{
	unsigned int magic = *(unsigned int *)buffer;
	if (magic != 0xE0FFD8FF && magic != 0xE1FFD8FF) {
		return NULL;
	}

	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;

	jinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&jinfo);
	jpeg_mem_src(&jinfo, (void *)buffer, buffer_size);
	jpeg_read_header(&jinfo, 1);

	vita2d_texture *texture = _vita2d_load_JPEG_generic(&jinfo, &jerr);

	jpeg_destroy_decompress(&jinfo);

	return texture;
}
