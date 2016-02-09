/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>

#include <vita2d.h>

#include <stdarg.h>

#include <psp2/net/net.h>
#include <psp2/net/netctl.h>

#define NET_INIT_SIZE 1 * 1024 * 1024
#define NETDBG_DEFAULT_PORT 9023

static int netdbg_sock = -1;
static void *net_memory = NULL;
static int net_init = -1;

int netdbg_init() {
	int ret = 0;
	SceNetSockaddrIn server;
	SceNetInitParam initparam;
	SceUShort16 port = NETDBG_DEFAULT_PORT;

	/* Init Net */
	ret = sceNetShowNetstat();
	if (ret == SCE_NET_ERROR_ENOTINIT) {
		net_memory = malloc(NET_INIT_SIZE);

		initparam.memory = net_memory;
		initparam.size = NET_INIT_SIZE;
		initparam.flags = 0;

		ret = net_init = sceNetInit(&initparam);
		if (net_init < 0)
			goto error_netinit;
	} else if (ret != 0) {
		goto error_netstat;
	}

	server.sin_len = sizeof(server);
	server.sin_family = SCE_NET_AF_INET;
	sceNetInetPton(SCE_NET_AF_INET, "192.168.1.40", &server.sin_addr);
	server.sin_port = sceNetHtons(port);
	memset(server.sin_zero, 0, sizeof(server.sin_zero));

	ret = netdbg_sock = sceNetSocket("NetDbg", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (netdbg_sock < 0)
		goto error_netsock;

	ret = sceNetConnect(netdbg_sock, (SceNetSockaddr *)&server, sizeof(server));
	if (ret < 0)
		goto error_netconnect;

	return 0;

error_netconnect:
	sceNetSocketClose(netdbg_sock);
	netdbg_sock = -1;
error_netsock:
	if (net_init == 0) {
		sceNetTerm();
		net_init = -1;
	}
error_netinit:
	if (net_memory) {
		free(net_memory);
		net_memory = NULL;
	}
error_netstat:
	return ret;
}

void netdbg_fini() {
	if (netdbg_sock > 0) {
		sceNetSocketClose(netdbg_sock);
		if (net_init == 0)
			sceNetTerm();
		if (net_memory)
			free(net_memory);
		netdbg_sock = -1;
		net_init = -1;
		net_memory = NULL;
	}
}

int netdbg(const char *text, ...) {
	va_list list;
	char string[512];
	if (netdbg_sock > 0) {
		va_start(list, text);
		vsprintf(string, text, list);
		va_end(list);
		return sceNetSend(netdbg_sock, string, strlen(string), 0);
	}
	return -1;
}


extern const vita2d_texture *vita2d_pgf_get_texture(const vita2d_pgf *font);

int main()
{
	netdbg_init();
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_PGF) != SCE_SYSMODULE_LOADED)
		sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);

	vita2d_init();
	vita2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));

	vita2d_pgf *pgf = vita2d_load_default_pgf();

	SceCtrlData pad;
	memset(&pad, 0, sizeof(pad));

	int x = 10;
	int y = 100;
	unsigned int color = RGBA8(0, 255, 0, 255);

	while (1) {
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if (pad.buttons & SCE_CTRL_START) break;

		vita2d_start_drawing();
		vita2d_clear_screen();

		int i;
		for (i = 0; i < 256; i++) {
			char c[2];
			c[0] = i;
			c[1] = 0;
			vita2d_pgf_draw_text(pgf, 700, 30, RGBA8(0,255,0,255), 1.0f, c);
		}

		const vita2d_texture *pgf_tex = vita2d_pgf_get_texture(pgf);
		int w = vita2d_texture_get_width(pgf_tex);
		int h = vita2d_texture_get_height(pgf_tex);
		vita2d_draw_line(x, y, x + w, y, color);
		vita2d_draw_line(x + w, y, x + w, y + h, color);
		vita2d_draw_line(x, y + h, x + w, y + h, color);
		vita2d_draw_line(x, y, x, y + h, color);

		vita2d_draw_texture(pgf_tex, x, y);


		vita2d_end_drawing();
		vita2d_swap_buffers();
	}

	vita2d_fini();
	vita2d_free_pgf(pgf);
	netdbg_fini();
	sceKernelExitProcess(0);
	return 0;
}
