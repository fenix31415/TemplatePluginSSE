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

#ifdef WITH_IMGUI
namespace Gui
{
	namespace Impl
	{
		const uint32_t enable_hotkey = 199;  // home
		const uint32_t hide_hotkey = 207;    // end

		bool is_hide_hotkey(RE::ButtonEvent* b) { return b->GetIDCode() == hide_hotkey; }
		bool is_enable_hotkey(RE::ButtonEvent* b) { return b->GetIDCode() == enable_hotkey; }

		void show() { ImGui::ShowDemoWindow(); }
	}

	void init()
	{
		using ImGuiHelper = ImguiUtils::ImGuiHelper<Impl::show, Impl::is_hide_hotkey, Impl::is_enable_hotkey>;

		ImGuiHelper::Initialize();
	}
}
#endif  // WITH_IMGUI

#ifdef WITH_DRAWING
class DrawThingsHook
{
public:
	static void Hook()
	{
		_UpdatePlayer = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_PlayerCharacter[0])).write_vfunc(0xad, UpdatePlayer);
		_UpdateCharacter = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_Character[0])).write_vfunc(0xad, UpdateCharacter);
	}

private:
	static void Draw([[maybe_unused]] RE::Actor* a, [[maybe_unused]] float delta) {}

	static void UpdatePlayer(RE::PlayerCharacter* a, float delta)
	{
		draw_line0(a->GetPosition(), a->GetPosition() + RE::NiPoint3{ 0, 200, 0 });

		_UpdatePlayer(a, delta);
		Draw(a, delta);
	}

	static void UpdateCharacter(RE::Character* a, float delta)
	{
		_UpdateCharacter(a, delta);
		Draw(a, delta);
	}

	static inline REL::Relocation<decltype(UpdatePlayer)> _UpdatePlayer;
	static inline REL::Relocation<decltype(UpdateCharacter)> _UpdateCharacter;
};
#endif  // WITH_DRAWING

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
#ifdef WITH_DRAWING
	DebugRenderUtils::OnMessage(message);
#endif  // WITH_DRAWING

	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		//

#ifdef WITH_DRAWING
		DrawThingsHook::Hook();
#endif  // WITH_DRAWING
#ifdef WITH_IMGUI
		Gui::init();
#endif  // WITH_IMGUI

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

#ifdef WITH_DRAWING
	DebugRenderUtils::UpdateHooks::Hook();
#endif  // WITH_DRAWING

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
