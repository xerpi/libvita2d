#include <stdlib.h>
#include <string.h>

#include <psp2/apputil.h>
#include "vita2d.h"

#ifdef DEBUG_BUILD
#  include <stdio.h>
#  define DEBUG(...) printf(__VA_ARGS__)
#else
#  define DEBUG(...)
#endif

void render_dialog();

int vita2d_dialog_init()
{
	SceAppUtilInitParam		initParam;
	SceAppUtilBootParam		bootParam;

	int		err;

	memset(&initParam, 0 ,sizeof(SceAppUtilInitParam));
	memset(&bootParam, 0 ,sizeof(SceAppUtilBootParam));
	err = sceAppUtilInit(&initParam, &bootParam);
	DEBUG("sceAppUtilInit(): 0x%08X\n", err);

	return 0;
}

int vita2d_dialog_shutdown()
{
	int		err;

	err = sceAppUtilShutdown();
	DEBUG("sceAppUtilShutdown(): 0x%08X\n", err);

	return 0;
}

int dialog_check()
{
	SceCommonDialogStatus	msgStatus;
	SceMsgDialogResult		msgResult;

	int		err;

	msgStatus = sceMsgDialogGetStatus();

	switch (msgStatus) {
	default:
	case SCE_COMMON_DIALOG_STATUS_NONE:
	case SCE_COMMON_DIALOG_STATUS_RUNNING:
		return msgStatus;
	case SCE_COMMON_DIALOG_STATUS_FINISHED:
		break;
	}

	memset(&msgResult, 0, sizeof(SceMsgDialogResult));
	err = sceMsgDialogGetResult(&msgResult);
	DEBUG("sceMsgDialogGetResult(): 0x%08X\n", err);

	if (msgResult.result < 0) {
		DEBUG("sceMsgDialogResult.result: 0x%08X\n", err);
	}

	err = sceMsgDialogTerm();
	DEBUG("sceMsgDialogTerm(): 0x%08X\n", err);

	return 0;
}

int vita2d_dialog_draw(int type, const char *str)
{
	SceMsgDialogParam				msgParam;
	SceMsgDialogUserMessageParam	userMsgParam;

	int		err;

	sceMsgDialogParamInit(&msgParam);
	msgParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;

	memset(&userMsgParam, 0, sizeof(SceMsgDialogUserMessageParam));
	msgParam.userMsgParam = &userMsgParam;
	msgParam.userMsgParam->msg = str;
	msgParam.userMsgParam->buttonType = type;
	msgParam.commonParam.infobarParam = NULL;
	msgParam.commonParam.dimmerColor = NULL;
	msgParam.commonParam.bgColor = NULL;

	err = sceMsgDialogInit(&msgParam);
	DEBUG("sceMsgDialogInit(): 0x%08X\n", err);

	while (1) {
		vita2d_start_drawing();
		vita2d_clear_screen();
		vita2d_end_drawing();
		render_dialog();
		vita2d_swap_buffers();
		dialog_check();
	}

	return 0;
}
