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

void clear_file()
{
	std::ofstream file;
	file.open("Data/IntData/Events.txt", std::ofstream::out | std::ofstream::trunc);
	file.close();
}

namespace Papyrus
{

	using Args = RE::BSScript::IFunctionArguments;
	using CallbackPtr = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>;
	inline bool DispatchStaticCall(RE::BSFixedString a_class, RE::BSFixedString a_fnName, CallbackPtr a_callback, Args* a_args)
	{
		auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		return vm->DispatchStaticCall(a_class, a_fnName, a_args, a_callback);
	}

	template <class... Args>
	inline bool DispatchStaticCall(RE::BSFixedString a_class, RE::BSFixedString a_fnName, CallbackPtr a_callback,
		Args&&... a_args)
	{
		auto args = RE::MakeFunctionArguments(std::forward<Args>(a_args)...);
		return DispatchStaticCall(a_class, a_fnName, a_callback, args);
	}

	template <typename T>
	auto convert(T&& val)
	{
		if constexpr (std::is_same<T, std::string>::value) {
			return RE::BSFixedString(T);
		}

		return std::forward<T>(val);
	}

	class VmCallback : public RE::BSScript::IStackCallbackFunctor
	{
	public:
		using OnResult = std::function<void(const RE::BSScript::Variable& result)>;

		static auto New(const OnResult& onResult_)
		{
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> res;
			res.reset(new VmCallback(onResult_));
			return res;
		}

		VmCallback(const OnResult& onResult_) : onResult(onResult_) {}

	private:
		void operator()(RE::BSScript::Variable result) override
		{
			if (onResult)
				onResult(result);
		}

		bool CanSave() const override { return false; }

		void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

		const OnResult onResult;
	};

	template <typename... Args>
	void callfunc(Args&&... args)
	{
		auto onResult = [](const RE::BSScript::Variable& res) {
			int result = res.GetSInt();
			if (result)
				clear_file();
		};
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		callback.reset(new VmCallback(onResult));

		DispatchStaticCall("InteractiveScript", "maincraft", callback, convert(std::forward<Args>(args))...);
	}
}

#include "json/json.h"
namespace Json
{
	int get_mod_index(const std::string_view& name)
	{
		auto esp = RE::TESDataHandler::GetSingleton()->LookupModByName(name);
		if (!esp)
			return -1;

		return !esp->IsLight() ? esp->compileIndex << 24 : (0xFE000 | esp->smallFileCompileIndex) << 12;
	}

	uint32_t get_formid(const std::string& name)
	{
		auto pos = name.find('|');
		auto ind = get_mod_index(name.substr(0, pos));
		if (ind == -1)
			return 0;

		return ind | std::stoul(name.substr(pos + 1), nullptr, 16);
	}

	std::unordered_map<std::string, std::vector<uint32_t>> types;
	std::unordered_map<std::string, std::string> types_names;

	void read_json()
	{
		types.clear();

		Json::Value json_root;
		std::ifstream ifs;
		ifs.open("Data/IntData/FormIDs.json");
		ifs >> json_root;
		ifs.close();

		for (const auto& group_name : json_root.getMemberNames()) {
			const auto& group = json_root[group_name];
			if (group.size() < 2) {
				logger::warn("Too small array {}", group_name);
				continue;
			}

			types_names.insert({ group_name, group[0].asString() });

			auto _a = types.insert({ group_name, {} });
			auto& a = (*_a.first).second;
			for (int i = 1; i < (int)group.size(); i++) {
				auto formid = get_formid(group[i].asString());
				if (formid == 0)
					logger::warn("In group {} at position {} error", group_name, i);
				else
					a.push_back(formid);
			}
		}
	}
}
using Json::types;
using Json::types_names;
using Json::read_json;

template <typename T>
T pick_random(const std::vector<T>& a) { return a[FenixUtils::random_range(0, (int)a.size() - 1)]; }

void setLevel(RE::Actor* a, int16_t lvl)
{
	auto base = a->GetBaseObject()->As<RE::TESNPC>()->As<RE::TESActorBaseData>();
	_generic_foo_<14263, void(RE::TESActorBaseData*, int16_t lvl)>::eval(base, lvl);

	a->InitValues();
}

bool spawn(const std::string& _who, int count, const std::string& donname)
{
	bool rand_lvl = _who.ends_with("R");

	std::string who = rand_lvl ? _who.substr(0, _who.size() - 1) : _who;
	auto data = types.find(who);
	if (data == types.end())
		return false;

	auto player = RE::PlayerCharacter::GetSingleton();
	int16_t player_lvl = rand_lvl ? 0 : player->GetLevel();

	for (int i = 0; i < count; i++) {
		auto man_base = RE::TESForm::LookupByID<RE::TESNPC>(pick_random((*data).second));
		auto man = FenixUtils::placeatmepap(player, man_base, 1);
		std::string new_name = man->GetName() + std::string(" - последователь") + donname;
		man->SetDisplayName(new_name, true);
		setLevel(man->As<RE::Actor>(), rand_lvl ? static_cast<int16_t>(FenixUtils::random_range(1, 255)) : player_lvl);
	}

	FenixUtils::notification("%s вызвал на бой с вами %d шт. %s", donname.c_str(), count, (*types_names.find(who)).second.c_str());

	return true;
}

bool addSeptims(int count)
{
	static auto septim = RE::TESForm::LookupByID<RE::TESObjectMISC>(0xf);

	FenixUtils::AddItemPlayer(septim, count);
	return true;
}

bool removeSeptims(int count)
{
	static auto septim = RE::TESForm::LookupByID<RE::TESObjectMISC>(0xf);

	auto player = RE::PlayerCharacter::GetSingleton();
	if (player->GetItemCount(septim) < count)
		return false;

	player->RemoveItem(septim, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
	return true;
}

auto get_inventory() {
	auto player = RE::PlayerCharacter::GetSingleton();
	auto container = player->GetContainer();
	auto changes = player->GetInventoryChanges();

	std::set<RE::TESBoundObject*> ans;
	container->ForEachContainerObject([&ans, player](RE::ContainerObject& item) {
		if (player->GetItemCount(item.obj) > 0)
			ans.insert(item.obj);
		return RE::BSContainer::ForEachResult::kContinue;
	});

	const auto& list = changes->entryList;
	for (auto it = list->begin(); it != list->end(); ++it) {
		auto item = (*it)->GetObject();
		if (ans.find(item) != ans.end()) {
			if ((*it)->IsQuestObject()) {
				ans.erase(item);
			}
		} else {
			if (player->GetItemCount(item) > 0 && !(*it)->IsQuestObject())
				ans.insert(item);
		}
	}

	return std::vector(ans.begin(), ans.end());
}

bool addOrRemoveRndItem(int count)
{
	for (int i = 0; i < count; i++) {
		auto inv = get_inventory();
		if (inv.empty())
			return i > 0;

		auto to_remove = pick_random(inv);
		FenixUtils::RemoveItemPlayer(to_remove, 1);
		_generic_foo_<50741, void(RE::TESBoundObject * a1, uint32_t count, char a3, char a4, char* name)>::eval(to_remove, 1, 0, 1, nullptr);
	}
	return true;
}

static const std::array<std::function<bool(int)>, 3> item_funcs = { addSeptims, removeSeptims, addOrRemoveRndItem };

#include "SimpleIni.h"
class Settings
{
	static constexpr auto ini_path = "Data/skse/plugins/Interactive.ini"sv;

	static bool ReadBool(const CSimpleIniA& ini, const char* section, const char* setting, bool& ans)
	{
		if (ini.GetValue(section, setting)) {
			ans = ini.GetBoolValue(section, setting);
			return true;
		}
		return false;
	}

	static bool ReadFloat(const CSimpleIniA& ini, const char* section, const char* setting, float& ans)
	{
		if (ini.GetValue(section, setting)) {
			ans = static_cast<float>(ini.GetDoubleValue(section, setting));
			return true;
		}
		return false;
	}

	static bool ReadUint32(const CSimpleIniA& ini, const char* section, const char* setting, uint32_t& ans)
	{
		if (ini.GetValue(section, setting)) {
			ans = static_cast<uint32_t>(ini.GetLongValue(section, setting));
			return true;
		}
		return false;
	}

public:
	static inline uint32_t PR[3];

	static void ReadSettings()
	{
		CSimpleIniA ini;
		ini.LoadFile(ini_path.data());

		for (int i = 0; i < item_funcs.size(); i++) {
			ReadUint32(ini, "Items", (std::string("iPR") + std::to_string(i + 1)).c_str(), PR[i]);
		}
	}
};

bool items(const std::string& what, int count)
{
	int id = -1;
	std::stringstream ss(what);
	ss >> id;

	if (ss.fail()) {
		logger::warn("Bad command {}", what);
		return false;
	}

	--id;
	if (id >= 0 && id < item_funcs.size()) {
		return item_funcs[id](count * (Settings::PR[id]));
	} else {
		return false;
	}
}

bool read_command(const std::string& command, std::ifstream& file) {
	if (command.starts_with("SM") || command.starts_with("SL")) {
		int count;
		std::string donname;
		file >> count >> donname;
		file.close();
		return spawn(command, count, donname);
	}

	if (command.starts_with("PR")) {
		int count;
		std::string donname;
		file >> count >> donname;
		file.close();
		return items(command.substr(2), count);
	}

	return false;
}

void check_file() {
	std::ifstream file;
	file.open("Data/IntData/Events.txt", std::ofstream::in);
	if (file.peek() == std::ifstream::traits_type::eof())
		return;

	std::string event_name;
	file >> event_name;
	if (read_command(event_name, file))
		return clear_file();

	logger::warn("Unknown command {}", event_name);
}

class timer
{
	float time = 0;
	const float interval;
	const std::function<void()> callback;

public:
	void tick(float dtime)
	{
		time += dtime;
		if (time > interval) {
			time -= interval;
			callback();
		}
	}

	timer(float interval, std::function<void()> callback) : time(0), interval(interval), callback(callback) {}
} timer{ 1, []() { check_file(); } };

class UpdateHook
{
public:
	static void Hook() { _Update = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_PlayerCharacter[0])).write_vfunc(0xad, Update); }

private:
	static void Update(RE::PlayerCharacter* a, float delta)
	{
		_Update(a, delta);
		timer.tick(delta);
	}

	static inline REL::Relocation<decltype(Update)> _Update;
};

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		Settings::ReadSettings();
		read_json();
		UpdateHook::Hook();

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

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
