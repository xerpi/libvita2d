#include <psp2/pgf.h>
#include <psp2/kernel/sysmem.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
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
	return malloc(size);
}

static void pgf_free_func(void *userdata, void *buffer)
{
	free(buffer);
}

vita2d_pgf *vita2d_load_default_pgf()
{
	vita2d_pgf *font = malloc(sizeof(*font));
	if (!font)
		return NULL;

	SceFontNewLibParams params = {
		NULL,
		4,
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

	font->lib_handle = sceFontNewLib(&params, NULL);
	font->font_handle = sceFontOpenUserFile(font->lib_handle,
		0, 0, NULL);

	font->tex_atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
		SCE_GXM_TEXTURE_FORMAT_U8_R111);

	return font;
}

void vita2d_free_pgf(vita2d_pgf *font)
{
	sceFontClose(font->font_handle);
	sceFontDoneLib(font->lib_handle);
	free(font);
}

static int atlas_add_glyph(vita2d_pgf *font, unsigned int character)
{
	SceFontCharInfo char_info;
	sceFontGetCharInfo(font->font_handle, character, &char_info);

	unsigned char *buffer = malloc(char_info.bitmapWidth * char_info.bitmapHeight);

	SceFontGlyphImage glyph_image = {
		FONT_PIXELFORMAT_8,
		0,
		0,
		char_info.bitmapWidth,
		char_info.bitmapHeight,
		1 * char_info.bitmapWidth,
		0,
		(unsigned int)buffer
	};
	sceFontGetCharGlyphImage(font->font_handle, character, &glyph_image);

	printf("char: %c  w: %i, h: %i\n", character, char_info.bitmapWidth, char_info.bitmapHeight);

	int ret = texture_atlas_insert(font->tex_atlas, character, buffer,
		char_info.bitmapWidth, char_info.bitmapHeight,
		char_info.bitmapLeft, char_info.bitmapTop,
		char_info.sfp26AdvanceH >> 6,
		char_info.sfp26AdvanceV >> 6);

	free(buffer);

	return ret;
}

void vita2d_pgf_draw_text(vita2d_pgf *font, int x, int y, unsigned int color, unsigned int size, const char *text)
{
	int pen_x = x;
	int pen_y = y;

	while (*text) {
		unsigned int character = *text++;

		if (!texture_atlas_exists(font->tex_atlas, character)) {
			if (!atlas_add_glyph(font, character)) {
				continue;
			}
		}

		bp2d_rectangle rect;
		int bitmap_left, bitmap_top;
		int advance_x, advance_y;

		texture_atlas_get(font->tex_atlas, *text,
			&rect, &bitmap_left, &bitmap_top,
			&advance_x, &advance_y);

		vita2d_draw_texture_tint_part(font->tex_atlas->tex,
			pen_x + bitmap_left,
			pen_y - bitmap_top,
			rect.x, rect.y, rect.w, rect.h,
			color);

		pen_x += advance_x >> 6;
		pen_y += advance_y >> 6;
	}
}

void vita2d_pgf_draw_textf(vita2d_pgf *font, int x, int y, unsigned int color, unsigned int size, const char *text, ...)
{
	char buf[1024];
	va_list argptr;
	va_start(argptr, text);
	vsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);
	vita2d_pgf_draw_text(font, x, y, color, size, buf);
}
