#include <psp2/kernel/sysmem.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
#include "vita2d.h"
#include "texture_atlas.h"
#include "bin_packing_2d.h"
#include "utils.h"
#include "shared.h"

#define ATLAS_DEFAULT_W 512
#define ATLAS_DEFAULT_H 512

typedef struct vita2d_font {
	FT_Library ftlibrary;
	FT_Face face;
	texture_atlas *tex_atlas;
} vita2d_font;

vita2d_font *vita2d_load_font_file(const char *pathname)
{
	FT_Error error;

	vita2d_font *font = malloc(sizeof(*font));
	if (!font)
		return NULL;

	error = FT_Init_FreeType(&font->ftlibrary);
	if (error != FT_Err_Ok) {
		free(font);
		return NULL;
	}

	error = FT_New_Face(
		font->ftlibrary,
		pathname,
		0,
		&font->face);
	if (error != FT_Err_Ok) {
		FT_Done_FreeType(font->ftlibrary);
		free(font);
		return NULL;
	}

	FT_Select_Charmap(font->face, FT_ENCODING_UNICODE);

	font->tex_atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
		SCE_GXM_TEXTURE_FORMAT_U8_R111);

	return font;
}

vita2d_font *vita2d_load_font_mem(const void *buffer, unsigned int size)
{
	FT_Error error;

	vita2d_font *font = malloc(sizeof(*font));
	if (!font)
		return NULL;

	error = FT_Init_FreeType(&font->ftlibrary);
	if (error != FT_Err_Ok) {
		free(font);
		return NULL;
	}

	error = FT_New_Memory_Face(
		font->ftlibrary,
		buffer,
		size,
		0,
		&font->face);
	if (error != FT_Err_Ok) {
		FT_Done_FreeType(font->ftlibrary);
		free(font);
		return NULL;
	}

	FT_Select_Charmap(font->face, FT_ENCODING_UNICODE);

	font->tex_atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
		SCE_GXM_TEXTURE_FORMAT_U8_R111);

	return font;
}

void vita2d_free_font(vita2d_font *font)
{
	texture_atlas_free(font->tex_atlas);
	FT_Done_Face(font->face);
	FT_Done_FreeType(font->ftlibrary);
	free(font);
}

static int atlas_add_glyph(texture_atlas *atlas, unsigned int glyph_index, const FT_GlyphSlot slot)
{
	const FT_Bitmap *bitmap = &slot->bitmap;

	unsigned char *buffer = malloc(bitmap->width * bitmap->rows);
	unsigned int w = bitmap->width;
	unsigned int h = bitmap->rows;

	int j, k;
	for (j = 0; j < h; j++) {
		for (k = 0; k < w; k++) {
			if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
				buffer[j*w + k] =
					(bitmap->buffer[j*bitmap->pitch + k/8] & (1 << (7 - k%8)))
					? 0xFF : 0;
			} else {
				buffer[j*w + k] = bitmap->buffer[j*bitmap->pitch + k];
			}
		}
	}

	int ret = texture_atlas_insert(atlas, glyph_index, buffer,
		bitmap->width, bitmap->rows,
		slot->bitmap_left, slot->bitmap_top,
		slot->advance.x, slot->advance.y);

	free(buffer);

	return ret;
}

void vita2d_draw_text(vita2d_font *font, int x, int y, unsigned int color, unsigned int size, const char *text)
{
	FT_Face face = font->face;
	FT_GlyphSlot slot = face->glyph;
	FT_UInt glyph_index;
	FT_Bool use_kerning = FT_HAS_KERNING(face);
	FT_UInt previous = 0;
	int pen_x = x;
	int pen_y = y + size;

	FT_Set_Pixel_Sizes(face, 0, size);

	while (*text) {
		glyph_index = FT_Get_Char_Index(face, *text++);

		if (use_kerning && previous && glyph_index) {
			FT_Vector delta;
			FT_Get_Kerning(face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
			pen_x += delta.x >> 6;
		}

		if (!texture_atlas_exists(font->tex_atlas, glyph_index)) {
			if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT)) continue;
			if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) continue;

			if (!atlas_add_glyph(font->tex_atlas, glyph_index, slot)) {
				continue;
			}
		}

		bp2d_rectangle rect;
		int bitmap_left, bitmap_top;
		int advance_x, advance_y;

		texture_atlas_get(font->tex_atlas, glyph_index,
			&rect, &bitmap_left, &bitmap_top,
			&advance_x, &advance_y);

		vita2d_draw_texture_tint_part(font->tex_atlas->tex,
			pen_x + bitmap_left,
			pen_y - bitmap_top,
			rect.x, rect.y, rect.w, rect.h,
			color);

		pen_x += advance_x >> 6;
		pen_y += advance_y >> 6;

		previous = glyph_index;
	}
}

void vita2d_draw_textf(vita2d_font *font, int x, int y, unsigned int color, unsigned int size, const char *text, ...)
{
	char buf[1024];
	va_list argptr;
	va_start(argptr, text);
	vsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);
	vita2d_draw_text(font, x, y, color, size, buf);
}

void vita2d_font_text_dimensions(vita2d_font *font, unsigned int size, const char *text, int *width, int *height)
{
	FT_Face face = font->face;
	FT_GlyphSlot slot = face->glyph;
	FT_UInt glyph_index;
	FT_Bool use_kerning = FT_HAS_KERNING(face);
	FT_UInt previous = 0;
	int pen_x = 0;

	FT_Set_Pixel_Sizes(face, 0, size);

	while (*text) {
		glyph_index = FT_Get_Char_Index(face, *text++);

		if (use_kerning && previous && glyph_index) {
			FT_Vector delta;
			FT_Get_Kerning(face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
			pen_x += delta.x >> 6;
		}

		if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT)) continue;

		pen_x += slot->advance.x >> 6;

		previous = glyph_index;
	}
	if (width)
		*width = pen_x;
	if (height)
		*height = face->size->metrics.height >> 6;
}

int vita2d_font_text_width(vita2d_font *font, unsigned int size, const char *text)
{
	int width;
	vita2d_font_text_dimensions(font, size, text, &width, NULL);
	return width;
}

int vita2d_font_text_height(vita2d_font *font, unsigned int size, const char *text)
{
	FT_Set_Pixel_Sizes(font->face, 0, size);
	return font->face->size->metrics.height >> 6;
}
