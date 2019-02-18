#include <psp2/pvf.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
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

#define PVF_GLYPH_MARGIN 2

typedef struct vita2d_pvf_font_handle {
	ScePvfFontId font_handle;
	int (*in_font_group)(unsigned int c);
	struct vita2d_pvf_font_handle *next;
} vita2d_pvf_font_handle;

typedef struct vita2d_pvf {
	ScePvfLibId lib_handle;
	vita2d_pvf_font_handle *font_handle_list;
	texture_atlas *atlas;
	SceKernelLwMutexWork mutex;
	float vsize;
} vita2d_pvf;

static void *pvf_alloc_func(void *userdata, unsigned int size)
{
	return memalign(sizeof(int), (size + sizeof(int) - 1) / sizeof(int) * sizeof(int));
}

static void *pvf_realloc_func(void *userdata, void *old_ptr, unsigned int size)
{
	return realloc(old_ptr, (size + sizeof(int) - 1) / sizeof(int) * sizeof(int));
}

static void pvf_free_func(void *userdata, void *p)
{
	free(p);
}

static void vita2d_load_pvf_post(vita2d_pvf *font)
{
	ScePvfFontInfo fontinfo;

	scePvfGetFontInfo(font->font_handle_list->font_handle, &fontinfo);
	//font->vsize = (fontinfo.fontStyleInfo.fontV / fontinfo.fontStyleInfo.fontVRes)
		//* SCREEN_DPI;
	font->vsize = 10.125f;

	font->atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
		SCE_GXM_TEXTURE_FORMAT_U8_R111);

	sceKernelCreateLwMutex(&font->mutex, "vita2d_pvf_mutex", 2, 0, NULL);
}

static vita2d_pvf *vita2d_load_pvf_pre(int numFonts)
{
	ScePvfError error;

	vita2d_pvf *font = malloc(sizeof(*font));
	if (!font)
		return NULL;

	memset(font, 0, sizeof(vita2d_pvf));

	ScePvfInitRec params = {
		font,
		numFonts,
		NULL,
		NULL,
		pvf_alloc_func,
		pvf_realloc_func,
		pvf_free_func
	};

	font->lib_handle = scePvfNewLib(&params, &error);
	if (error != 0) {
		free(font);
		return NULL;
	}

	scePvfSetEM(font->lib_handle, 72.0f / (10.125f * 128.0f));
	scePvfSetResolution(font->lib_handle, 128.0f, 128.0f);

	return font;
}

vita2d_pvf *vita2d_load_system_pvf(int numFonts, const vita2d_system_pvf_config *configs)
{
	if (numFonts < 1) {
		return NULL;
	}

	ScePvfError error;
	int i;

	vita2d_pvf *font = vita2d_load_pvf_pre(numFonts);

	if (!font)
		return NULL;

	vita2d_pvf_font_handle *tmp = NULL;

	for (i = 0; i < numFonts; i++) {
		ScePvfFontStyleInfo style;

		memset(&style, 0, sizeof(style));
		style.languageCode = configs[i].code;
		style.familyCode = SCE_PVF_DEFAULT_FAMILY_CODE;
		style.style = SCE_PVF_DEFAULT_STYLE_CODE;

		ScePvfFontIndex index = scePvfFindOptimumFont(font->lib_handle, &style, &error);
		if (error != 0)
			goto cleanup;

		ScePvfFontId handle = scePvfOpen(font->lib_handle, index, 0, &error);
		if (error != 0)
			goto cleanup;

		scePvfSetCharSize(handle, 10.125f, 10.125f);

		if (font->font_handle_list == NULL) {
			tmp = font->font_handle_list = malloc(sizeof(vita2d_pvf_font_handle));
		} else {
			tmp = tmp->next = malloc(sizeof(vita2d_pvf_font_handle));
		}
		if (!tmp) {
			scePvfClose(handle);
			goto cleanup;
		}

		memset(tmp, 0, sizeof(vita2d_pvf_font_handle));
		tmp->font_handle = handle;
		tmp->in_font_group = configs[i].in_font_group;
	}

	vita2d_load_pvf_post(font);

	return font;

cleanup:
	tmp = font->font_handle_list;
	while (tmp) {
		scePvfClose(tmp->font_handle);
		vita2d_pvf_font_handle *next = tmp->next;
		free(tmp);
		tmp = next;
	}
	scePvfDoneLib(font->lib_handle);
	free(font);
	return NULL;
}

vita2d_pvf *vita2d_load_default_pvf()
{
	vita2d_system_pvf_config configs[] = {
		{SCE_PVF_DEFAULT_LANGUAGE_CODE, NULL},
	};

	return vita2d_load_system_pvf(1, configs);
}

vita2d_pvf *vita2d_load_custom_pvf(const char *path)
{
	ScePvfError error;
	vita2d_pvf *font = vita2d_load_pvf_pre(1);

	if (!font)
		return NULL;

	vita2d_pvf_font_handle *handle = malloc(sizeof(vita2d_pvf_font_handle));
	if (!handle) {
		free(font);
		return NULL;
	}

	ScePvfFontId font_handle = scePvfOpenUserFile(font->lib_handle, (char *)path, 1, &error);
	if (error != 0) {
		scePvfDoneLib(font->lib_handle);
		free(handle);
		free(font);
		return NULL;
	}

	memset(handle, 0, sizeof(vita2d_pvf_font_handle));
	handle->font_handle = font_handle;
	font->font_handle_list = handle;

	vita2d_load_pvf_post(font);

	return font;
}

void vita2d_free_pvf(vita2d_pvf *font)
{
	if (font) {
		sceKernelDeleteLwMutex(&font->mutex);

		vita2d_pvf_font_handle *tmp = font->font_handle_list;
		while (tmp) {
			scePvfClose(tmp->font_handle);
			vita2d_pvf_font_handle *next = tmp->next;
			free(tmp);
			tmp = next;
		}
		scePvfDoneLib(font->lib_handle);
		texture_atlas_free(font->atlas);
		free(font);
	}
}

ScePvfFontId get_font_for_character(vita2d_pvf *font, unsigned int character)
{
	ScePvfFontId font_handle = font->font_handle_list->font_handle;
	vita2d_pvf_font_handle *tmp = font->font_handle_list;

	while (tmp) {
		if (tmp->in_font_group == NULL || tmp->in_font_group(character)) {
			font_handle = tmp->font_handle;
			break;
		}
		tmp = tmp->next;
	}

	return font_handle;
}

static int atlas_add_glyph(vita2d_pvf *font, ScePvfFontId font_handle, unsigned int character)
{
	ScePvfCharInfo char_info;
	ScePvfIrect char_image_rect;
	bp2d_position position;
	void *texture_data;
	vita2d_texture *tex = font->atlas->texture;

	if (scePvfGetCharInfo(font_handle, character, &char_info) < 0)
		return 0;

	if (scePvfGetCharImageRect(font_handle, character, &char_image_rect) < 0)
		return 0;

	bp2d_size size = {
		char_image_rect.width + 2 * PVF_GLYPH_MARGIN,
		char_image_rect.height + 2 * PVF_GLYPH_MARGIN
	};

	texture_atlas_entry_data data = {
		char_info.glyphMetrics.horizontalBearingX64 >> 6,
		char_info.glyphMetrics.horizontalBearingY64 >> 6,
		char_info.glyphMetrics.horizontalAdvance64,
		char_info.glyphMetrics.verticalAdvance64,
		0
	};

	if (!texture_atlas_insert(font->atlas, character, &size, &data,
				  &position))
			return 0;

	texture_data = vita2d_texture_get_datap(tex);

	ScePvfUserImageBufferRec glyph_image;
	glyph_image.pixelFormat = SCE_PVF_USERIMAGE_DIRECT8;
	glyph_image.xPos64 = ((position.x + PVF_GLYPH_MARGIN) << 6) - char_info.glyphMetrics.horizontalBearingX64;
	glyph_image.yPos64 = ((position.y + PVF_GLYPH_MARGIN) << 6) + char_info.glyphMetrics.horizontalBearingY64;
	glyph_image.rect.width = vita2d_texture_get_width(tex);
	glyph_image.rect.height = vita2d_texture_get_height(tex);
	glyph_image.bytesPerLine = vita2d_texture_get_stride(tex);
	glyph_image.reserved = 0;
	glyph_image.buffer = (ScePvfU8 *)texture_data;

	return scePvfGetCharGlyphImage(font_handle, character, &glyph_image) == 0;
}

int generic_pvf_draw_text(vita2d_pvf *font, int draw, int *height,
			  int x, int y, unsigned int color, float scale,
			  const char *text)
{
	sceKernelLockLwMutex(&font->mutex, 1, NULL);

	int i;
	unsigned int character;
	ScePvfFontId fontid;
	bp2d_rectangle rect;
	texture_atlas_entry_data data;
	ScePvfKerningInfo kerning_info;
	unsigned int old_character = 0;
	vita2d_texture *tex = font->atlas->texture;
	int start_x = x;
	int max_x = 0;
	int pen_x = x;
	int pen_y = y;

	for (i = 0; text[i];) {
		i += utf8_to_ucs2(&text[i], &character);

		if (character == '\n') {
			if (pen_x > max_x)
				max_x = pen_x;
			pen_x = start_x;
			pen_y += font->vsize * scale;
			continue;
		}

		fontid = get_font_for_character(font, character);

		if (!texture_atlas_get(font->atlas, character, &rect, &data)) {
			if (!atlas_add_glyph(font, fontid, character))
				continue;

			if (!texture_atlas_get(font->atlas, character,
					       &rect, &data))
					continue;
		}

		if (old_character) {
			if (scePvfGetKerningInfo(fontid, old_character, character, &kerning_info) >= 0) {
				pen_x += kerning_info.fKerningInfo.xOffset;
				pen_y += kerning_info.fKerningInfo.yOffset;
			}
		}

		if (draw) {
			vita2d_draw_texture_tint_part_scale(tex,
				pen_x + data.bitmap_left * scale,
				pen_y - data.bitmap_top * scale,
				rect.x + PVF_GLYPH_MARGIN / 2.0f, rect.y + PVF_GLYPH_MARGIN / 2.0f,
				rect.w - PVF_GLYPH_MARGIN / 2.0f, rect.h - PVF_GLYPH_MARGIN / 2.0f,
				scale,
				scale,
				color);
		}

		pen_x += (data.advance_x >> 6) * scale;
		old_character = character;
	}

	if (pen_x > max_x)
		max_x = pen_x;

	if (height)
		*height = pen_y + font->vsize * scale - y;

	sceKernelUnlockLwMutex(&font->mutex, 1);

	return max_x - x;
}

int vita2d_pvf_draw_text(vita2d_pvf *font, int x, int y,
			 unsigned int color, float scale,
			 const char *text)
{
	return generic_pvf_draw_text(font, 1, NULL, x, y, color, scale, text);
}

int vita2d_pvf_draw_textf(vita2d_pvf *font, int x, int y,
			  unsigned int color, float scale,
			  const char *text, ...)
{
	char buf[1024];
	va_list argptr;
	va_start(argptr, text);
	vsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);
	return vita2d_pvf_draw_text(font, x, y, color, scale, buf);
}

void vita2d_pvf_text_dimensions(vita2d_pvf *font, float scale,
				const char *text, int *width, int *height)
{
	int w;
	w = generic_pvf_draw_text(font, 0, height, 0, 0, 0, scale, text);

	if (width)
		*width = w;
}

int vita2d_pvf_text_width(vita2d_pvf *font, float scale, const char *text)
{
	int width;
	vita2d_pvf_text_dimensions(font, scale, text, &width, NULL);
	return width;
}

int vita2d_pvf_text_height(vita2d_pvf *font, float scale, const char *text)
{
	int height;
	vita2d_pvf_text_dimensions(font, scale, text, NULL, &height);
	return height;
}
