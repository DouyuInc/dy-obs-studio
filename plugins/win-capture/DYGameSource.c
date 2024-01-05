#include <inttypes.h>
#include <obs-module.h>
#include <obs-hotkey.h>
#include <util/platform.h>
#include <util/threading.h>
#include <windows.h>
#include <dxgi.h>
#include <util/sse-intrin.h>
#include <ipc-util/pipe.h>
#include "obfuscate.h"
#include "inject-library.h"
#include "graphics-hook-info.h"
#include "graphics-hook-ver.h"
#include "window-helpers.h"
#include "cursor-capture.h"
#include "app-helpers.h"
#include "nt-stuff.h"

#include "../DYFramework/./graphics/graphics.h"

#include "../../Config/DYLiveConstantDef.h"
#include "gameCover.h"

extern struct obs_source_info dyGameSingleSourceInfo;
extern struct obs_source_info dyGameCoverSourceInfo;


struct DataNode {
    void *gameData;
    struct DataNode *next;
};

struct GameListData
{
    obs_source_t *source;

    obs_data_t *settings;

    struct DataNode *dataHead;
    void *tempData;

    bool cover;
    void *coverData;

	gs_texture_t *texture;
    uint32_t cx;
    uint32_t cy;
	bool hasSuccessed;
	bool caputeSuccess;
    float accTime;
    float timeOut;
	float checkTextureTime;
	float checkTextureTimeOut;


	//发送信号标识
    bool sentStateEnable;
	bool hasSentSiganal;

	//线程锁
	pthread_mutex_t mutex;

};


static void gameUpdate(void *data, obs_data_t *settings);

const char *gameGetName(void *unused)
{
    UNUSED_PARAMETER(unused);
    return "游戏";
}

static void stateRepoertSwitch(void *data, calldata_t *param){
    struct GameListData *s = data;
	pthread_mutex_lock(&s->mutex);
    s->sentStateEnable = calldata_bool(param,c_keyEnable);
	pthread_mutex_unlock(&s->mutex);
} 

static void getState(void *data, calldata_t *param)
{
	struct GameListData *s = data;
	calldata_set_bool(param, c_keyResult, s->caputeSuccess);
}

extern HWND getGameSingleHwnd(void *data);
extern HWND getGameHookHwnd(void *data);

static void onSendState(void *data)
{
	struct GameListData *gameData = data;
	signal_handler_t *signal = obs_source_get_signal_handler(gameData->source);
	calldata_t *param = calldata_create();
	HWND hwnd = NULL;
	if (gameData && gameData->dataHead && gameData->dataHead->gameData)
	{
		hwnd = getGameSingleHwnd(gameData->dataHead->gameData);
	}
	calldata_set_int(param, c_keyHWnd, hwnd);
	calldata_set_bool(param, c_keyAntiCheat, obs_data_get_bool(gameData->settings, c_keyAntiCheat));
	calldata_set_string(param, c_keyGameName, obs_data_get_string(gameData->settings, c_keyGameName));
	calldata_set_string(param, c_keyProcessName, obs_data_get_string(gameData->settings, c_keyProcessName));
	calldata_set_bool(param, c_keyShareMem, obs_data_get_bool(gameData->settings, c_keyShareMem));
	signal_handler_signal(signal, c_funcGameHookFailed, param);
	calldata_destroy(param);
	param = NULL;
}

void getGameHwnd(void *data, calldata_t *param)
{
    struct GameListData *s = data;
    HWND hWnd = getGameHookHwnd(s->dataHead->gameData);
    calldata_set_int(param, c_keyHWnd, (int)hWnd);
}

static void initSignalHandler(void *data){
    struct GameListData *s = data;
    //call
    signal_handler_t *singalHandler = obs_source_get_signal_handler(s->source);
    signal_handler_add(singalHandler, "void GameHookFailed()");
    //called
    proc_handler_t *procHandler = obs_source_get_proc_handler(s->source);
    proc_handler_add(procHandler, "void ErrorReport()", stateRepoertSwitch, s);
	proc_handler_add(procHandler, "void HasGameCaptured()", getState, s);
    proc_handler_add(procHandler, "void GetGameHwnd()", getGameHwnd, s);
}

static void *gameCreate(obs_data_t *settings, obs_source_t *source)
{
    struct GameListData *gameData = bzalloc(sizeof(struct GameListData));
    //
    gameData->source = source;
	pthread_mutexattr_t attr;
	gameData->caputeSuccess = true;
	gameData->hasSuccessed = false;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&gameData->mutex, &attr);
	initSignalHandler(gameData);
	gameData->sentStateEnable = false;
    gameData->settings = settings;
    //default size
    gameData->cx = 1920;
    gameData->cy = 1080;

    //gameData->dataHead = bzalloc(sizeof(struct DataNode));
    //gameData->dataHead->next = NULL;

    //gameData->dataHead->gameData = dyGameSingleSourceInfo.create(settings, gameData->source);
    //gameData->tempData = gameData->dataHead->gameData;
    gameData->coverData = dyGameCoverSourceInfo.create(settings, gameData->source);
    
    
    
    gameUpdate(gameData, settings);
    return gameData;
}

void gameDestroy(void *data)
{
    struct GameListData *gameData = data;
    if (NULL == gameData) {
        return;
    }
	if (gameData->hasSuccessed && !gameData->caputeSuccess)
	{
		onSendState(gameData);
	}
	gameData->texture = NULL;
	if (gameData->mutex)
	{
		pthread_mutex_destroy(&gameData->mutex);
	}

    struct DataNode *p = gameData->dataHead;
    while (p) {
        struct DataNode *next =  p->next;
        dyGameSingleSourceInfo.destroy(p->gameData);
        bfree(p);
        p = next;
    }
    dyGameCoverSourceInfo.destroy(gameData->coverData);
	
    bfree(gameData);
}

static uint32_t gameGetWidth(void *data)
{
    struct GameListData *gameData = data;
    return gameData->cx;
}

static uint32_t gameGetHeight(void *data)
{
    struct GameListData *gameData = data;
    return gameData->cy;
}

static void gameDefaults(obs_data_t *defaults)
{
    //
    const char *mode = obs_data_get_string(defaults, "capture_mode");
    dyGameSingleSourceInfo.get_defaults(defaults);
    mode = obs_data_get_string(defaults, "capture_mode");
    dyGameCoverSourceInfo.get_defaults(defaults);
    mode = obs_data_get_string(defaults, "capture_mode");
    //info count
    obs_data_set_default_int(defaults, "info_cnt", 2);

    //info0;
 /*   obs_data_array_t *info = obs_data_array_create();
    obs_data_t *info0 = obs_data_create();
    obs_data_set_string(info0, "class_name", "MortarGame");
    obs_data_set_string(info0, "title_name", "Fruit Ninja");
    obs_data_array_push_back(info, info0);
    obs_data_t *info1 = obs_data_create();
    obs_data_set_string(info1, "class_name", "Notepad++");
    obs_data_set_string(info1, "title_name", "Notepad++");
    obs_data_array_push_back(info, info1);
    obs_data_set_array(defaults, "info", info);
    obs_data_array_release(info);*/

    //obs_data_release(info0);
    //game name
    obs_data_set_default_string(defaults, "game_name", "");
    //process name
    obs_data_set_default_string(defaults, "process_name", "");
    //anti cheat
    obs_data_set_default_bool(defaults, "anti_cheat", true);
    //cursor
    obs_data_set_default_bool(defaults, "capture_cursor", false);

    //private
    //obs_data_t *privateData = obs_data_create();
    //obs_data_set_default_obj(defaults, "private", privateData);
    //obs_data_release(privateData);
    //mode = obs_data_get_string(defaults, "capture_mode");
}

static obs_properties_t *gameProperties(void *data)
{
    struct GameListData *gameData = data;

    obs_properties_t *ppts = dyGameSingleSourceInfo.get_properties(gameData->dataHead->gameData);

    obs_properties_add_text(ppts, "title_name", "title_name", OBS_TEXT_DEFAULT);
    obs_properties_add_text(ppts, "class_name", "class_name", OBS_TEXT_DEFAULT);
    obs_properties_add_text(ppts, "game_name", "game_name", OBS_TEXT_DEFAULT);
    obs_properties_add_text(ppts, "process_name", "process_name", OBS_TEXT_DEFAULT);
    return ppts;
}

static void gameUpdateOnTick(void *data, obs_data_t *settings) {
	struct GameListData *gameData = data;
	dyGameCoverSourceInfo.update(gameData->coverData, settings);
}

static void gameUpdate(void *data, obs_data_t *settings)
{
    struct GameListData *gameData = data;
    // info:class name list and title name list

    const char *mode = obs_data_get_string(settings, "capture_mode");
    const char *gameNmae = obs_data_get_string(settings, c_keyGameName);
    if (strlen(gameNmae) <= 1) {
        gameNmae = obs_data_get_string(settings, c_keyProcessName);
        if (strlen(gameNmae) <= 1) {
            gameNmae = obs_data_get_string(settings, c_keyTitleName);
        }
        if (strlen(gameNmae) <= 1) {
            gameNmae = obs_data_get_string(settings, c_keyClassName);
        }
        if (strlen(gameNmae) <= 1) {
            gameNmae = "请打开游戏";
        }
        obs_data_set_string(settings, "game_name", gameNmae);
    }

    //obs test
    //info count
    obs_data_array_t *info = obs_data_get_array(settings, c_keyInfo);
    int infoCount = (int)obs_data_array_count(info);
    if (infoCount > 3)
    {
        infoCount = 3;
    }
    // release data
    struct DataNode *p = gameData->dataHead;
    struct DataNode *pre = gameData->dataHead;
    for (int i = 0; i < infoCount; i++) {
        obs_data_t *infox = obs_data_array_item(info, (size_t)i);
        const char *titleName = obs_data_get_string(infox, c_keyTitleName);
        const char *className = obs_data_get_string(infox, c_keyClassName);
        //set subSetting
        obs_data_set_string(settings, "title_name", titleName);
        obs_data_set_string(settings, "class_name", className);
        //const char *titleName1 = obs_data_get_string(settings, c_keyTitleName);
        //const char *className2 = obs_data_get_string(settings, c_keyClassName);
        if (!p) {
            p = bzalloc(sizeof(struct DataNode));
            p->gameData = dyGameSingleSourceInfo.create(settings, gameData->source);
            p->next = NULL;
			if (!gameData->dataHead) {
				gameData->dataHead = p;
				pre = gameData->dataHead;
			}
			else {
				pre->next = p;
				pre = p;
			}
		}
        dyGameSingleSourceInfo.update(p->gameData, settings);
        p = p->next;
    }

    while (p && p->next) {
		pre->next = p->next;
		if (p->gameData) {
			dyGameSingleSourceInfo.destroy(p->gameData);
			p->gameData = NULL;
			p->next = NULL;
		}
		p = NULL;
		bfree(p);
		p = pre->next;
    }

    gameData->cover = true;
    dyGameCoverSourceInfo.update(gameData->coverData, settings);

    gameData->timeOut = 1.0f;
    gameData->accTime = 0.0f;	
	gameData->checkTextureTime = 0.0f;
	gameData->checkTextureTimeOut = 5.0f;
	gameData->cx = obs_data_get_int(settings,"cx");
	gameData->cy = obs_data_get_int(settings, "cy");
}

static void sendReport(void *data, float seconds)
{
	struct GameListData *gameData = data;
	gameData->accTime += seconds;
	if (gameData->accTime > gameData->timeOut)
	{
		HWND hwnd = NULL;
		if (gameData && gameData->dataHead && gameData->dataHead->gameData)
		{
			hwnd = getGameSingleHwnd(gameData->dataHead->gameData);
		}
		if (hwnd)
		{
			onSendState(gameData);
			blog(LOG_INFO, "send game capture filed signal,last time=%f,frame seconds=%f", gameData->accTime, seconds);
		}
		gameData->accTime = 0.0f;
	}
}

extern bool hasSingleGameSuccessed(void *data);

static void gameTick(void *data, float seconds)
{
	struct GameListData *gameData = data;
	struct DataNode *p = gameData->dataHead;
	while (p)
	{
		gameData->cover = true;
		dyGameSingleSourceInfo.video_tick(p->gameData, seconds);
		uint32_t cx = dyGameSingleSourceInfo.get_width(p->gameData);
		uint32_t cy = dyGameSingleSourceInfo.get_height(p->gameData);
		bool hasSuccessed = hasSingleGameSuccessed(p->gameData);

		if (cx > 0 && cy > 0 && hasSuccessed)
		{
			gameData->hasSuccessed = true;
			obs_data_set_int(gameData->settings, "cx", cx);
			obs_data_set_int(gameData->settings, "cy", cy);
			gameData->cx = cx;
			gameData->cy = cy;
			gameUpdateOnTick(gameData, gameData->settings);
			gameData->cover = false;
			gameData->tempData = p->gameData;
			break;
		}
		p = p->next;
	}
	if (!obs_source_showing(gameData->source))
	{
		return;
	}
	dyGameCoverSourceInfo.video_tick(gameData->coverData, seconds);

	if (gameData->hasSuccessed)
	{
		gameData->checkTextureTime = 0.0f;
		gameData->accTime = 0.0f;
	}
	else
	{
		if (gameData->sentStateEnable)
		{
			if (gameData->checkTextureTime > gameData->checkTextureTimeOut)
			{
				sendReport(gameData, seconds);
			}
			else
			{
				gameData->checkTextureTime += seconds;
			}
		}
	}
}

static void gameRender(void *data, gs_effect_t *effect)
{
    struct GameListData *gameData = data;
    if (gameData->cover) {
        dyGameCoverSourceInfo.video_render(gameData->coverData, effect);
    }
    else {
		dyGameSingleSourceInfo.video_render(gameData->tempData, effect);
	}
}


struct obs_source_info dyGameSourceInfo = {
    .id = "DY_Game",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
    OBS_SOURCE_DO_NOT_DUPLICATE,
    .get_name = gameGetName,
    .create = gameCreate,
    .destroy = gameDestroy,
    .get_width = gameGetWidth,
    .get_height = gameGetHeight,
    .get_defaults = gameDefaults,
    .get_properties = gameProperties,
    .update = gameUpdate,
    .video_tick = gameTick,
    .video_render = gameRender,
    .icon_type = OBS_ICON_TYPE_GAME_CAPTURE,
};
