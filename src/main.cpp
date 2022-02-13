extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef NDEBUG
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

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::warn);
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

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	logger::info("loaded");

	SKSE::Init(a_skse);

	auto moduleBase = REL::Module::get().base();
	auto injectionPoint = moduleBase + 0xc119d8;
	auto &trampolin = SKSE::GetTrampoline();
	trampolin.create(32, (void*)injectionPoint);

	struct OrigCode : Xbyak::CodeGenerator
	{
		OrigCode()
		{
			mov(eax, qword[rax + 0x118]);
		}
	};
	OrigCode orig;
	orig.ready();

	struct DeleteBit6 : Xbyak::CodeGenerator
	{
		DeleteBit6()
		{
			and_(cl, 0xdf); //delete bit #6
		}
	};
	DeleteBit6 db6;
	db6.ready();

	trampolin.allocate(orig);
	trampolin.allocate(db6);

	//erase the original instruction with nops
	std::vector<std::uint8_t> nops(orig.getSize());
	std::fill(nops.begin(), nops.end(), 0x90);
	REL::safe_write<std::uint8_t>(injectionPoint, nops);

	//writes:
	//jmp from injectionPoint -> start_InjectedCode
	//jmp from end_InjectedCode -> injectionPoint+org.getSize()
	trampolin.write_branch<5>(injectionPoint, injectionPoint+orig.getSize());

	return true;
}
