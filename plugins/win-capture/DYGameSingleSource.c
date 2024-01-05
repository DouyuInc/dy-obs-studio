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

#include "DYD3D9HookInfo.h"
#include "../../Config/DYLiveConstantDef.h"

extern struct obs_source_info dyHookSourceInfo;
extern struct obs_source_info dyWindowSourceInfo;

enum TextureReady { none, win, hook };

struct GameData {
	obs_source_t *source;

	void *hookData;
	void *windowData;

	enum TextureReady textureReady;
	bool antiCheat;

	gs_texture_t *texture;
	uint32_t cx;
	uint32_t cy;
	bool hasSuccessed;
	struct obs_source_info hookInfo;
};

static void gameSingleUpdate(void *data, obs_data_t *settings);

const char *gameSingleGetName(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "游戏";
}

static void *gameSingleCreate(obs_data_t *settings, obs_source_t *source)
{
	struct GameData *gameData = bzalloc(sizeof(struct GameData));
	//
	gameData->source = source;
	//
	//gameData->hookData = dyHookSourceInfo.create(settings, source);
	//const char* className = obs_data_get_string(settings, c_keyClassName);
	//gameData->hookInfo = dyHookSourceInfo;
	//gameData->hookData = gameData->hookInfo.create(settings, source);
	gameData->windowData = dyWindowSourceInfo.create(settings, source);
	//default size
	gameData->cx = 0;
	gameData->cy = 0;

	gameSingleUpdate(gameData, settings);
	return gameData;
}

void gameSingleDestroy(void *data)
{
	struct GameData *gameData = data;
	if (NULL == gameData) {
		return;
	}
	gameData->texture = NULL;
	//dyHookSourceInfo.destroy(gameData->hookData);
	gameData->hookInfo.destroy(gameData->hookData);
	dyWindowSourceInfo.destroy(gameData->windowData);

	bfree(gameData);
}

static uint32_t gameSingleGetWidth(void *data)
{
	struct GameData *gameData = data;
	return gameData->cx;
}

static uint32_t gameSingleGetHeight(void *data)
{
	struct GameData *gameData = data;
	return gameData->cy;
}

static void gameSingleDefaults(obs_data_t *defaults)
{

	dyHookSourceInfo.get_defaults(defaults);
	const char*mode = obs_data_get_string(defaults, "capture_mode");
	dyWindowSourceInfo.get_defaults(defaults);
	obs_data_set_default_bool(defaults, c_keyAntiCheat, true);
	//obs_data_set_bool(defaults, c_keyAntiCheat, false);

}

static obs_properties_t *gameSingleProperties(void *data)
{
	struct GameData *gameData = data;

	obs_properties_t *ppts =
		//dyHookSourceInfo.get_properties(gameData->hookData);
		gameData->hookInfo.get_properties(gameData->hookData);
	obs_properties_add_text(ppts, "title_name", "title_name",
				OBS_TEXT_DEFAULT);
	obs_properties_add_text(ppts, "class_name", "class_name",
				OBS_TEXT_DEFAULT);
	obs_properties_add_text(ppts, "game_name", "game_name",
				OBS_TEXT_DEFAULT);
	obs_properties_add_text(ppts, "process_name", "process_name",
				OBS_TEXT_DEFAULT);
	return ppts;
}


static void gameSingleUpdate(void *data, obs_data_t *settings)
{
	struct GameData *gameData = data;
	//dyHookSourceInfo.update(gameData->hookData, settings);
	bool antiCheat = obs_data_get_bool(settings, c_keyAntiCheat);
    if (gameData->antiCheat!= antiCheat || NULL == gameData->hookData)
    {
        if (NULL != gameData->hookData && NULL != gameData->hookInfo.destroy)
        {
            gameData->hookInfo.destroy(gameData->hookData);
            gameData->hookData = NULL;
        }
        if (antiCheat)
        {
            gameData->hookInfo = dyHookSourceInfo;
        }
        else
        {
            gameData->hookInfo = getD3D9HookSourceInfo();
        }
        gameData->hookData = gameData->hookInfo.create(settings, gameData->source);
        gameData->antiCheat = antiCheat;
    }
	gameData->hookInfo.update(gameData->hookData, settings);
	dyWindowSourceInfo.update(gameData->windowData, settings);
	gameData->textureReady = none;

}
extern bool hasHookSuccessed(void *data);
extern bool hasWCSuccessed(void *data);

static void gameSingleTick(void *data, float seconds)
{
	if (!data) return;

	struct GameData *gameData = data;
	gameData->textureReady = none;
//	dyHookSourceInfo.video_tick(gameData->hookData, seconds);
//	if (dyHookSourceInfo.get_width(gameData->hookData) > 0 &&
//	    dyHookSourceInfo.get_height(gameData->hookData) > 0) {
//		gameData->cx = dyHookSourceInfo.get_width(gameData->hookData);
//		gameData->cy = dyHookSourceInfo.get_height(gameData->hookData);
	gameData->hookInfo.video_tick(gameData->hookData, seconds);
	if (gameData->hookInfo.get_width(gameData->hookData) > 0 &&
		gameData->hookInfo.get_height(gameData->hookData) > 0) {
		gameData->cx = gameData->hookInfo.get_width(gameData->hookData);
		gameData->cy = gameData->hookInfo.get_height(gameData->hookData);
		gameData->textureReady = hook;
		gameData->hasSuccessed = hasHookSuccessed(gameData->hookData);
		gameData->hasSuccessed = true;
		return;
	}

	dyWindowSourceInfo.video_tick(gameData->windowData, seconds);
	if (dyWindowSourceInfo.get_width(gameData->windowData) > 0 &&
		dyWindowSourceInfo.get_height(gameData->windowData) > 0)
	{
		gameData->cx =
			dyWindowSourceInfo.get_width(gameData->windowData);
		gameData->cy =
			dyWindowSourceInfo.get_height(gameData->windowData);
		gameData->textureReady = win;
		gameData->hasSuccessed = hasWCSuccessed(gameData->windowData);
		return;
	}
	gameData->cx = 0;
	gameData->cy = 0;
}

static void gameRender(void *data, gs_effect_t *effect)
{
	struct GameData *gameData = data;
	switch (gameData->textureReady) {
	case hook:
		 //dyHookSourceInfo.video_render(gameData->hookData, effect);
		gameData->hookInfo.video_render(gameData->hookData, effect);
		break;
	case win:
		dyWindowSourceInfo.video_render(gameData->windowData, effect);
		break;
	default:
		break;
	}
}

extern HWND getHookHWND(void *data);
extern HWND getWinHWND(void *data);
extern HWND getGameSingleHwnd(void *data){
    struct GameData *gameData = data;
    HWND gameHWND = getHookHWND(gameData->hookData);
    if(!gameHWND){
        gameHWND= getWinHWND(gameData->windowData);
    }
    return gameHWND;
}

extern HWND getGameHookHwnd(void *data){
    struct GameData *gameData = data;
    return getHookHWND(gameData->hookData);
}

extern bool hasSingleGameSuccessed(void *data)
{
	struct GameData *gameData = data;
	return gameData->hasSuccessed;
}

struct obs_source_info dyGameSingleSourceInfo = {
	.id = "DY_GameSingle",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = gameSingleGetName,
	.create = gameSingleCreate,
	.destroy = gameSingleDestroy,
	.get_width = gameSingleGetWidth,
	.get_height = gameSingleGetHeight,
	.get_defaults = gameSingleDefaults,
	.get_properties = gameSingleProperties,
	.update = gameSingleUpdate,
	.video_tick = gameSingleTick,
	.video_render = gameRender,
	.icon_type = OBS_ICON_TYPE_GAME_CAPTURE,
};
