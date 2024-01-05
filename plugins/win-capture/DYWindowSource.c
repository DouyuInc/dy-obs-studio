#pragma once

#include "DYWindowSource.h"

#include <stdlib.h>
#include <util/dstr.h>
#include "dc-capture.h"
#include "window-helpers.h"
#include "util/platform.h"
#include "winrt-capture.h"
#include "../../Deps/w32-pthreads/pthread.h"
#include "../../Config/DYLiveConstantDef.h"

#include "gameCover.h"
#include "DYWindowSource.h"
/* clang-format off */

#define TEXT_WINDOW_CAPTURE obs_module_text("DYWindowCapture")
#define TEXT_WINDOW         obs_module_text("WindowCapture.Window")
#define TEXT_METHOD         obs_module_text("WindowCapture.Method")
#define TEXT_METHOD_AUTO    obs_module_text("WindowCapture.Method.Auto")
#define TEXT_METHOD_BITBLT  obs_module_text("WindowCapture.Method.BitBlt")
#define TEXT_METHOD_WGC     obs_module_text("WindowCapture.Method.WindowsGraphicsCapture")
#define TEXT_MATCH_PRIORITY obs_module_text("WindowCapture.Priority")
#define TEXT_MATCH_TITLE    obs_module_text("WindowCapture.Priority.Title")
#define TEXT_MATCH_CLASS    obs_module_text("WindowCapture.Priority.Class")
#define TEXT_MATCH_EXE      obs_module_text("WindowCapture.Priority.Exe")
#define TEXT_CAPTURE_CURSOR obs_module_text("CaptureCursor")
#define TEXT_COMPATIBILITY  obs_module_text("Compatibility")
#define TEXT_CLIENT_AREA    obs_module_text("ClientArea")

/* clang-format on */

#define WC_CHECK_TIMER 1.0f

struct winrt_exports {
    BOOL *(*winrt_capture_supported)();
    BOOL *(*winrt_capture_cursor_toggle_supported)();
    struct winrt_capture *(*winrt_capture_init)(BOOL cursor, HWND window,
        BOOL client_area);
    void(*winrt_capture_free)(struct winrt_capture *capture);
    void(*winrt_capture_show_cursor)(struct winrt_capture *capture,
        BOOL visible);
    void(*winrt_capture_render)(struct winrt_capture *capture,
        gs_effect_t *effect);
    uint32_t(*winrt_capture_width)(const struct winrt_capture *capture);
    uint32_t(*winrt_capture_height)(const struct winrt_capture *capture);
	gs_texture_t*(*winrt_get_texture)(const struct winrt_capture *capture);
};

enum window_capture_method {
    METHOD_AUTO,
    METHOD_BITBLT,
    METHOD_WGC,
};

extern struct winrt_capture;

struct dyWindouSource {
    obs_source_t *source;

    char *title;
    char *class;
    char *processName;
    char *executable;
    bool layered;
    enum window_capture_method method;
    enum window_priority priority;
    bool cursor;
    bool compatibility;
    bool client_area;
    bool use_wildcards; /* TODO */

    struct dc_capture capture;

    bool wgc_supported;
    bool previously_failed;
    void *winrt_module;
    struct winrt_exports exports;
    struct winrt_capture *capture_winrt;

    float resize_timer;
    float check_window_timer;
    float cursor_check_time;

    HWND window;
    RECT last_rect;

    bool sentStateEnable;
	bool hasSendState;
	bool captureSuccessed;
	bool hasSuccessed;
    float accTime;
    float timeOut;
	float methodTime;
	float methodTimeOut;
	float checkTextureTime;
	float checkTextureTimeOut;

    //线程锁
    pthread_mutex_t mutex;

	//用于黑频检测不用于draw
	gs_texture_t *texture;

	//
	bool hasSetted;

	uint32_t srcWidth;
	uint32_t srcHeight;
	bool methodChanged;
    // 增加settings保存 [9/14/2020 chensi2]
    obs_data_t* settings;
};

static const char *wgc_partial_match_classes[] = {
//     "Chrome",
    //"Mozilla",
    NULL,
};

static const char *wgc_whole_match_classes[] = {
    //"ApplicationFrameWindow",
    //"Windows.UI.Core.CoreWindow",
    //"XLMAIN",        /* excel*/
    //"PPTFrameClass", /* powerpoint */
    //"OpusApp",       /* word */
    //"kwmusicmaindlg",
    //"TXGuiFoundation",
    NULL,
};

static enum window_capture_method
choose_method(enum window_capture_method method, bool wgc_supported,
    const char *current_class)
{
// 	return METHOD_BITBLT;
	//if (wgc_supported)
	//{
	//	return METHOD_WGC;
	//}
    if (!wgc_supported)
        return METHOD_BITBLT;

    if (method != METHOD_AUTO)
        return method;

    if (!current_class)
        return METHOD_BITBLT;

    // 不内置白名单，白名单通过后台下发 [9/8/2021 chensi2]
//     const char **class = wgc_partial_match_classes;
//     while (*class) {
//         if (astrstri(current_class, *class) != NULL) {
//             return METHOD_WGC;
//         }
//         class ++;
//     }
// 
//     class = wgc_whole_match_classes;
//     while (*class) {
//         if (astrcmpi(current_class, *class) == 0) {
//             return METHOD_WGC;
//         }
//         class ++;
//     }

    return METHOD_BITBLT;
}


static void update_settings(struct dyWindouSource *wc, obs_data_t *s)
{
    int method = (int)obs_data_get_int(s, "method");
    const char *window = obs_data_get_string(s, "window");
    int priority = (int)obs_data_get_int(s, "priority");

    const char *title = obs_data_get_string(s, "title_name");
    const char *class = obs_data_get_string(s, "class_name");
    const char *executable = obs_data_get_string(s, "process_name");
    wc->layered = obs_data_get_bool(s, "layered");
    wc->window = (HWND)obs_data_get_int(s, c_keyHWnd);

    bfree(wc->title);
    bfree(wc->class);
    bfree(wc->executable);
    wc->title = bzalloc(strlen(title) + 1);
    wc->class = bzalloc(strlen(class) + 1);
    wc->executable = bzalloc(strlen(executable) + 1);
    memcpy(wc->title, title, strlen(title));
    memcpy(wc->class, class, strlen(class));
    memcpy(wc->executable, executable, strlen(executable));

    if (wc->window)
    {
        struct dstr title;
        struct dstr className;
        struct dstr ProcessName;
        dstr_init(&title);
        dstr_init(&className);
        dstr_init(&ProcessName);
        get_window_title(&title, wc->window);
        get_window_class(&className, wc->window);
        get_window_exe(&ProcessName, wc->window);

        if (/*0!=astrcmpi(title.array, wc->title)||*/
            0 != astrcmpi(className.array, wc->class)||
            0 != astrcmpi(ProcessName.array, wc->executable)
            )
        {
            wc->window = NULL;
        }
        
        dstr_free(&title);
        dstr_free(&className);
        dstr_free(&ProcessName);
    }

    //build_window_strings(window, &wc->class, &wc->title, &wc->executable);

    if (wc->title != NULL) {
        blog(LOG_INFO,
            "[window-capture: '%s'] update settings:\n"
            "\texecutable: %s",
            obs_source_get_name(wc->source), wc->executable);
        blog(LOG_DEBUG, "\tclass:      %s", wc->class);
    }

    wc->method = choose_method(method, wc->wgc_supported, wc->class);
    if (NULL!= wc->processName && strlen(wc->processName) >0)
    {
        wc->priority = WINDOW_PRIORITY_EXE;
    }
    else
    {
        wc->priority = WINDOW_PRIORITY_CLASS;
    }
    
    wc->cursor = obs_data_get_bool(s, "capture_cursor");
    wc->use_wildcards = obs_data_get_bool(s, "use_wildcards");
	wc->use_wildcards = true;
    wc->compatibility = obs_data_get_bool(s, "compatibility");
    wc->client_area = obs_data_get_bool(s, "client_area");
	wc->client_area = false;
	wc->methodChanged = false;


}

/* ------------------------------------------------------------------------- */

static const char *windowSourceGetname(void *unused)
{
    UNUSED_PARAMETER(unused);
    return "窗口";
}

#define WINRT_IMPORT(func)                                        \
	do {                                                      \
		exports->func = os_dlsym(module, #func);          \
		if (!exports->func) {                             \
			success = false;                          \
			blog(LOG_ERROR,                           \
			     "Could not load function '%s' from " \
			     "module '%s'",                       \
			     #func, module_name);                 \
		}                                                 \
	} while (false)

static bool load_winrt_imports(struct winrt_exports *exports, void *module,
    const char *module_name)
{
    bool success = true;

    WINRT_IMPORT(winrt_capture_supported);
    WINRT_IMPORT(winrt_capture_cursor_toggle_supported);
    WINRT_IMPORT(winrt_capture_init);
    WINRT_IMPORT(winrt_capture_free);
    WINRT_IMPORT(winrt_capture_show_cursor);
    WINRT_IMPORT(winrt_capture_render);
    WINRT_IMPORT(winrt_capture_width);
    WINRT_IMPORT(winrt_capture_height);
	WINRT_IMPORT(winrt_get_texture);

    return success;
}

static void onWndCapFailed(void *data)
{
    struct dyWindouSource *wc = data;
    signal_handler_t *signal = obs_source_get_signal_handler(wc->source);
    calldata_t *param = calldata_create();
//      int     - c_keyHWnd         - 窗口句柄指针
//      str     - c_keyGameName     - 游戏名称
//      str     - c_keyTitleName    - 标题名称
//      str     - c_keyClassName    - 类名
//      bool    - c_keyShareMem     - 是否共享内存捕获
    calldata_set_int(param, c_keyHWnd, wc->window);
    calldata_set_string(param, c_keyTitleName, wc->title);
    calldata_set_string(param, c_keyClassName, wc->class);
    calldata_set_string(param, c_keyProcessName, obs_data_get_string(wc->settings, c_keyProcessName));
    calldata_set_string(param, c_keyGameName, obs_data_get_string(wc->settings, c_keyGameName));
    signal_handler_signal(signal, c_funcWndCapFailed, param);
    calldata_destroy(param);
    param = NULL;

}

static void stateRepoertSwitch(void *data, calldata_t *param)
{
    struct dyWindouSource *wc = data;
    pthread_mutex_lock(&wc->mutex);
    wc->sentStateEnable = calldata_bool(param, c_keyEnable);
    pthread_mutex_unlock(&wc->mutex);
}

static void initSingal(void *data){
    struct dyWindouSource *s = data;
    //call
    signal_handler_t *signalHandler = obs_source_get_signal_handler(s->source);
	signal_handler_add(signalHandler, "void WndCapFailed()");
    //called
    proc_handler_t *procHandler = obs_source_get_proc_handler(s->source);
    proc_handler_add(procHandler, "void ErrorReport()", stateRepoertSwitch, s);
}

static void *windowSourceCreate(obs_data_t *settings, obs_source_t *source)
{
    struct dyWindouSource *wc = bzalloc(sizeof(struct dyWindouSource));
    wc->source = source;

    obs_enter_graphics();
    const bool uses_d3d11 = gs_get_device_type() == GS_DEVICE_DIRECT3D_11;
    obs_leave_graphics();

	blog(LOG_INFO, "wc->uses_d3d11=%d", uses_d3d11);

    if (uses_d3d11) {
//         char module[MAX_PATH] = {0};
//         GetModuleFileNameA(NULL, module, MAX_PATH);
//         char* filebase = strrchr(module, '\\');
//         *filebase = '\0';
//         strcat(module, "\\LibWinRT.dll");
// 
// 		blog(LOG_DEBUG, "DYWindowSource get LibWinRT.dll path:      %s", module);

        bool use_winrt_capture = false;
		static const char *const module = "LibWinRT";
        wc->winrt_module = os_dlopen(module);
		blog(LOG_DEBUG, "DYWindowSource load winrt_module:      %d", wc->winrt_module);
		
        if (wc->winrt_module &&
            load_winrt_imports(&wc->exports, wc->winrt_module,
                module) &&
            wc->exports.winrt_capture_supported()) {
            wc->wgc_supported = true;
			blog(LOG_DEBUG, "DYWindowSource load winrt_imports");
        }
    }
	blog(LOG_INFO, "wc->wgc_supported=%d", wc->wgc_supported);

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&wc->mutex, &attr);
    initSingal(wc);
    wc->sentStateEnable = false;
    update_settings(wc, settings);
    // 增加settings保存 [9/14/2020 chensi2]
    wc->settings = settings;
    return wc;
}

static void wc_actual_destroy(void *data)
{
    struct dyWindouSource *wc = data;
	if (wc->texture)
	{
		wc->texture = NULL;
	}
    if (wc->mutex)
    {
        pthread_mutex_destroy(&wc->mutex);
    }
    if (wc->capture_winrt) {
        wc->exports.winrt_capture_free(wc->capture_winrt);
    }

    obs_enter_graphics();
    dc_capture_free(&wc->capture);
    obs_leave_graphics();

    bfree(wc->title);
    bfree(wc->class);
    bfree(wc->executable);

    if (wc->winrt_module)
        os_dlclose(wc->winrt_module);

    bfree(wc);
}

static void windowSourceDestroy(void *data)
{
    struct dyWindouSource *wc = data;
    obs_queue_task(OBS_TASK_GRAPHICS, wc_actual_destroy, data, false);
}

static void windowSourceUpdate(void *data, obs_data_t *settings)
{
    struct dyWindouSource *wc = data;
	//if (wc->hasSetted)
	//{
	//	return;
	//}
    update_settings(wc, settings);
    /* forces a reset */
    //wc->window = NULL;
    wc->check_window_timer = WC_CHECK_TIMER;

    wc->previously_failed = false;
    wc->accTime = 0.0f;
    wc->timeOut = 1.0f;
	wc->methodTime = 0.0f;
	wc->methodTimeOut = 2.5f;
	wc->checkTextureTime = 0.0f;
	wc->checkTextureTimeOut = 2.5f;
	wc->hasSetted = true;
}

static uint32_t windowSourceGetWidth(void *data)
{
    struct dyWindouSource *wc = data;
	return wc->srcWidth;
    //return (wc->method == METHOD_WGC)
    //    ? wc->exports.winrt_capture_width(wc->capture_winrt)
    //    : wc->capture.width;
}

static uint32_t windowSourceGetHeight(void *data)
{
    struct dyWindouSource *wc = data;
	return wc->srcHeight;
    //return (wc->method == METHOD_WGC)
    //    ? wc->exports.winrt_capture_height(wc->capture_winrt)
    //    : wc->capture.height;
}

static void windowSourceGetDefaults(obs_data_t *defaults)
{
    obs_data_set_default_int(defaults, "method", METHOD_AUTO);
    obs_data_set_default_int(defaults, "priority", 0);
    obs_data_set_default_bool(defaults, "capture_cursor", false);
    obs_data_set_default_bool(defaults, "compatibility", false);
    obs_data_set_default_bool(defaults, "client_area", true);

    obs_data_set_default_string(defaults, "title_name", "");
    obs_data_set_default_string(defaults, "class_name", "");
    obs_data_set_default_string(defaults, "process_name", "");
    obs_data_set_default_bool(defaults, "layered", false);


    obs_data_t *privateData = obs_data_create();
    obs_data_set_default_obj(defaults, "private", privateData);
    obs_data_release(privateData);
}

static void update_settings_visibility(obs_properties_t *props,
    struct dyWindouSource *wc)
{
    const enum window_capture_method method = wc->method;
    const bool bitblt_options = method == METHOD_BITBLT;
    const bool wgc_options = method == METHOD_WGC;

    const bool wgc_cursor_toggle =
        wgc_options &&
        wc->exports.winrt_capture_cursor_toggle_supported();

    obs_property_t *p = obs_properties_get(props, "cursor");
    obs_property_set_visible(p, bitblt_options || wgc_cursor_toggle);

    p = obs_properties_get(props, "compatibility");
    obs_property_set_visible(p, bitblt_options);

    p = obs_properties_get(props, "client_area");
    obs_property_set_visible(p, wgc_options);
}

static bool wc_capture_method_changed(obs_properties_t *props,
    obs_property_t *p, obs_data_t *settings)
{
    struct dyWindouSource *wc = obs_properties_get_param(props);
    update_settings(wc, settings);

    update_settings_visibility(props, wc);

    return true;
}

extern bool check_window_property_setting(obs_properties_t *ppts,
    obs_property_t *p,
    obs_data_t *settings, const char *val,
    size_t idx);

static bool wc_window_changed(obs_properties_t *props, obs_property_t *p,
    obs_data_t *settings)
{
    struct dyWindouSource *wc = obs_properties_get_param(props);
    update_settings(wc, settings);

    update_settings_visibility(props, wc);

    check_window_property_setting(props, p, settings, "window", 0);
    return true;
}

static obs_properties_t *windowSourceGetProperties(void *data)
{
    struct dyWindouSource *wc = data;

    obs_properties_t *ppts = obs_properties_create();
    obs_properties_set_param(ppts, wc, NULL);

    obs_property_t *p;

    p = obs_properties_add_list(ppts, "window", TEXT_WINDOW,
        OBS_COMBO_TYPE_LIST,
        OBS_COMBO_FORMAT_STRING);
    fill_window_list(p, EXCLUDE_MINIMIZED, NULL);
    obs_property_set_modified_callback(p, wc_window_changed);


    p = obs_properties_add_list(ppts, "method", TEXT_METHOD,
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(p, TEXT_METHOD_AUTO, METHOD_AUTO);
    obs_property_list_add_int(p, TEXT_METHOD_BITBLT, METHOD_BITBLT);
    obs_property_list_add_int(p, TEXT_METHOD_WGC, METHOD_WGC);
    obs_property_list_item_disable(p, 2, !wc->wgc_supported);
    obs_property_set_modified_callback(p, wc_capture_method_changed);

    p = obs_properties_add_list(ppts, "priority", TEXT_MATCH_PRIORITY,
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(p, TEXT_MATCH_TITLE, WINDOW_PRIORITY_TITLE);
    obs_property_list_add_int(p, TEXT_MATCH_CLASS, WINDOW_PRIORITY_CLASS);
    obs_property_list_add_int(p, TEXT_MATCH_EXE, WINDOW_PRIORITY_EXE);

    obs_properties_add_bool(ppts, "cursor", TEXT_CAPTURE_CURSOR);

    obs_properties_add_bool(ppts, "compatibility", TEXT_COMPATIBILITY);

    obs_properties_add_bool(ppts, "client_area", TEXT_CLIENT_AREA);

    return ppts;
}

static void windowSourceHide(void *data)
{
    struct dyWindouSource *wc = data;

    if (wc->texture)
    {
        wc->texture = NULL;
    }
    if (wc->capture_winrt)
    {
        wc->exports.winrt_capture_free(wc->capture_winrt);
        wc->capture_winrt = NULL;
    }

	memset(&wc->last_rect, 0, sizeof(wc->last_rect));
}

#define RESIZE_CHECK_TIME 0.2f
#define CURSOR_CHECK_TIME 0.2f

static void checkCapture(void *data, float seconds)
{
	struct dyWindouSource *wc = data;
	if (wc->hasSuccessed)
	{
		wc->texture = NULL;
		return;
	}

	if (wc->method == METHOD_WGC)
	{
		if (wc->capture_winrt)
		{
			wc->texture = wc->exports.winrt_get_texture(wc->capture_winrt);
			//blog(LOG_DEBUG, "winrt_get_texture: %d", wc->texture);
		}
	}
	else
	{
		wc->texture = wc->capture.texture;
		//blog(LOG_DEBUG, "dc_capture_get_texture: %d", wc->texture);
	}

	if (isTextureValid(wc->texture, true))
	{
		wc->texture = NULL;
		wc->hasSuccessed = true;
		wc->captureSuccessed = true;
		wc->accTime = 0;
	}
	else
	{
		wc->captureSuccessed = false;
		if (wc->checkTextureTime <= wc->checkTextureTimeOut)
		{
			wc->checkTextureTime += seconds;
		}
		else
		{
			if (wc->sentStateEnable)
			{
				wc->accTime += seconds;
				if (wc->accTime > wc->timeOut)
				{
					onWndCapFailed(wc);
                    blog(LOG_INFO,
                         "send window capture filed signal,last time=%f,frame seconds=%f,title=%s",
                         wc->accTime,
                         seconds,
                         wc->title);
                    wc->accTime = 0.0f;
				}
			}
		}
		wc->methodTime += seconds;
		if (wc->methodTime > wc->methodTimeOut && wc->method == METHOD_WGC)
		{
			//wc->method = METHOD_BITBLT;
			//wc->method = wc->method == METHOD_WGC ? METHOD_BITBLT : (wc->wgc_supported ? METHOD_WGC : METHOD_BITBLT);
			//wc->methodChanged = true;
			wc->methodTime = 0.0f;
			blog(LOG_INFO, "window source change method to METHOD_BITBLT");
		}
	}
}

static void windowSourceTick(void *data, float seconds)
{
    struct dyWindouSource *wc = data;


    if (!obs_source_showing(wc->source))
        return;

	checkCapture(wc, seconds);
	RECT rect;
	bool reset_capture = false;

    if (!wc->window || !IsWindow(wc->window)) {
 		wc->srcWidth = 0;
 		wc->srcHeight = 0;
        if (!wc->title && !wc->class)
            return;

        wc->check_window_timer += seconds;

        if (wc->check_window_timer < WC_CHECK_TIMER) {
            if (wc->capture.valid)
                dc_capture_free(&wc->capture);
            return;
        }

        if (wc->capture_winrt) {
            wc->exports.winrt_capture_free(wc->capture_winrt);
            wc->capture_winrt = NULL;
			wc->texture = NULL;
			blog(LOG_DEBUG, "winrt_capture_free line:593");
        }

        wc->check_window_timer = 0.0f;

        wc->window = (wc->method == METHOD_WGC)
            ? find_window_top_level(INCLUDE_MINIMIZED,
                wc->priority,
                wc->class,
                wc->title,
                wc->executable)
            : find_window(INCLUDE_MINIMIZED,
                wc->priority, wc->class,
                wc->title, wc->executable);
        if (!wc->window) 
        {
            if (wc->capture.valid)
            {
                dc_capture_free(&wc->capture);
            }
            return;
        }
        else
        {
            // 增加句柄设置保存 [9/14/2020 chensi2]
            obs_data_set_int(wc->settings, c_keyHWnd, (int)wc->window);
        }
        
        reset_capture = true;

    }
    else if (IsIconic(wc->window) ) {
        return;
    }

    wc->cursor_check_time += seconds;
    if (wc->cursor_check_time > CURSOR_CHECK_TIME) {
        DWORD foreground_pid, target_pid;

        // Can't just compare the window handle in case of app with child windows
        if (!GetWindowThreadProcessId(GetForegroundWindow(),
            &foreground_pid))
            foreground_pid = 0;

        if (!GetWindowThreadProcessId(wc->window, &target_pid))
            target_pid = 0;

        const bool cursor_hidden = foreground_pid && target_pid &&
            foreground_pid != target_pid;
		if (wc->captureSuccessed)
		{
			wc->capture.cursor_hidden = !wc->cursor;
		}
		else
		{
			wc->capture.cursor_hidden = true;
		}
        
		if (wc->capture_winrt)
		{
			if (wc->captureSuccessed)
			{
				wc->exports.winrt_capture_show_cursor(wc->capture_winrt, !wc->cursor);
			}
			else
			{
				wc->exports.winrt_capture_show_cursor(wc->capture_winrt, false);
			}
		}

        wc->cursor_check_time = 0.0f;
    }

    obs_enter_graphics();

    if (wc->method == METHOD_BITBLT) {
        GetClientRect(wc->window, &rect);
		if (wc->methodChanged && wc->capture_winrt)
		{
			wc->texture = NULL;
			wc->exports.winrt_capture_free(wc->capture_winrt);
			wc->capture_winrt = NULL;
			wc->methodChanged = false;
			blog(LOG_DEBUG, "winrt_capture_free line:658");
		}

        if (!reset_capture) {
            wc->resize_timer += seconds;

            if (wc->resize_timer >= RESIZE_CHECK_TIME)
            {
                if ((rect.bottom - rect.top) !=
                    (wc->last_rect.bottom -
                        wc->last_rect.top) ||
                        (rect.right - rect.left) !=
                    (wc->last_rect.right -
                        wc->last_rect.left))
                    reset_capture = true;

                wc->resize_timer = 0.0f;
            }
        }

        if (reset_capture) 
        {
            wc->resize_timer = 0.0f;
            wc->last_rect = rect;
            dc_capture_free(&wc->capture);
            dc_capture_init(
                &wc->capture, 0, 0, rect.right - rect.left, rect.bottom - rect.top, true, wc->compatibility);
        }

        dc_capture_capture(&wc->capture, wc->window);
		if (!IsIconic(wc->window))
		{
			wc->srcWidth = wc->capture.width;
			wc->srcHeight = wc->capture.height;
		}
    }
    else if (wc->method == METHOD_WGC) {
        if (wc->window && (wc->capture_winrt == NULL)) {
            if (!wc->previously_failed) {
				wc->capture_winrt = wc->exports.winrt_capture_init(true, wc->window,wc->client_area);

                if (!wc->capture_winrt) {
                    wc->previously_failed = true;
                }
            }
        }
		if (wc->capture_winrt && !IsIconic(wc->window))
		{
			if (wc->exports.winrt_capture_width && wc->exports.winrt_capture_height && wc->capture_winrt)
			{
                //int srcWidth= wc->exports.winrt_capture_width(wc->capture_winrt);
                //int srcHeight= wc->exports.winrt_capture_height(wc->capture_winrt);
                //if (srcWidth>0 && srcHeight>0)
                //{
                //    wc->srcWidth = srcWidth;
                //    wc->srcHeight = srcHeight;
                //}
                wc->srcWidth = wc->exports.winrt_capture_width(wc->capture_winrt);
                wc->srcHeight = wc->exports.winrt_capture_height(wc->capture_winrt);
                //wc->srcWidth = 1858;
                //wc->srcHeight = 1080;
			}
		}
    }

    obs_leave_graphics();
}

static void windowSourceRender(void *data, gs_effect_t *effect)
{
    struct dyWindouSource *wc = data;
    gs_effect_t *const opaque = obs_get_base_effect(OBS_EFFECT_OPAQUE);
	if (wc->method == METHOD_WGC) 
	{
		if (wc->exports.winrt_capture_render && wc->capture_winrt)
		{
			wc->exports.winrt_capture_render(wc->capture_winrt, opaque);
		}
	}
	else
	{
		dc_capture_render(&wc->capture, opaque);
	}

    UNUSED_PARAMETER(effect);
}

extern HWND getWinHWND(void *data){
    struct dyWindouSource *wc = data;
    return wc->window;
}

extern bool hasWCSuccessed(void *data)
{
	struct dyWindouSource *wc = data;
	return wc->hasSuccessed;
}

struct obs_source_info dyWindowSourceInfo = {
    .id = "DY_Window",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
    .get_name = windowSourceGetname,
    .create = windowSourceCreate,
    .destroy = windowSourceDestroy,
    .update = windowSourceUpdate,
    .video_render = windowSourceRender,
    .hide = windowSourceHide,
    .video_tick = windowSourceTick,
    .get_width = windowSourceGetWidth,
    .get_height = windowSourceGetHeight,
    .get_defaults = windowSourceGetDefaults,
    .get_properties = windowSourceGetProperties,
    .icon_type = OBS_ICON_TYPE_WINDOW_CAPTURE,
};

UNIONPOWERTOOL_ENTITY struct obs_source_info getDyWindowSourceInfo()
{
    struct obs_source_info dyWindowSourceInfo = {
    .id = "DY_Window",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
    .get_name = windowSourceGetname,
    .create = windowSourceCreate,
    .destroy = windowSourceDestroy,
    .update = windowSourceUpdate,
    .video_render = windowSourceRender,
    .hide = windowSourceHide,
    .video_tick = windowSourceTick,
    .get_width = windowSourceGetWidth,
    .get_height = windowSourceGetHeight,
    .get_defaults = windowSourceGetDefaults,
    .get_properties = windowSourceGetProperties,
    .icon_type = OBS_ICON_TYPE_WINDOW_CAPTURE,
    };
    return dyWindowSourceInfo;
}
