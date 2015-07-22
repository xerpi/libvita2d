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
	memset(atlas->ascii_table, 0, sizeof(atlas->ascii_table));

	return atlas;
}

void texture_atlas_free(texture_atlas *atlas)
{
	vita2d_free_texture(atlas->tex);
	bp2d_free(atlas->bp_root);
	free(atlas);
}

int texture_atlas_insert(texture_atlas *atlas, unsigned char character, const void *image, int width, int height, int bitmap_left, int bitmap_top, int advance_x, int advance_y)
{
	bp2d_size size;
	size.w = width;
	size.h = height;

	bp2d_position pos;
	if (bp2d_insert(atlas->bp_root, &size, &pos) == 0)
		return 0;

	atlas->ascii_table[character].rect.x = pos.x;
	atlas->ascii_table[character].rect.y = pos.y;
	atlas->ascii_table[character].rect.w = width;
	atlas->ascii_table[character].rect.h = height;
	atlas->ascii_table[character].bitmap_left = bitmap_left;
	atlas->ascii_table[character].bitmap_top = bitmap_top;
	atlas->ascii_table[character].advance_x = advance_x;
	atlas->ascii_table[character].advance_y = advance_y;
	atlas->ascii_table[character].used = 1;

	void *tex_data = vita2d_texture_get_datap(atlas->tex);
	unsigned int tex_width = vita2d_texture_get_width(atlas->tex);

	int i;
	for (i = 0; i < height; i++) {
		memcpy(tex_data + (pos.x + (pos.y + i)*tex_width)*4, image + i*width*4, width*4);
	}

	return 1;
}

int texture_atlas_exists(texture_atlas *atlas, unsigned char character)
{
	return atlas->ascii_table[character].used;
}

void texture_atlas_get(texture_atlas *atlas, unsigned char character, bp2d_rectangle *rect, int *bitmap_left, int *bitmap_top, int *advance_x, int *advance_y)
{
	rect->x = atlas->ascii_table[character].rect.x;
	rect->y = atlas->ascii_table[character].rect.y;
	rect->w = atlas->ascii_table[character].rect.w;
	rect->h = atlas->ascii_table[character].rect.h;
	*bitmap_left = atlas->ascii_table[character].bitmap_left;
	*bitmap_top = atlas->ascii_table[character].bitmap_top;
	*advance_x = atlas->ascii_table[character].advance_x;
	*advance_y = atlas->ascii_table[character].advance_y;
}
