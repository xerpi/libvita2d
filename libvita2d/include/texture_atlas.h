#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include "vita2d.h"
#include "bin_packing_2d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ascii_table_entry {
	bp2d_rectangle rect;
	int bitmap_left;
	int bitmap_top;
	int advance_x;
	int advance_y;
	int used;
} ascii_table_entry;

typedef struct texture_atlas {
	vita2d_texture *tex;
	bp2d_node *bp_root;
	/* Crappy ASCII cache */
	ascii_table_entry ascii_table[128];
} texture_atlas;

texture_atlas *texture_atlas_create(int width, int height);
void texture_atlas_free(texture_atlas *atlas);
int texture_atlas_insert(texture_atlas *atlas, unsigned char character, const void *image, int width, int height, int bitmap_left, int bitmap_top, int advance_x, int advance_y);
int texture_atlas_exists(texture_atlas *atlas, unsigned char character);
void texture_atlas_get(texture_atlas *atlas, unsigned char character, bp2d_rectangle *rect, int *bitmap_left, int *bitmap_top, int *advance_x, int *advance_y);

#ifdef __cplusplus
}
#endif

#endif
