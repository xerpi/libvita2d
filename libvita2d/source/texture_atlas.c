#include <stdlib.h>
#include <string.h>
#include "texture_atlas.h"

texture_atlas *texture_atlas_create(int width, int height)
{
	texture_atlas *atlas = malloc(sizeof(*atlas));
	if (!atlas)
		return NULL;

	bp2d_rectangle rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = width;
	rect.h = height;

	atlas->tex = vita2d_create_empty_texture(width, height);
	atlas->bp_root = bp2d_create(&rect);
	atlas->htab = int_htab_create();

	return atlas;
}

void texture_atlas_free(texture_atlas *atlas)
{
	vita2d_free_texture(atlas->tex);
	bp2d_free(atlas->bp_root);
	int_htab_free(atlas->htab);
	free(atlas);
}

int texture_atlas_insert(texture_atlas *atlas, unsigned int character, const void *image, int width, int height, int bitmap_left, int bitmap_top, int advance_x, int advance_y)
{
	bp2d_size size;
	size.w = width;
	size.h = height;

	bp2d_position pos;
	if (bp2d_insert(atlas->bp_root, &size, &pos) == 0)
		return 0;

	atlas_htab_entry *entry = malloc(sizeof(*entry));

	entry->rect.x = pos.x;
	entry->rect.y = pos.y;
	entry->rect.w = width;
	entry->rect.h = height;
	entry->bitmap_left = bitmap_left;
	entry->bitmap_top = bitmap_top;
	entry->advance_x = advance_x;
	entry->advance_y = advance_y;

	int_htab_insert(atlas->htab, character, entry);

	void *tex_data = vita2d_texture_get_datap(atlas->tex);
	unsigned int tex_width = vita2d_texture_get_width(atlas->tex);

	int i;
	for (i = 0; i < height; i++) {
		memcpy(tex_data + (pos.x + (pos.y + i)*tex_width)*4, image + i*width*4, width*4);
	}

	return 1;
}

int texture_atlas_exists(texture_atlas *atlas, unsigned int character)
{
	return int_htab_get(atlas->htab, character) != NULL;
}

void texture_atlas_get(texture_atlas *atlas, unsigned int character, bp2d_rectangle *rect, int *bitmap_left, int *bitmap_top, int *advance_x, int *advance_y)
{
	atlas_htab_entry *entry = int_htab_get(atlas->htab, character);

	rect->x = entry->rect.x;
	rect->y = entry->rect.y;
	rect->w = entry->rect.w;
	rect->h = entry->rect.h;
	*bitmap_left = entry->bitmap_left;
	*bitmap_top = entry->bitmap_top;
	*advance_x = entry->advance_x;
	*advance_y = entry->advance_y;
}
