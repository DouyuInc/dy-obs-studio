#include <windows.h>
#include <obs-module.h>
#include <util/windows/win-version.h>
#include <util/platform.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-capture", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Windows game/screen/window capture";
}

extern struct obs_source_info duplicator_capture_info;
extern struct obs_source_info monitor_capture_info;
extern struct obs_source_info window_capture_info;
extern struct obs_source_info game_capture_info;

extern struct obs_source_info dyScreenSourceInfo;
extern struct obs_source_info dyScreenShotInfo;
extern struct obs_source_info dyScreenSourceWin7Info;
extern struct obs_source_info dyScreenShotSourceWin7Info;


extern struct obs_source_info dyWindowSourceInfo;
extern struct obs_source_info dyGameSourceInfo;



static HANDLE init_hooks_thread = NULL;

extern bool cached_versions_match(void);
extern bool load_cached_graphics_offsets(bool is32bit, const char *config_path);
extern bool load_graphics_offsets(bool is32bit, bool use_hook_address_cache,
				  const char *config_path);

/* temporary, will eventually be erased once we figure out how to create both
 * 32bit and 64bit versions of the helpers/hook */
#ifdef _WIN64
#define IS32BIT false
#else
#define IS32BIT true
#endif

static const bool use_hook_address_cache = false;

static DWORD WINAPI init_hooks(LPVOID param)
{
	char *config_path = param;

	if (use_hook_address_cache && cached_versions_match() &&
	    load_cached_graphics_offsets(IS32BIT, config_path)) {

		load_cached_graphics_offsets(!IS32BIT, config_path);
		obs_register_source(&game_capture_info);
        obs_register_source(&dyGameSourceInfo);

	} else if (load_graphics_offsets(IS32BIT, use_hook_address_cache,
					 config_path)) {
		load_graphics_offsets(!IS32BIT, use_hook_address_cache,
				      config_path);
	}

	bfree(config_path);
	return 0;
}

void wait_for_hook_initialization(void)
{
	static bool initialized = false;

	if (!initialized) {
		if (init_hooks_thread) {
			WaitForSingleObject(init_hooks_thread, INFINITE);
			CloseHandle(init_hooks_thread);
			init_hooks_thread = NULL;
		}
		initialized = true;
	}
}

void init_hook_files(void);

bool graphics_uses_d3d11 = false;
bool wgc_supported = false;

bool obs_module_load(void)
{
	blog(LOG_INFO, "obs_module_load winCapture");
	struct win_version_info ver;
	bool win8_or_above = false;
	char *config_dir;

	struct win_version_info win1903 = {
		.major = 10, .minor = 0, .build = 18362, .revis = 0};

	config_dir = obs_module_config_path(NULL);
	if (config_dir) {
		os_mkdirs(config_dir);
		bfree(config_dir);
	}

	get_win_ver(&ver);

	win8_or_above = ver.major > 6 || (ver.major == 6 && ver.minor >= 2);

	obs_enter_graphics();
	graphics_uses_d3d11 = gs_get_device_type() == GS_DEVICE_DIRECT3D_11;
	obs_leave_graphics();

	if (graphics_uses_d3d11)
		wgc_supported = win_version_compare(&ver, &win1903) >= 0;

    if (win8_or_above && gs_get_device_type() == GS_DEVICE_DIRECT3D_11) {
        //obs_register_source(&duplicator_capture_info);
        obs_register_source(&dyScreenSourceInfo);
        obs_register_source(&dyScreenShotInfo);
		//obs_register_source(&dyScreenSourceWin7Info);
		//obs_register_source(&dyScreenShotSourceWin7Info);
		blog(LOG_INFO, "win8_or_above=%d", win8_or_above);
		blog(LOG_INFO, "gs_get_device_type=%d", gs_get_device_type());
		blog(LOG_INFO,"using win8_or_above Info");
		blog(LOG_INFO, "dyScreenSourceInfo=", &dyScreenSourceInfo);
    }
    else
    {
        obs_register_source(&dyScreenSourceWin7Info);
		obs_register_source(&dyScreenShotSourceWin7Info);
        //obs_register_source(&monitor_capture_info);
		blog(LOG_INFO, "using  win7 Info");
    }

	//obs_register_source(&window_capture_info);
	//obs_register_source(&dyWindowSourceInfo);

	char *config_path = obs_module_config_path(NULL);

	init_hook_files();
	init_hooks_thread =
		CreateThread(NULL, 0, init_hooks, config_path, 0, NULL);
	obs_register_source(&game_capture_info);
    obs_register_source(&dyGameSourceInfo);

	return true;
}

void obs_module_unload(void)
{
	wait_for_hook_initialization();
}
