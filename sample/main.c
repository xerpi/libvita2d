/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>

#include <vita2d.h>

int main()
{
	vita2d_init();
	vita2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));

	vita2d_texture *tex = vita2d_create_empty_texture(128, 128);
	unsigned int *tex_data = vita2d_texture_get_datap(tex);

	SceCtrlData pad;
	memset(&pad, 0, sizeof(pad));

	float rad = 0.0f;

	while (1) {
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if (pad.buttons & SCE_CTRL_START) break;

		vita2d_start_drawing();
		vita2d_clear_screen();

		vita2d_draw_rectangle(20, 20, 400, 250, RGBA8(255, 0, 0, 255));
		vita2d_draw_rectangle(700, 300, 100, 150, RGBA8(0, 0, 255, 255));
		vita2d_draw_fill_circle(200, 420, 100, RGBA8(0, 255,0 ,255));

		/* Fill the texture with random data */
		int i, j;
		for (i = 0; i < 128; i++) {
			for (j = 0; j < 128; j++) {
				tex_data[j + i*128] = rand();
			}
		}

		vita2d_draw_texture_rotate(tex, 940/2, 544/2, rad);

		vita2d_draw_line(40, 40, 300, 300, RGBA8(255, 0, 255, 255));

		vita2d_end_drawing();
		vita2d_swap_buffers();

		rad += 0.1f;
	}

	vita2d_fini();
	vita2d_free_texture(tex);

	sceKernelExitProcess(0);
	return 0;
}
