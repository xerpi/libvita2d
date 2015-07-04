#include <psp2/kernel/memorymgr.h>
#include <string.h>
#include <stdlib.h>
#include "vita2d.h"
#include "utils.h"

/* Shared with other .c */
extern float ortho_matrix[4*4];
extern SceGxmContext *context;
extern SceGxmVertexProgram *colorVertexProgram;
extern SceGxmFragmentProgram *colorFragmentProgram;
extern SceGxmVertexProgram *textureVertexProgram;
extern SceGxmFragmentProgram *textureFragmentProgram;
extern const SceGxmProgramParameter *colorWvpParam;
extern const SceGxmProgramParameter *textureWvpParam;


vita2d_texture *vita2d_create_empty_texture(unsigned int w, unsigned int h)
{
	vita2d_texture *texture = malloc(sizeof(*texture));
	if (!texture) {
		return NULL;
	}

	const int tex_size =  w * h * 4;

	/* Allocate a GPU buffer for the texture */
	void *texture_data = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
		tex_size,
		SCE_GXM_TEXTURE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&texture->data_UID);

	if (!texture_data) {
		free(texture);
		return NULL;
	}

	/* Clear the texture */
	memset(texture_data, 0, tex_size);

	/* Create the gxm texture */
	sceGxmTextureInitLinear(
		&texture->gxm_tex,
		texture_data,
		SCE_GXM_TEXTURE_FORMAT_A8B8G8R8,
		w,
		h,
		0);

	return texture;
}

void vita2d_free_texture(vita2d_texture *texture)
{
	if (texture) {
		gpu_free(texture->data_UID);
		free(texture);
	}
}

unsigned int vita2d_texture_get_width(const vita2d_texture *texture)
{
	return sceGxmTextureGetWidth(&texture->gxm_tex);
}

unsigned int vita2d_texture_get_height(const vita2d_texture *texture)
{
	return sceGxmTextureGetHeight(&texture->gxm_tex);
}

void *vita2d_texture_get_datap(const vita2d_texture *texture)
{
	return sceGxmTextureGetData(&texture->gxm_tex);
}

void vita2d_draw_texture(const vita2d_texture *texture, float x, float y)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	uint16_t *indices = (uint16_t *)vita2d_pool_memalign(
		4 * sizeof(uint16_t), // 4 indices
		sizeof(uint16_t));

	const unsigned int w = sceGxmTextureGetWidth(&texture->gxm_tex);
	const unsigned int h = sceGxmTextureGetHeight(&texture->gxm_tex);

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = +0.5f;
	vertices[0].u = 0.0f;
	vertices[0].v = 0.0f;

	vertices[1].x = x + w;
	vertices[1].y = y;
	vertices[1].z = +0.5f;
	vertices[1].u = 1.0f;
	vertices[1].v = 0.0f;

	vertices[2].x = x;
	vertices[2].y = y + h;
	vertices[2].z = +0.5f;
	vertices[2].u = 0.0f;
	vertices[2].v = 1.0f;

	vertices[3].x = x + w;
	vertices[3].y = y + h;
	vertices[3].z = +0.5f;
	vertices[3].u = 1.0f;
	vertices[3].v = 1.0f;

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 3;

	sceGxmSetVertexProgram(context, textureVertexProgram);
	sceGxmSetFragmentProgram(context, textureFragmentProgram);

	void *vertexDefaultBuffer;
	sceGxmReserveVertexDefaultUniformBuffer(context, &vertexDefaultBuffer);
	sceGxmSetUniformDataF(vertexDefaultBuffer, textureWvpParam, 0, 16, ortho_matrix);

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(context, 0, vertices);
	sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, indices, 4);
}
