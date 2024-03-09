extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef DEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef DEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

namespace Gui
{
	bool enable = false;
	bool hidden = true;

	void flip_enable()
	{
		enable = !enable;
		ImGui::GetIO().MouseDrawCursor = enable && !hidden;
	}

	void flip_hidden()
	{
		hidden = !hidden;
		ImGui::GetIO().MouseDrawCursor = enable && !hidden;
	}

	const uint32_t enable_hotkey = 199;  // home
	const uint32_t hide_hotkey = 207;    // end

	void Process(const RE::ButtonEvent* button)
	{
		if (button->IsPressed() && button->IsDown()) {
			if (button->GetIDCode() == enable_hotkey) {
				flip_enable();
			}
			if (button->GetIDCode() == hide_hotkey) {
				flip_hidden();
			}
		}
	}

	void show()
	{
		if (!hidden) {
			ImGui::ShowDemoWindow();
		}
	}

	bool skipevents() { return enable && !hidden; }

	using ImGuiHook = ImguiUtils::ImGuiHooks<Process, show, skipevents>;
}

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	DebugRender::OnMessage(message);

	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		//

		Gui::ImGuiHook::Initialize();

		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}

	logger::info("loaded");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	DebugRender::UpdateHooks::Hook();

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
