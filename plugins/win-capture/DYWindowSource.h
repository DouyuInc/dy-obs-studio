#pragma once

#include <obs-module.h>

#ifdef DYWindowSource
#define UNIONPOWERTOOL_ENTITY   __declspec(dllexport)
#else 
#define UNIONPOWERTOOL_ENTITY   __declspec(dllimport)
#endif

UNIONPOWERTOOL_ENTITY struct obs_source_info getDyWindowSourceInfo();

