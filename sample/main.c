/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>

#include <vita2d.h>

/*
 * Symbol of the image.png file
 */
extern unsigned char _binary_image_png_start;

int main()
{
	SceCtrlData pad;
	vita2d_pgf *pgf;
	vita2d_pvf *pvf;
	vita2d_texture *image;
	float rad = 0.0f;

	vita2d_init();
	vita2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));

	pgf = vita2d_load_default_pgf();
	pvf = vita2d_load_default_pvf();

	/*
	 * Load the statically compiled image.png file.
	 */
	image = vita2d_load_PNG_buffer(&_binary_image_png_start);

	memset(&pad, 0, sizeof(pad));

	while (1) {
		sceCtrlPeekBufferPositive(0, &pad, 1);

		if (pad.buttons & SCE_CTRL_START)
			break;

		vita2d_start_drawing();
		vita2d_clear_screen();

		vita2d_draw_rectangle(20, 20, 400, 250, RGBA8(255, 0, 0, 255));
		vita2d_draw_rectangle(680, 350, 100, 150, RGBA8(0, 0, 255, 255));
		vita2d_draw_fill_circle(200, 420, 100, RGBA8(0, 255,0 ,255));

		vita2d_draw_texture_rotate(image, 940/2, 544/2, rad);

		vita2d_draw_line(500, 30, 800, 300, RGBA8(255, 0, 255, 255));

		vita2d_pgf_draw_text(pgf, 700, 30, RGBA8(0,255,0,255), 1.0f, "PGF Font sample!");

		vita2d_pvf_draw_text(pvf, 700, 80, RGBA8(0,255,0,255), 1.0f, "PVF Font sample!");

		size_t n_vertices = 69;
		vita2d_color_vertex *vertices = (vita2d_color_vertex *)vita2d_pool_memalign(
			n_vertices * sizeof(vita2d_color_vertex),
			sizeof(vita2d_color_vertex));

		for (int i=0; i<n_vertices; ++i) {
			vertices[i].x = 830.f + 100.f * fabsf(sinf(i*10.f+rad)) * sinf(i*0.1f+rad*0.4f);
			vertices[i].y = 420.f + 100.f * fabsf(sinf(i*10.f+rad)) * cosf(i*0.1f+rad*0.4f);
			vertices[i].z = 0.5f;
			vertices[i].color = RGBA8(0xff-i*2, i*3, 0x8a-2*i, 0x80);
		}

		vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, vertices, n_vertices);

		size_t nslices = 50;
		size_t n_tvertices = 6 * nslices;
		vita2d_texture_vertex *tvertices = (vita2d_texture_vertex *)vita2d_pool_memalign(
			n_tvertices * sizeof(vita2d_texture_vertex),
			sizeof(vita2d_texture_vertex));

		for (int slice=0; slice<nslices; slice++) {
			float a = (float)slice/(float)nslices;
			float b = (float)(slice+1)/(float)nslices;

			vita2d_texture_vertex *v = &tvertices[slice*6];
			(v++)->u = a; (v++)->u = a; (v++)->u = b;
			(v++)->u = a; (v++)->u = b; (v++)->u = b;
		}

		for (int i=0; i<n_tvertices; ++i) {
			tvertices[i].v = i % 2;
			tvertices[i].x = 720.f + 200.f * tvertices[i].u;
			tvertices[i].y = 100.f + 200.f * tvertices[i].v + 10.f * sinf(tvertices[i].u*(3.f+40.f*fabsf(sinf(rad*0.1f)))+rad);
			tvertices[i].z = 0.5f;
		}

		vita2d_draw_array_textured(image, SCE_GXM_PRIMITIVE_TRIANGLES, tvertices, n_tvertices, RGBA8(0xFF, 0xFF, 0xFF, 0xFF));

		vita2d_draw_rectangle(40, 40, 100, 100, RGBA8(128, 64, 192, 255));
		vita2d_set_blend_mode_add(1);
		vita2d_draw_rectangle(40, 60, 200, 60, RGBA8(0, 100, 0, 128));
		vita2d_set_blend_mode_add(0);

		vita2d_end_drawing();
		vita2d_swap_buffers();

		rad += 0.1f;
	}

	/*
	 * vita2d_fini() waits until the GPU has finished rendering,
	 * then we can free the assets freely.
	 */
	vita2d_fini();
	vita2d_free_texture(image);
	vita2d_free_pgf(pgf);
	vita2d_free_pvf(pvf);

	sceKernelExitProcess(0);
	return 0;
}
