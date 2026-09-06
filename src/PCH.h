#pragma once

#define f314_DEBUG

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include "spdlog/logger.h"
#include "spdlog/spdlog.h"

#pragma warning(push)
#ifdef f314_DEBUG
#	include <spdlog/sinks/basic_file_sink.h>
#else
#	include <spdlog/sinks/msvc_sink.h>
#endif
#pragma warning(pop)

using namespace std::literals;

#define DLLEXPORT __declspec(dllexport)

#include "Version.h"
#ifdef USELESS_UTILS
#	include "UselessFenixUtils.h"
#endif
