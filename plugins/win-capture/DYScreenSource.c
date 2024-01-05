#include <windows.h>

#include <util/windows/win-version.h>
#include <util/platform.h>
#include <util/dstr.h>

#include <obs-module.h>
#include <graphics/vec2.h>

#include "cursor-capture.h"

#include "../../Config/DYLiveConstantDef.h"
//#include "DYScreenSource.h"

/* clang-format off */

/* clang-format on */

#define RESET_INTERVAL_SEC 3.0f

struct DYScreenSourceData {
    obs_source_t *source;
    int monitor;
    bool capture_cursor;
    bool showing;

    long x;
    long y;
    int rot;
    uint32_t width;
    uint32_t height;
    gs_duplicator_t *duplicator;
    float reset_timeout;
    struct cursor_data cursor_data;

	uint32_t ox;
	uint32_t oy;
	uint32_t cx;
	uint32_t cy;
};

/* ------------------------------------------------------------------------- */

static inline void update_settings(struct DYScreenSourceData *capture,
    obs_data_t *settings)
{
    bool initNeeded = false;
    int monitor= (int)obs_data_get_int(settings, c_keyIndex);
    if (capture->monitor != monitor)
    {
        initNeeded = true;
        capture->monitor = monitor;
    }

    capture->capture_cursor = obs_data_get_bool(settings, c_keyCaptureCursor);
	capture->ox = (uint32_t)obs_data_get_int(settings, c_keyX);
	capture->oy = (uint32_t)obs_data_get_int(settings, c_keyY);
	capture->cx = (uint32_t)obs_data_get_int(settings, c_keyWidth);
	capture->cy = (uint32_t)obs_data_get_int(settings, c_keyHeight);

    if (initNeeded)
    {
        obs_enter_graphics();
        gs_duplicator_destroy(capture->duplicator);
        obs_leave_graphics();
        capture->duplicator = NULL;
        capture->width = 0;
        capture->height = 0;
        capture->x = 0;
        capture->y = 0;
        capture->rot = 0;
        capture->reset_timeout = RESET_INTERVAL_SEC;
    }
}

/* ------------------------------------------------------------------------- */

static const char *duplicator_capture_getname(void *unused)
{
    UNUSED_PARAMETER(unused);
    return "全屏";
}

static void duplicator_capture_destroy(void *data)
{
    struct DYScreenSourceData *capture = data;

    obs_enter_graphics();

    gs_duplicator_destroy(capture->duplicator);
    cursor_data_free(&capture->cursor_data);

    obs_leave_graphics();

    bfree(capture);
}

static void duplicator_capture_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(settings, "index", 0);
    obs_data_set_default_bool(settings, "capture_cursor", false);
	obs_data_set_default_int(settings, c_keyX, 0);
	obs_data_set_default_int(settings, c_keyY, 0);
	obs_data_set_default_int(settings, c_keyWidth, 0);
	obs_data_set_default_int(settings, c_keyHeight, 0);
    obs_data_t *privateData = obs_data_create();
    obs_data_set_default_obj(settings, "private", privateData);
    obs_data_release(privateData);
}

static void duplicator_capture_update(void *data, obs_data_t *settings)
{
    struct DYScreenSourceData *mc = data;

    update_settings(mc, settings);
}

static void *duplicator_capture_create(obs_data_t *settings,
    obs_source_t *source)
{
    blog(LOG_INFO, "duplicator_capture_create");
    struct DYScreenSourceData *capture;

    capture = bzalloc(sizeof(struct DYScreenSourceData));
    capture->source = source;
    capture->monitor = -1;
    update_settings(capture, settings);

    return capture;
}

static void reset_capture_data(struct DYScreenSourceData *capture)
{
    struct gs_monitor_info monitor_info = { 0 };
    gs_texture_t *texture = gs_duplicator_get_texture(capture->duplicator);

    gs_get_duplicator_monitor_info(capture->monitor, &monitor_info);
    capture->width = gs_texture_get_width(texture);
    capture->height = gs_texture_get_height(texture);
    capture->x = monitor_info.x;
    capture->y = monitor_info.y;
    capture->rot = monitor_info.rotation_degrees;
}

static void free_capture_data(struct DYScreenSourceData *capture)
{
    gs_duplicator_destroy(capture->duplicator);
    cursor_data_free(&capture->cursor_data);
    capture->duplicator = NULL;
    capture->width = 0;
    capture->height = 0;
    capture->x = 0;
    capture->y = 0;
    capture->rot = 0;
    capture->reset_timeout = 0.0f;
}

static void duplicator_capture_tick(void *data, float seconds)
{
    //blog(LOG_INFO, "duplicator_capture_tick");
    struct DYScreenSourceData *capture = data;

    /* completely shut down monitor capture if not in use, otherwise it can
     * sometimes generate system lag when a game is in fullscreen mode */
    if (!obs_source_showing(capture->source)) {
//         blog(LOG_INFO, "duplicator_capture_tick obs_source_showing is false");
        if (capture->showing) {
            obs_enter_graphics();
            free_capture_data(capture);
            obs_leave_graphics();

            capture->showing = false;
        }
        return;

        /* always try to load the capture immediately when the source is first
     * shown */
    }
    else if (!capture->showing) {
        capture->reset_timeout = RESET_INTERVAL_SEC;
    }

    obs_enter_graphics();
    //blog(LOG_INFO, "duplicator_capture_tick obs_enter_graphics,capture->duplicator=%d",capture->duplicator);
    if (!capture->duplicator) {
        capture->reset_timeout += seconds;

        if (capture->reset_timeout >= RESET_INTERVAL_SEC) {
            capture->duplicator =
                gs_duplicator_create(capture->monitor);
            blog(LOG_INFO, "duplicator_capture_tick gs_duplicator_create =%d",capture->duplicator);
            capture->reset_timeout = 0.0f;
        }
    }

    if (!!capture->duplicator) {
        if (capture->capture_cursor)
            cursor_capture(&capture->cursor_data);
//         blog(LOG_INFO, "gs_duplicator_update_frame before");
        if (!gs_duplicator_update_frame(capture->duplicator)) {
            free_capture_data(capture);
            blog(LOG_INFO, "gs_duplicator_update_frame failed");
        }
        else if (capture->width == 0) {
            reset_capture_data(capture);
        }
    }

    obs_leave_graphics();

    if (!capture->showing)
        capture->showing = true;

	if (capture->cx <= 0) {
		capture->cx = capture->width;
	}
	if (capture->cy <= 0) {
		capture->cy = capture->height;
	}

    UNUSED_PARAMETER(seconds);
}

static uint32_t duplicator_capture_width(void *data)
{
    struct DYScreenSourceData *capture = data;
    //blog(LOG_INFO, "duplicator_capture_width=%d",capture->rot % 180 == 0 ? capture->cx : capture->cy);
    return capture->rot % 180 == 0 ? capture->cx : capture->cy;
}

static uint32_t duplicator_capture_height(void *data)
{
    struct DYScreenSourceData *capture = data;
    //blog(LOG_INFO, "duplicator_capture_width=%d",capture->rot % 180 == 0 ? capture->cy : capture->cx);
    return capture->rot % 180 == 0 ? capture->cy : capture->cx;
}

static void draw_cursor(struct DYScreenSourceData *capture)
{
	/* cursor_draw(&capture->cursor_data, -capture->x, -capture->y, 1.0f, 1.0f,
		 capture->rot % 180 == 0 ? capture->cx : capture->cy,
		 capture->rot % 180 == 0 ? capture->cy : capture->cx);*/
	cursor_draw(&capture->cursor_data, -capture->x- capture->ox, -capture->y- capture->oy, 1.0f, 1.0f,
		capture->rot % 180 == 0 ? capture->cx : capture->cy,
		capture->rot % 180 == 0 ? capture->cy : capture->cx);
}

static void draw_texture_subregion(gs_texture_t *texture, gs_effect_t *effect, uint32_t ox, uint32_t oy, uint32_t cx, uint32_t cy)
{
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	size_t passes;

	gs_effect_set_texture(image, texture);

	passes = gs_technique_begin(tech);
	for (size_t i = 0; i < passes; i++)
	{
		if (gs_technique_begin_pass(tech, i))
		{
			gs_draw_sprite_subregion(texture, 0, ox, oy, cx, cy);
			gs_technique_end_pass(tech);
		}
	}
	gs_technique_end(tech);
}

static void duplicator_capture_render(void *data, gs_effect_t *effect)
{
    struct DYScreenSourceData *capture = data;

    gs_texture_t *texture;
    int rot;

    if (!capture->duplicator)
        return;

    texture = gs_duplicator_get_texture(capture->duplicator);
    if (!texture)
        return;

    effect = obs_get_base_effect(OBS_EFFECT_OPAQUE);

    rot = capture->rot;

    while (gs_effect_loop(effect, "Draw")) {
        if (rot != 0) {
            float x = 0.0f;
            float y = 0.0f;

            switch (rot) {
            case 90:
                x = (float)capture->cy;
                break;
            case 180:
                x = (float)capture->cx;
                y = (float)capture->cy;
                break;
            case 270:
                y = (float)capture->cx;
                break;
            }

            gs_matrix_push();
            gs_matrix_translate3f(x, y, 0.0f);
            gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, RAD((float)rot));
        }

        //obs_source_draw(texture, capture->width /2, capture->height /2, capture->width, capture->height, false);
		//gs_draw_sprite_subregion(texture, capture->width / 2, capture->height / 2, capture->width, capture->height, false);
		draw_texture_subregion(texture, effect, capture->ox, capture->oy, capture->cx, capture->cy);

		if (rot != 0)
            gs_matrix_pop();
    }

    if (capture->capture_cursor) {
        effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

        while (gs_effect_loop(effect, "Draw")) {
            draw_cursor(capture);
        }
    }
}

static bool get_monitor_props(obs_property_t *monitor_list, int monitor_idx)
{
    struct dstr monitor_desc = { 0 };
    struct gs_monitor_info info;

    if (!gs_get_duplicator_monitor_info(monitor_idx, &info))
        return false;

    dstr_catf(&monitor_desc, "%s %d: %ldx%ld @ %ld,%ld",
        obs_module_text("Monitor"), monitor_idx + 1, info.cx, info.cy,
        info.x, info.y);

    obs_property_list_add_int(monitor_list, monitor_desc.array,
        monitor_idx);

    dstr_free(&monitor_desc);

    return true;
}

static obs_properties_t *duplicator_capture_properties(void *unused)
{
    int monitor_idx = 0;

    UNUSED_PARAMETER(unused);

    obs_properties_t *props = obs_properties_create();

    obs_property_t *monitors = obs_properties_add_list(
        props, "index", obs_module_text("index"), OBS_COMBO_TYPE_LIST,
        OBS_COMBO_FORMAT_INT);

    obs_properties_add_bool(props, "capture_cursor",
        obs_module_text("CaptureCursor"));

    obs_enter_graphics();

    while (get_monitor_props(monitors, monitor_idx++))
        ;

    obs_leave_graphics();

    return props;
    //return props;
}


struct obs_source_info dyScreenSourceInfo = {
    .id = "DY_FullScreen",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
            OBS_SOURCE_DO_NOT_DUPLICATE,
    .get_name = duplicator_capture_getname,
    .create = duplicator_capture_create,
    .destroy = duplicator_capture_destroy,
    .video_render = duplicator_capture_render,
    .video_tick = duplicator_capture_tick,
    .update = duplicator_capture_update,
    .get_width = duplicator_capture_width,
    .get_height = duplicator_capture_height,
    .get_defaults = duplicator_capture_defaults,
    .get_properties = duplicator_capture_properties,
    .icon_type = OBS_ICON_TYPE_DESKTOP_CAPTURE,
};


static const char *screenShotGetname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "截屏";
}

struct obs_source_info dyScreenShotInfo = {
	.id = "DY_ScreenShot",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = screenShotGetname,
	.create = duplicator_capture_create,
	.destroy = duplicator_capture_destroy,
	.video_render = duplicator_capture_render,
	.video_tick = duplicator_capture_tick,
	.update = duplicator_capture_update,
	.get_width = duplicator_capture_width,
	.get_height = duplicator_capture_height,
	.get_defaults = duplicator_capture_defaults,
	.get_properties = duplicator_capture_properties,
	.icon_type = OBS_ICON_TYPE_DESKTOP_CAPTURE,
};

