#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include "font.h"
#include "splash.cpp"

int main()
{
	char vita_ip[16];
	unsigned short int vita_port = 0;

	vita2d_init();
	vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));

	vita2d_texture* texture_splash = vita2d_load_JPEG_buffer(splash, size_splash);

	vita2d_start_drawing();
	vita2d_draw_texture(texture_splash, 0, 0);
	vita2d_end_drawing();
	vita2d_swap_buffers();

	sceKernelDelayThread(4000000);

	vita2d_start_drawing();
	vita2d_clear_screen();
	vita2d_end_drawing();
	vita2d_swap_buffers();

	vita2d_free_texture(texture_splash);

	SceCtrlData pad;
	SceCtrlData oldpad;

	while (1) {
		vita2d_start_drawing();
		vita2d_clear_screen();

		font_draw_string(10, 10, RGBA8(255,255,255,255), "This is a sample");

		vita2d_end_drawing();
		vita2d_swap_buffers();
	}

	vita2d_fini();
	return 0;
}
