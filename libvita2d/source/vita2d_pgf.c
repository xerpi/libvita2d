#include <psp2/pgf.h>
#include <psp2/kernel/sysmem.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <math.h>
#include "vita2d.h"
#include "texture_atlas.h"
#include "bin_packing_2d.h"
#include "utils.h"
#include "shared.h"

#define ATLAS_DEFAULT_W 512
#define ATLAS_DEFAULT_H 512

typedef struct vita2d_pgf {
	SceFontLibHandle lib_handle;
	SceFontHandle font_handle;
	texture_atlas *tex_atlas;
} vita2d_pgf;

static void *pgf_alloc_func(void *userdata, unsigned int size)
{
	return memalign(sizeof(int), size + sizeof(int));
}

static void pgf_free_func(void *userdata, void *p)
{
	free(p);
}

vita2d_pgf *vita2d_load_default_pgf()
{
	unsigned int error;

	vita2d_pgf *font = malloc(sizeof(*font));
	if (!font)
		return NULL;

	SceFontNewLibParams params = {
		font,
		1,
		NULL,
		pgf_alloc_func,
		pgf_free_func,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	};

	font->lib_handle = sceFontNewLib(&params, &error);
	if (error != 0) {
		free(font);
		return NULL;
	}

	font->font_handle = sceFontOpen(font->lib_handle, 1, 0, &error);
	if (error != 0) {
		sceFontDoneLib(font->lib_handle);
		free(font);
		return NULL;
	}

	font->tex_atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
		SCE_GXM_TEXTURE_FORMAT_U8_R111);

	return font;
}

void vita2d_free_pgf(vita2d_pgf *font)
{
	if (font) {
		sceFontClose(font->font_handle);
		sceFontDoneLib(font->lib_handle);
		texture_atlas_free(font->tex_atlas);
		free(font);
	}
}

static int atlas_add_glyph(vita2d_pgf *font, unsigned int character, int glyph_size)
{
	SceFontCharInfo char_info;
	if (sceFontGetCharInfo(font->font_handle, character, &char_info) < 0)
		return 0;

	int pos_x;
	int pos_y;
	if (!texture_atlas_insert(font->tex_atlas, character,
		char_info.bitmapWidth, char_info.bitmapHeight,
		char_info.bitmapLeft, char_info.bitmapTop,
		char_info.sfp26AdvanceH,
		char_info.sfp26AdvanceV,
		glyph_size, &pos_x, &pos_y))
			return 0;


	SceFontGlyphImage glyph_image;
	glyph_image.pixelFormat = SCE_FONT_PIXELFORMAT_8;
	glyph_image.xPos64 = pos_x*64;
	glyph_image.yPos64 = pos_y*64;
	glyph_image.bufWidth = 512;
	glyph_image.bufHeight = 512;
	glyph_image.bytesPerLine = 512;
	glyph_image.pad = 0;
	glyph_image.bufferPtr = (unsigned int)vita2d_texture_get_datap(font->tex_atlas->tex);

	sceFontGetCharGlyphImage(font->font_handle, character, &glyph_image);

	return 0;
}

int vita2d_pgf_draw_text(vita2d_pgf *font, int x, int y, unsigned int color, unsigned int size, const char *text)
{
	int pen_x = x;
	int pen_y = y + 20;

	while (*text) {
		unsigned int character = *text++;

		if (!texture_atlas_exists(font->tex_atlas, character)) {
			if (!atlas_add_glyph(font, character, size)) {
				continue;
			}
		}

		bp2d_rectangle rect;
		int bitmap_left, bitmap_top;
		int advance_x, advance_y;
		int glyph_size;

		if (!texture_atlas_get(font->tex_atlas, character,
			&rect, &bitmap_left, &bitmap_top,
			&advance_x, &advance_y, &glyph_size))
				continue;

		//const float draw_scale = size/(float)glyph_size;
		const float draw_scale = 1.0f;

		vita2d_draw_texture_tint_part_scale(font->tex_atlas->tex,
			pen_x + bitmap_left * draw_scale,
			pen_y - bitmap_top * draw_scale,
			rect.x, rect.y, rect.w, rect.h,
			draw_scale,
			draw_scale,
			color);

		pen_x += (advance_x >> 6) * draw_scale;
		//pen_y += (advance_y >> 6) * draw_scale;
	}

	return pen_x - x;
}

int vita2d_pgf_draw_textf(vita2d_pgf *font, int x, int y, unsigned int color, unsigned int size, const char *text, ...)
{
	char buf[1024];
	va_list argptr;
	va_start(argptr, text);
	vsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);
	return vita2d_pgf_draw_text(font, x, y, color, size, buf);
}

void vita2d_pgf_text_dimensions(vita2d_pgf *font, unsigned int size, const char *text, int *width, int *height)
{
	int pen_x = 0;
	int pen_y = 0;

	while (*text) {
		unsigned int character = *text++;

		if (!texture_atlas_exists(font->tex_atlas, character)) {
			if (!atlas_add_glyph(font, character, size)) {
				continue;
			}
		}

		bp2d_rectangle rect;
		int bitmap_left, bitmap_top;
		int advance_x, advance_y;
		int glyph_size;

		if (!texture_atlas_get(font->tex_atlas, character,
			&rect, &bitmap_left, &bitmap_top,
			&advance_x, &advance_y, &glyph_size))
				continue;

		//const float draw_scale = size/(float)glyph_size;
		const float draw_scale = 1.0f;

		pen_x += (advance_x >> 6) * draw_scale;
		//pen_y += (advance_y >> 6) * draw_scale;
	}

	if (width)
		*width = pen_x;
	if (height)
		*height = size + pen_y;
}

int vita2d_pgf_text_width(vita2d_pgf *font, unsigned int size, const char *text)
{
	int width;
	vita2d_pgf_text_dimensions(font, size, text, &width, NULL);
	return width;
}

int vita2d_pgf_text_height(vita2d_pgf *font, unsigned int size, const char *text)
{
	int height;
	vita2d_pgf_text_dimensions(font, size, text, NULL, &height);
	return height;
}
