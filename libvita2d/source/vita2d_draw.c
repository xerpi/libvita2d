#include "vita2d.h"

/* Shared with other .c */
extern float ortho_matrix[4*4];
extern SceGxmContext *context;
extern SceGxmVertexProgram *colorVertexProgram;
extern SceGxmFragmentProgram *colorFragmentProgram;
extern SceGxmVertexProgram *textureVertexProgram;
extern SceGxmFragmentProgram *textureFragmentProgram;
extern const SceGxmProgramParameter *colorWvpParam;
extern const SceGxmProgramParameter *textureWvpParam;

void vita2d_draw_rectangle(float x, float y, float w, float h, unsigned int color)
{
	vita2d_color_vertex *vertices = (vita2d_color_vertex *)vita2d_pool_memalign(
		4 * sizeof(vita2d_color_vertex), // 4 vertices
		sizeof(vita2d_color_vertex));

	uint16_t *indices = (uint16_t *)vita2d_pool_memalign(
		4 * sizeof(uint16_t), // 4 indices
		sizeof(uint16_t));

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = +0.5f;
	vertices[0].color = color;

	vertices[1].x = x + w;
	vertices[1].y = y;
	vertices[1].z = +0.5f;
	vertices[1].color = color;

	vertices[2].x = x;
	vertices[2].y = y + h;
	vertices[2].z = +0.5f;
	vertices[2].color = color;

	vertices[3].x = x + w;
	vertices[3].y = y + h;
	vertices[3].z = +0.5f;
	vertices[3].color = color;

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 3;

	sceGxmSetVertexProgram(context, colorVertexProgram);
	sceGxmSetFragmentProgram(context, colorFragmentProgram);

	void *vertexDefaultBuffer;
	sceGxmReserveVertexDefaultUniformBuffer(context, &vertexDefaultBuffer);
	sceGxmSetUniformDataF(vertexDefaultBuffer, colorWvpParam, 0, 16, ortho_matrix);

	sceGxmSetVertexStream(context, 0, vertices);
	sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, indices, 4);

}
