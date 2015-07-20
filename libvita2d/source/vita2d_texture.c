#include <psp2/kernel/sysmem.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "vita2d.h"
#include "utils.h"
#include "shared.h"

#ifdef DEBUG_BUILD
#  include <stdio.h>
#  define DEBUG(...) printf(__VA_ARGS__)
#else
#  define DEBUG(...)
#endif

static int tex_format_to_bytespp(SceGxmTextureFormat format)
{
	switch (format & 0x9f000000U) {
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
		return 1;
	case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
		return 2;
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
		return 3;
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
	default:
		return 4;
	}
}

vita2d_texture *vita2d_create_empty_texture(unsigned int w, unsigned int h)
{
	return vita2d_create_empty_texture_format(w, h, SCE_GXM_TEXTURE_FORMAT_A8B8G8R8);
}

vita2d_texture *vita2d_create_empty_texture_format(unsigned int w, unsigned int h, SceGxmTextureFormat format)
{
	vita2d_texture *texture = malloc(sizeof(*texture));
	if (!texture) {
		return NULL;
	}

	const int tex_size =  w * h * tex_format_to_bytespp(format);

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
		format,
		w,
		h,
		0);

	if((format & 0x9f000000U) == SCE_GXM_TEXTURE_BASE_FORMAT_P8){

		const int pal_size = 256*sizeof(uint32_t);

		void *texture_palette = gpu_alloc(
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			pal_size,
			SCE_GXM_PALETTE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ,
			&texture->palette_UID);

		if (!texture_palette) {
			gpu_free(texture->data_UID);
			free(texture);
			return NULL;
		}

		memset(texture_palette, 0, pal_size);

		int res = sceGxmTextureSetPalette(&texture->gxm_tex,texture_palette);
		printf("pal: %x", res);
  }

	return texture;
}

void vita2d_free_texture(vita2d_texture *texture)
{
	if (texture) {
		if(texture->palette_UID)
			gpu_free(texture->palette_UID);
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

	const float w = vita2d_texture_get_width(texture);
	const float h = vita2d_texture_get_height(texture);

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

void vita2d_draw_texture_rotate(const vita2d_texture *texture, float x, float y, float rad)
{
	vita2d_draw_texture_rotate_hotspot(texture, x, y, rad,
		vita2d_texture_get_width(texture)/2.0f,
		vita2d_texture_get_height(texture)/2.0f);
}


void vita2d_draw_texture_rotate_hotspot(const vita2d_texture *texture, float x, float y, float rad, float center_x, float center_y)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	uint16_t *indices = (uint16_t *)vita2d_pool_memalign(
		4 * sizeof(uint16_t), // 4 indices
		sizeof(uint16_t));

	const float w = vita2d_texture_get_width(texture);
	const float h = vita2d_texture_get_height(texture);

	vertices[0].x = -center_x;
	vertices[0].y = -center_y;
	vertices[0].z = +0.5f;
	vertices[0].u = 0.0f;
	vertices[0].v = 0.0f;

	vertices[1].x = w - center_x;
	vertices[1].y = -center_y;
	vertices[1].z = +0.5f;
	vertices[1].u = 1.0f;
	vertices[1].v = 0.0f;

	vertices[2].x = -center_x;
	vertices[2].y = h - center_y;
	vertices[2].z = +0.5f;
	vertices[2].u = 0.0f;
	vertices[2].v = 1.0f;

	vertices[3].x = w - center_x;
	vertices[3].y = h - center_y;
	vertices[3].z = +0.5f;
	vertices[3].u = 1.0f;
	vertices[3].v = 1.0f;

	float c = cosf(rad);
	float s = sinf(rad);
	int i;
	for (i = 0; i < 4; ++i) { // Rotate and translate
		float _x = vertices[i].x;
		float _y = vertices[i].y;
		vertices[i].x = _x*c - _y*s + x;
		vertices[i].y = _x*s + _y*c + y;
	}

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

void vita2d_draw_texture_scale(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	uint16_t *indices = (uint16_t *)vita2d_pool_memalign(
		4 * sizeof(uint16_t), // 4 indices
		sizeof(uint16_t));

	const float w = x_scale * vita2d_texture_get_width(texture);
	const float h = y_scale * vita2d_texture_get_height(texture);

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

void vita2d_draw_texture_part(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	uint16_t *indices = (uint16_t *)vita2d_pool_memalign(
		4 * sizeof(uint16_t), // 4 indices
		sizeof(uint16_t));

	const float w = vita2d_texture_get_width(texture);
	const float h = vita2d_texture_get_height(texture);

	const float u0 = tex_x/w;
	const float v0 = tex_y/h;
	const float u1 = (tex_x+tex_w)/w;
	const float v1 = (tex_y+tex_h)/h;

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = +0.5f;
	vertices[0].u = u0;
	vertices[0].v = v0;

	vertices[1].x = x + tex_w;
	vertices[1].y = y;
	vertices[1].z = +0.5f;
	vertices[1].u = u1;
	vertices[1].v = v0;

	vertices[2].x = x;
	vertices[2].y = y + tex_h;
	vertices[2].z = +0.5f;
	vertices[2].u = u0;
	vertices[2].v = v1;

	vertices[3].x = x + tex_w;
	vertices[3].y = y + tex_h;
	vertices[3].z = +0.5f;
	vertices[3].u = u1;
	vertices[3].v = v1;

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

extern SceGxmVertexProgram *lcd3xVertexProgram;
extern SceGxmFragmentProgram *lcd3xFragmentProgram;

extern const SceGxmProgramParameter *lcd3xWvpParam;
extern const SceGxmProgramParameter *lcd3xTextureSizeParam;


void vita2d_draw_texture_lcd3x(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale)
{

	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	uint16_t *indices = (uint16_t *)vita2d_pool_memalign(
		4 * sizeof(uint16_t), // 4 indices
		sizeof(uint16_t));

	float texture_size[2]={vita2d_texture_get_width(texture),vita2d_texture_get_height(texture)};

	const float w = x_scale * vita2d_texture_get_width(texture);
	const float h = y_scale * vita2d_texture_get_height(texture);

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

	sceGxmSetVertexProgram(context, lcd3xVertexProgram);
	sceGxmSetFragmentProgram(context, lcd3xFragmentProgram);

	void *vertexDefaultBuffer;
	sceGxmReserveVertexDefaultUniformBuffer(context, &vertexDefaultBuffer);
	sceGxmSetUniformDataF(vertexDefaultBuffer, lcd3xWvpParam, 0, 16, ortho_matrix);
	sceGxmSetUniformDataF(vertexDefaultBuffer, lcd3xTextureSizeParam, 0, 2, texture_size);

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(context, 0, &texture->gxm_tex);

	sceGxmSetVertexStream(context, 0, vertices);
	sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, indices, 4);
}

void vita2d_draw_texture_part_scale(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale)
{
	vita2d_texture_vertex *vertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_texture_vertex), // 4 vertices
		sizeof(vita2d_texture_vertex));

	uint16_t *indices = (uint16_t *)vita2d_pool_memalign(
		4 * sizeof(uint16_t), // 4 indices
		sizeof(uint16_t));

	const float w = vita2d_texture_get_width(texture);
	const float h = vita2d_texture_get_height(texture);

	const float u0 = tex_x/w;
	const float v0 = tex_y/h;
	const float u1 = (tex_x+tex_w)/w;
	const float v1 = (tex_y+tex_h)/h;

	tex_w *= x_scale;
	tex_h *= y_scale;

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = +0.5f;
	vertices[0].u = u0;
	vertices[0].v = v0;

	vertices[1].x = x + tex_w;
	vertices[1].y = y;
	vertices[1].z = +0.5f;
	vertices[1].u = u1;
	vertices[1].v = v0;

	vertices[2].x = x;
	vertices[2].y = y + tex_h;
	vertices[2].z = +0.5f;
	vertices[2].u = u0;
	vertices[2].v = v1;

	vertices[3].x = x + tex_w;
	vertices[3].y = y + tex_h;
	vertices[3].z = +0.5f;
	vertices[3].u = u1;
	vertices[3].v = v1;

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

void *vita2d_texture_get_palette(const vita2d_texture *texture)
{
	return sceGxmTextureGetPalette(&texture->gxm_tex);
}
