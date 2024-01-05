
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
#include <util/windows/win-version.h>

#include "../../Config/DYLiveConstantDef.h"
#include "gameCover.h"

enum TipsType
{
	TipsTypeUnknown=0,
	TipsTypeKnownStart=1,
	TipsTypeKnownEnter=2
};

struct GameCoverData {
	obs_source_t *source;

    struct dstr gameName;
    uint32_t gameNameFontSize;
    uint32_t gameNameColor;

    struct dstr tips;
    uint32_t tipsFontSize;
    uint32_t tipsColor;

    struct dstr des;
    uint32_t desFontSize;
    uint32_t desColor;

    struct dstr cover;

    uint32_t cx;
    uint32_t cy;

    gs_texture_t *texture;

    bool needUpdate;

	enum TipsType tipsType;

	//线程锁
	pthread_mutex_t mutex;
};

static void gameCoverUpdate(void *data, obs_data_t *settings);

static const char *gameCoverGetName(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("DYGameCoverSource");
}

static void setTips(void *data, calldata_t *param)
{
	struct GameCoverData *gameCoverData = data;
	bool find = calldata_bool(param, c_keyEnable);
	if (gameCoverData->tipsType != ( find? TipsTypeKnownStart: TipsTypeKnownEnter))
	{
		pthread_mutex_lock(&gameCoverData->mutex);
		gameCoverData->needUpdate = true;
		pthread_mutex_unlock(&gameCoverData->mutex);
	}
	gameCoverData->tipsType = find ? TipsTypeKnownStart : TipsTypeKnownEnter;
}

static void initSignalHandler(void *data)
{
	struct GameCoverData *gameCoverData = data;

	proc_handler_t *procHandler = obs_source_get_proc_handler(gameCoverData->source);
	proc_handler_add(procHandler, "void ShowExecTips()", setTips, gameCoverData);
}

static void *gameCoverCreate(obs_data_t *settings, obs_source_t *source)
{
    struct GameCoverData *gameCoverData =
        bzalloc(sizeof(struct GameCoverData));
	gameCoverData->source = source;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&gameCoverData->mutex, &attr);
	initSignalHandler(gameCoverData);
    dstr_init(&gameCoverData->gameName);
    dstr_init(&gameCoverData->tips);
    dstr_init(&gameCoverData->des);
    dstr_init(&gameCoverData->cover);

	gameCoverData->needUpdate = true;
	gameCoverData->tipsType = TipsTypeUnknown;

    gameCoverUpdate(gameCoverData, settings);
    return gameCoverData;
}

static void gameCoverDestroy(void *data)
{
    struct GameCoverData *gameCoverData = data;
    if (NULL == gameCoverData) {
        return;
    }
	if (gameCoverData->mutex)
	{
		pthread_mutex_destroy(&gameCoverData->mutex);
	}

    dstr_free(&gameCoverData->gameName);
    dstr_free(&gameCoverData->tips);
    dstr_free(&gameCoverData->des);
    dstr_free(&gameCoverData->cover);

    if (gameCoverData->texture) {
        obs_enter_graphics();
        gs_texture_destroy(gameCoverData->texture);
        obs_leave_graphics();
		gameCoverData->texture=NULL;
    }
    if (gameCoverData) {
        bfree(gameCoverData);
    }
}

static uint32_t getGameCoverWidth(void *data)
{
    struct GameCoverData *gameCoverData = data;
    return gameCoverData->cx;
}

static uint32_t getGameCoverHeight(void *data)
{
    struct GameCoverData *gameCoverData = data;
    return gameCoverData->cy;
}

static void gameCoverDefaults(obs_data_t *defaults)
{
    //size
    obs_data_set_default_int(defaults, "cx", 1920);
    obs_data_set_default_int(defaults, "cy", 1080);
    //gameNameData;
    obs_data_set_default_string(defaults, "game_name", "");
    obs_data_set_default_int(defaults, "game_name_font_size", 96);
    obs_data_set_default_int(defaults, "game_name_color", 0xFFEEEEEE);
    //tipsData;
    obs_data_set_default_string(defaults, "tips", L"获取游戏数据中\0");
    obs_data_set_default_int(defaults, "tips_font_size", 40);
    obs_data_set_default_int(defaults, "tips_color", 0xFFFF7700);
    //descriptionData;
    struct dstr des;
    dstr_init(&des);
    const wchar_t *desWt = L"此提示区域不代表游戏画面的实际位置和大小\0";
    dstr_from_wcs(&des, desWt);
    obs_data_set_default_string(defaults, "des", des.array);
    dstr_free(&des);


    obs_data_set_default_int(defaults, "des_font_size", 40);
    obs_data_set_default_int(defaults, "des_color", 0xFFEEEEEE);

    obs_data_t *privateData = obs_data_create();
    obs_data_set_default_obj(defaults, "private", privateData);
    obs_data_release(privateData);
}

static obs_properties_t *gameCoverProperties(void *data)
{
    struct GameCoverData *gameCoverData = data;

    obs_properties_t *ppts = obs_properties_create();
    obs_properties_add_text(ppts, "game_name", "game_name", OBS_TEXT_DEFAULT);
    obs_properties_add_text(ppts, "tips", "tips_text", OBS_TEXT_DEFAULT);
    obs_properties_add_text(ppts, "des", "des", OBS_TEXT_DEFAULT);

    return ppts;
}

static void updateTexture(void *data) {
    struct GameCoverData *gameCoverData = data;
    if (!gameCoverData)
    {
        return;
    }
	if (!gameCoverData->needUpdate && gameCoverData->texture)
	{
		return;
	}
	if (!gameCoverData->needUpdate)
	{
		return;
	}
	const wchar_t *tipsWt;
	switch (gameCoverData->tipsType)
	{
	case TipsTypeUnknown:
		tipsWt = L"获取游戏数据中\0";
		break;
	case TipsTypeKnownStart:
		tipsWt = L"双击启动游戏\0";
		break;
	case TipsTypeKnownEnter:
		tipsWt = L"进入游戏后自动获取游戏画面\n请先进入游戏\0";
		break;
	default:
		tipsWt = L"获取游戏数据中\0";
		break;
	}
	dstr_from_wcs(&gameCoverData->tips, tipsWt);

    wchar_t *gameName = dstr_to_wcs(&gameCoverData->gameName);
    //const wchar_t *gameName = L"故人西辞黄鹤楼，烟花三月下扬州。孤帆远影碧空尽，唯见长江天际流。";
    wchar_t *tips = dstr_to_wcs(&gameCoverData->tips);
    wchar_t *des = dstr_to_wcs(&gameCoverData->des);
    wchar_t *cover = dstr_to_wcs(&gameCoverData->cover);
    //float fontSizeRateX = ((float)gameCoverData->cx) / 1920.0f;
    //float fontSizeRateY = ((float)gameCoverData->cy) / 1080.0f;
    //float fontSizeRate = fontSizeRateX < fontSizeRateY ? fontSizeRateX : fontSizeRateY;


    gameCoverData->texture = ((cover != NULL) && (wcslen(cover) > 0))
        ? buildGameImage(gameCoverData->texture, 1920, 1080, cover)
        : buildGamePixel(gameCoverData->texture, 1920, 1080,
        gameName, gameCoverData->gameNameFontSize, gameCoverData->gameNameColor,
        tips, gameCoverData->tipsFontSize, gameCoverData->tipsColor,
        des, gameCoverData->desFontSize, gameCoverData->desColor);
    bfree(gameName);
    bfree(tips);
    bfree(des);
    bfree(cover);
	if (gameCoverData->texture)
	{
		pthread_mutex_lock(&gameCoverData->mutex);
		gameCoverData->needUpdate = false;
		pthread_mutex_unlock(&gameCoverData->mutex);
	}
};

static void gameCoverUpdate(void *data, obs_data_t *settings)
{
    struct GameCoverData *gameCoverData = data;
    //size
    gameCoverData->cx = (uint32_t)obs_data_get_int(settings, "cx");
    gameCoverData->cy = (uint32_t)obs_data_get_int(settings, "cy");
    //text;c_keyTitleName
    const char *gameName = obs_data_get_string(settings, c_keyGameName);
	//const char *gameName = obs_data_get_string(settings, c_keyTitleName);
    //const char *tips = obs_data_get_string(settings, "tips");
    const char *des = obs_data_get_string(settings, "des");
    const char *cover = obs_data_get_string(settings, c_keyCover);

    dstr_copy(&gameCoverData->gameName, gameName);
    //dstr_copy(&gameCoverData->tips, tips);
    dstr_copy(&gameCoverData->des, des);
    dstr_copy(&gameCoverData->cover, cover);

    //font size
    gameCoverData->gameNameFontSize = (uint32_t)obs_data_get_int(settings, "game_name_font_size");
    gameCoverData->tipsFontSize = (uint32_t)obs_data_get_int(settings, "tips_font_size");
    gameCoverData->desFontSize = (uint32_t)obs_data_get_int(settings, "des_font_size");
    //color
    gameCoverData->gameNameColor = (uint32_t)obs_data_get_int(settings, "game_name_color");
    gameCoverData->tipsColor = (uint32_t)obs_data_get_int(settings, "tips_color");
    gameCoverData->desColor = (uint32_t)obs_data_get_int(settings, "des_color");

    //update texture
    //updateTexture(gameCoverData);
}

static void gameCoverTick(void *data, float seconds) {
	struct GameCoverData *gameCoverData = data;
	if (!obs_source_showing(gameCoverData->source))
		return;
	updateTexture(gameCoverData);
}

static void gameCoverRender(void *data, gs_effect_t *effect) {
    struct GameCoverData *gameCoverData = data;
    if (!gameCoverData->texture) {
        return;
    }
    effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
    gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

    gs_technique_begin(tech);
    gs_technique_begin_pass(tech, 0);

    gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), gameCoverData->texture);
    gs_draw_sprite(gameCoverData->texture, 0, gameCoverData->cx, gameCoverData->cy);

    gs_technique_end_pass(tech);
    gs_technique_end(tech);
}

struct obs_source_info dyGameCoverSourceInfo = {
    .id = "DY_GameCover",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
    OBS_SOURCE_DO_NOT_DUPLICATE,
    .get_name = gameCoverGetName,
    .create = gameCoverCreate,
    .destroy = gameCoverDestroy,
    .get_width = getGameCoverWidth,
    .get_height = getGameCoverHeight,
    .get_defaults = gameCoverDefaults,
    .get_properties = gameCoverProperties,
    .update = gameCoverUpdate,
    .video_tick = gameCoverTick,
    .video_render = gameCoverRender,
    .icon_type = OBS_ICON_TYPE_GAME_CAPTURE,
};


