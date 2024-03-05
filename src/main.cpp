// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <dlh/container/initializer_list.hpp>
#include <dlh/container/vector.hpp>
#include <dlh/stream/output.hpp>
#include <dlh/parser/arguments.hpp>
#include <dlh/parser/config.hpp>
#include <dlh/auxiliary.hpp>
#include <dlh/macro.hpp>
#include <dlh/file.hpp>
#include <dlh/log.hpp>

#include <elfo/elf.hpp>

#include "object/base.hpp"
#include "build_info.hpp"
#include "loader.hpp"

#ifndef LUCIDIR
#define LUCIDIR "/opt/luci/"
#endif
#ifndef LIBPATH_CONF
#warning LIBPATH_CONF is not set
#define LIBPATH_CONF LUCIDIR "libpath.conf"
#endif
#ifndef LDLUCI_CONF
#warning LDLUCI_CONF is not set
#define LDLUCI_CONF LUCIDIR "ld-luci.conf"
#endif
#ifndef SOPATH
#warning SOPATH is not set
#define SOPATH LUCIDIR "ld-luci.so"
#endif


// Parse Arguments
struct Opts {
	int loglevel{Log::WARNING};
	int relaxPatchCheck{0};
	int updateMode{Loader::Config::UPDATE_MODE_GOT};
	int trapMode{Redirect::MODE_BREAKPOINT_TRAP};
	const char * logfile{};
	Vector<const char *> libpath{};
	Vector<const char *> libload{};
	Vector<const char *> libexclude{};
	Vector<const char *> preload{};
	const char * libpathconf{ STR(LIBPATH_CONF) };
	const char * luciconf{ STR(LDLUCI_CONF) };
	const char * argv0{ nullptr };
	const char * entry{ nullptr };
	const char * debughash { nullptr };
	const char * statusinfo{ nullptr };
	const char * detectOutdated{ nullptr };
	const char * debugSymbolsRoot{ nullptr };
	unsigned delayOutdated{1};
	bool pie{};
	bool noPie{};
	bool logtimeAbs{};
	bool logfileAppend{};
	bool dynamicUpdate{};
	bool dynamicDlUpdate{};
	bool dependencyCheck{};
	bool modificationTime{};
	bool forceUpdate{};
	bool stopOnUpdate{};
	bool skipIdentical{};
	bool debugSymbols{};
	bool dynamicWeak{};
	bool relocateCheck{};
	bool relocateOutdated{};
	bool earlyStatusInfo{};
	bool bindNow{};
	bool bindNot{};
	bool tracing{};
	bool linkstatic{};
	bool showVersion{};
	bool showArgs{};
	bool showAuxv{};
	bool showEnv{};
	bool showHelp{};
	bool debug{};
};

// Symbols defined by the linker
extern uintptr_t _DYNAMIC[];
extern const uintptr_t _GLOBAL_OFFSET_TABLE_[];

// Get ELF base from program header (Hack)
static uintptr_t base_from_phdr(void * phdr_ptr, long int entries = 1) {
	Elf::Phdr * phdr = reinterpret_cast<Elf::Phdr *>(phdr_ptr);
	uintptr_t addr = reinterpret_cast<uintptr_t>(phdr_ptr);
	bool valid = false;
	uintptr_t base = phdr[0].p_vaddr;
	for (long int i = 0; i < entries; i++)
		switch(phdr[i].p_type) {
			case Elf::PT_PHDR:
				return addr - phdr[i].p_offset;
			case Elf::PT_LOAD:
				if (addr >= phdr[i].p_vaddr && addr <= phdr[i].p_vaddr + phdr[i].p_memsz)
					valid = true;
				if (base > phdr[i].p_vaddr)
					base = phdr[i].p_vaddr;
			default:
				continue;
		}
	(void) valid;
	assert(valid);
	return base;
}


// Helper to prevent multiple entries in list (slow, but its academic work)
static void vector_append_unique(Vector<const char *> &dest, const char * src) {
	bool in_dest = false;
	for (auto & d : dest)
		if (String::compare(src, d) == 0) {
			in_dest = true;
			break;
		}
	if (!in_dest)
		dest.push_back(src);
}

static void vector_append_unique(Vector<const char *> &dest, Vector<const char *> &&src) {
	for (auto & s : src)
		vector_append_unique(dest, s);
}


// Setup commands
static Loader * setup(uintptr_t luci_base, const char * luci_path, struct Opts & opts, Vector<const char *> & preload) {
	// Use config from environment vars and files
	Parser::Config config_file(opts.luciconf, Parser::Config::ENV_CONFIG, true);

	// Logger
	auto cfg_loglevel = config_file.value_as<int>("LD_LOGLEVEL");
	auto cfg_logtime_abs = opts.logtimeAbs || config_file.value_or_default<bool>("LD_LOGTIME_ABS", false);

	LOG.set(static_cast<Log::Level>(cfg_loglevel && cfg_loglevel.value() > opts.loglevel ? cfg_loglevel.value() : opts.loglevel), cfg_logtime_abs ? Log::Time::ABSOLUTE : Log::Time::DELTA);
	BuildInfo::log();  // should be first output
	LOG_DEBUG << "Set log level to " << static_cast<int>(LOG.get()) << endl;

	const char * logfile = opts.logfile == nullptr ? config_file.value("LD_LOGFILE") : opts.logfile;
	if (logfile != nullptr) {
		auto cfg_logfile_append = config_file.value_as<bool>("LD_LOGFILE_APPEND");
		bool truncate = !(opts.logfileAppend || (cfg_logfile_append && cfg_logfile_append.value()));
		LOG_DEBUG << "Log to file " << logfile << (truncate ? " (truncating contents)" : " (appending output)") << endl;
		LOG.output(logfile, truncate);
	}

	// Tracing
	auto cfg_tracing = config_file.value_as<bool>("LD_TRACING");
	if (opts.tracing || (cfg_tracing && cfg_tracing.value())) {
		LOG_ERROR << "Tracing not implemented yet!" << endl;
	}

	// New Loader
	if (luci_path == nullptr || luci_path[0] == '\0') {
		luci_path = config_file.value("LD_PATH");
		if (luci_path == nullptr || luci_path[0] == '\0')
			luci_path = STR(SOPATH);
	}

	Loader::Config config_loader;
	// Position dependent
	if (opts.noPie && !opts.linkstatic) {
		LOG_WARNING << "Position dependent code is only possible when using Luci as static linker!" << endl;
	} else if (opts.pie && !opts.linkstatic) {
		LOG_WARNING << "Position independent code is only possible when using Luci as static linker!" << endl;
	} else if (opts.pie && opts.noPie) {
		LOG_ERROR << "Please select either position independent code (--pie) or position dependent code (--no-pie), not both (ignoring)!" << endl;
	} else if (opts.noPie) {
		config_loader.position_independent = false;
	} else if (opts.pie) {
		config_loader.position_independent = true;
	}
	// Debug
	config_loader.debugger = opts.debug || config_file.value_or_default<bool>("LD_DEBUG", false);
	// Dynamic updates
	config_loader.dynamic_update = opts.dynamicUpdate || config_file.value_or_default<bool>("LD_DYNAMIC_UPDATE", false);
	// Dynamic updates of dl-funcs
	if (config_loader.dynamic_update)
		config_loader.dynamic_dlupdate = opts.dynamicDlUpdate || config_file.value_or_default<bool>("LD_DYNAMIC_DLUPDATE", false);
	// Dynamic updates of dl-funcs
	if (config_loader.dynamic_update)
		config_loader.dependency_check = opts.dependencyCheck || config_file.value_or_default<bool>("LD_DEPENDENCY_CHECK", false);
	// Force dynamic updates
	if (config_loader.dynamic_update)
		config_loader.force_update = opts.forceUpdate || config_file.value_or_default<bool>("LD_FORCE_UPDATE", false);
	// Stop process during relocation at updates
	if (config_loader.dynamic_update)
		config_loader.stop_on_update = opts.stopOnUpdate || config_file.value_or_default<bool>("LD_STOP_ON_UPDATE", false);
	// Ignore identical updates
	if (config_loader.skip_identical)
		config_loader.skip_identical = opts.skipIdentical || config_file.value_or_default<bool>("LD_SKIP_IDENTICAL", false);
	// Use modification time to detect changes
	config_loader.use_mtime = opts.modificationTime || config_file.value_or_default<bool>("LD_USE_MTIME", false);
	// Weak linking
	config_loader.dynamic_weak = opts.dynamicWeak ||  config_file.value_or_default<bool>("LD_DYNAMIC_WEAK", false);
	// Check relocation targets for modifications before updating
	if (config_loader.dynamic_update)
		config_loader.check_relocation_content = opts.relocateCheck || config_file.value_or_default<bool>("LD_RELOCATE_CHECK", false);
	// Fix relocations in outdated varsions
	if (config_loader.dynamic_update)
		config_loader.update_outdated_relocations = (opts.relocateOutdated || config_file.value_or_default<bool>("LD_RELOCATE_OUTDATED", false));
	// Early Status Info output
	config_loader.early_statusinfo = opts.earlyStatusInfo ||  config_file.value_or_default<bool>("LD_EARLY_STATUS_INFO", false);
	// Process init debug output
	config_loader.show_args = opts.showArgs || config_file.value_or_default<bool>("LD_SHOW_ARGS", false);
	config_loader.show_auxv = opts.showAuxv || config_file.value_or_default<bool>("LD_SHOW_AUXV", false);
	config_loader.show_env = opts.showEnv ||  config_file.value_or_default<bool>("LD_SHOW_ENV", false);

	// Debug symbols
	if (config_loader.dynamic_update)
		config_loader.find_debug_symbols = opts.debugSymbols || config_file.value_or_default<bool>("LD_DEBUG_SYMBOLS", false);
	if (config_loader.find_debug_symbols) {
		config_loader.debug_symbols_root = opts.debugSymbolsRoot != nullptr && String::len(opts.debugSymbolsRoot) > 0 ? opts.debugSymbolsRoot : config_file.value_or_default<const char *>("LD_DEBUG_SYMBOLS_ROOT", nullptr);
		LOG_DEBUG << "Set root for debug symbols to " << config_loader.debug_symbols_root << endl;
	}

	// Detect access of outdated varsions
	if (opts.detectOutdated == nullptr || String::len(opts.detectOutdated) == 0)
		opts.detectOutdated = config_file.value_or_default<const char *>("LD_DETECT_OUTDATED", "disabled");
	if (!config_loader.dynamic_update || opts.detectOutdated == nullptr || String::len(opts.detectOutdated) == 0 || String::compare_case(opts.detectOutdated, "disabled") == 0) {
		config_loader.detect_outdated = Loader::Config::DETECT_OUTDATED_DISABLED;
	} else if (String::compare_case(opts.detectOutdated, "userfaultfd") == 0) {
		config_loader.detect_outdated = Loader::Config::DETECT_OUTDATED_VIA_USERFAULTFD;
		LOG_DEBUG << "Detecting outdated access via userfaultfd" << endl;
	} else if (String::compare_case(opts.detectOutdated, "uprobes", 7) == 0) {
		if (String::compare_case(opts.detectOutdated, "uprobes") == 0) {
			config_loader.detect_outdated = Loader::Config::DETECT_OUTDATED_VIA_UPROBES;
			LOG_DEBUG << "Detecting outdated access via uprobes" << endl;
		} else if (String::compare_case(opts.detectOutdated, "uprobes_deps") == 0) {
			config_loader.detect_outdated = Loader::Config::DETECT_OUTDATED_WITH_DEPS_VIA_UPROBES;
			LOG_DEBUG << "Detecting outdated access (inc. dependencies) via uprobes" << endl;
		} else {
			config_loader.detect_outdated = Loader::Config::DETECT_OUTDATED_VIA_UPROBES;
			LOG_ERROR << "Invalid mode '" << opts.detectOutdated << "' for uprobe to detect outdated -- will use default uprobe" << endl;
		}
		if (File::contents::set("/sys/kernel/debug/tracing/events/uprobes/enable", "0") != 0)
			File::contents::set("/sys/kernel/debug/tracing/uprobe_events", nullptr);
	} else {
		LOG_ERROR << "Yet unsupported mode to detect outdated: " << opts.detectOutdated << endl;
	}
	// Delay before detecting access of outdated varsions
	if (config_loader.detect_outdated != Loader::Config::DETECT_OUTDATED_DISABLED) {
		config_loader.detect_outdated_delay = Math::max(opts.delayOutdated, config_file.value_or_default<unsigned>("LD_DETECT_OUTDATED_DELAY", config_loader.detect_outdated_delay));
		LOG_DEBUG << "Delay for detecting outdated access is " << config_loader.detect_outdated_delay << "s" << endl;
	}

	// Set update mode for patchability
	config_loader.update_mode = static_cast<Loader::Config::UpdateMode>(Math::max(opts.updateMode, config_file.value_or_default<int>("LD_UPDATE_MODE", config_loader.update_mode)));
	if (config_loader.dynamic_update)
		LOG_DEBUG << "Using update mode " << config_loader.update_mode << endl;

	config_loader.trap_mode = static_cast<Redirect::Mode>(Math::max(opts.trapMode, config_file.value_or_default<int>("LD_TRAP_MODE", config_loader.trap_mode)));
	if (config_loader.dynamic_update)
		LOG_DEBUG << "Using trap mode " << config_loader.trap_mode << endl;

	// Set comparison mode for patchability checks
	config_loader.relax_comparison = Math::max(opts.relaxPatchCheck, config_file.value_or_default<int>("LD_RELAX_CHECK", config_loader.relax_comparison));
	if (config_loader.relax_comparison > 0)
		LOG_DEBUG << "Relaxing comparison of patchability to mode " << config_loader.relax_comparison << endl;

	Loader * loader = new Loader(luci_base,	luci_path, config_loader);
	if (loader == nullptr) {
		LOG_ERROR << "Unable to allocate loader" << endl;
	} else {
		if (loader->config.dynamic_update) {
			LOG_INFO << "Dynamic updates are enabled!" << endl;
		} else {
			LOG_DEBUG << "Dynamic updates are disabled!" << endl;
		}
		if (loader->config.dependency_check) {
			LOG_INFO << "Recursively comparing function dependencies on updates!" << endl;
		}

		if (loader->config.dynamic_weak) {
			LOG_INFO << "Weak references in shared library are supported (nonstandard)!" << endl;
		}

		// Debug Hash
		const char * debughash_uri = opts.debughash;
		if (debughash_uri == nullptr) {
			debughash_uri = config_file.value("LD_DEBUG_HASH");
		}
		if (debughash_uri != nullptr && String::len(debughash_uri) > 0) {
			if (loader->debug_hash_socket.connect(debughash_uri)) {
				LOG_DEBUG << "Using URI " << debughash_uri << " for debug (DWARF) hashing" << endl;
			} else {
				LOG_ERROR << "Debug hashing not available (invalid URI " << debughash_uri << ")" << endl;
			}
		}

		// Status info
		const char * statusinfo = opts.statusinfo;
		if (statusinfo == nullptr) {
			statusinfo = config_file.value("LD_STATUS_INFO");
		}
		if (statusinfo != nullptr) {
			if (auto open = Syscall::open(statusinfo, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0644)) {
				LOG_DEBUG << "Writing status info to " << statusinfo << endl;
				loader->statusinfofd = open.value();
			} else {
				LOG_INFO << "Opening '" << statusinfo << "' for status info failed: " << open.error_message() << endl;
			}
		}

		// Flags
		loader->default_flags.bind_now = opts.bindNow || (config_file.value("LD_BIND_NOW") != nullptr);
		loader->default_flags.bind_not = opts.bindNot || (config_file.value("LD_BIND_NOT") != nullptr);

		// Library search path
		for (auto & libpath : opts.libpath) {
			LOG_DEBUG << "Add '" << libpath << "' (from --library-path) to library search path..." << endl;
			vector_append_unique(loader->library_path_runtime, libpath);
		}

		char * ld_library_path = const_cast<char*>(config_file.value("LD_LIBRARY_PATH"));
		if (ld_library_path != nullptr && *ld_library_path != '\0') {
			LOG_DEBUG << "Add '" << ld_library_path<< "' (from LD_LIBRARY_PATH) to library search path..." << endl;
			vector_append_unique(loader->library_path_runtime, String::split_inplace(ld_library_path, ';'));
		}

		// Library search path config
		if (File::readable(opts.libpathconf)) {
			LOG_DEBUG << "Adding contents of '" << opts.libpathconf << "' to library search path..." << endl;
			vector_append_unique(loader->library_path_config, File::lines(opts.libpathconf));
		}
		const char * ld_library_conf = config_file.value("LD_LIBRARY_CONF");
		if (ld_library_conf != nullptr && *ld_library_conf != '\0' && File::readable(ld_library_conf)) {
			LOG_DEBUG << "Adding contents of '" << ld_library_conf << "' (from LD_LIBRARY_CONF) to library search path..." << endl;
			vector_append_unique(loader->library_path_config, File::lines(ld_library_conf));
		}
		const auto libpathconf_size = loader->library_path_config.size();
		if (libpathconf_size == 0) {
			LOG_WARNING << "No library search path entries form library config!" << endl;
		} else {
			LOG_DEBUG << "Library config has " << loader->library_path_config.size() << " search path entries!" << endl;
		}

		// Exclude Library
		for (const char * lib : opts.libexclude)
			loader->library_exclude.insert(lib);

		char * exclude_libs = const_cast<char*>(config_file.value("LD_EXCLUDE"));
		if (exclude_libs != nullptr && *exclude_libs != '\0') {
			for (const char * lib : String::split_any_inplace(exclude_libs, ";:"))
				loader->library_exclude.insert(lib);
		}

		// Preload Library
		for (auto & lib : opts.preload)
			preload.push_back(lib);

		char * preload_libs = const_cast<char*>(config_file.value("LD_PRELOAD"));
		if (preload_libs != nullptr && *preload_libs != '\0') {
			for (auto & lib : String::split_any_inplace(preload_libs, ";:"))
				preload.push_back(lib);
		}
	}

	return loader;
}

enum LibName {
	LIB_NAME_COLON,
	LIB_NAME_SHARED,
	LIB_NAME_STATIC,
};
static bool load_library_by_name(Loader * loader, const char * lib, enum LibName libname) {
	// Try as namespec
	StringStream<255> filename;
	switch (libname) {
		case LIB_NAME_COLON:
			filename << lib + 1;
			break;
		case LIB_NAME_SHARED:
			filename << "lib" << lib << ".so";
			// Special case: common shared libc / libm / libstdc++ are suffixed by '.6'
			if (((lib[0] == 'c' || lib[0] == 'm') && lib[1] == '\0') || String::compare(lib, "stdc++") == 0)
				filename << ".6";
			break;
		case LIB_NAME_STATIC:
			filename << "lib" << lib << ".a";
			break;
	}
	if (ObjectIdentity * loaded = loader->library(filename.str(), loader->default_flags)) {
		LOG_INFO << "Loaded " << lib << ": " << *loaded << endl;
		return true;
	} else {
		return false;
	}
}

int main(int argc, char* argv[]) {
	// We do no (implicit) self relocation, hence make sure it is already correct
	assert(reinterpret_cast<uintptr_t>(&_DYNAMIC) - _GLOBAL_OFFSET_TABLE_[0] == 0);

	assert(Auxiliary::vector(Auxiliary::AT_PHENT).value() == sizeof(Elf::Phdr));
	uintptr_t base = base_from_phdr(Auxiliary::vector(Auxiliary::AT_PHDR).pointer(), Auxiliary::vector(Auxiliary::AT_PHNUM).value());
	Vector<const char *> preload;

	// Luci explicitly started from command line?
	if (base == BASEADDRESS) {
		// Available commandline options
		auto args = Parser::Arguments<Opts>({
				/* short & long name,      argument, element               required, help text,  optional validation function */
				{'v',  "verbosity",        "LEVEL",  &Opts::loglevel,         false, "Set log level (0 = none, 3 = warning, 6 = debug). This can also be done using the environment variable LD_LOGLEVEL.", [](const char * str) -> bool { int l = 0; return Parser::string(l, str) ? l >= Log::NONE && l <= Log::TRACE : false; }},  // NOLINT
				{'A',  "logtimeabs",       nullptr,  &Opts::logtimeAbs,       false, "Use absolute time (instead of delta) in log. This option can also be enabled by setting the environment variable LD_LOGTIME_ABS to 1"},
				{'f',  "logfile",          "FILE",   &Opts::logfile,          false, "Log to the given file. This can also be specified using the environment variable LD_LOGFILE" },
				{'a',  "logfile-append",   nullptr,  &Opts::logfileAppend,    false, "Append output to log file (instead of truncate). Requires logfile, can also be enabled by setting the environment variable LD_LOGFILE_APPEND to 1" },
				{'e',  "entry",            "SYM",    &Opts::entry,            false, "Overwrite default start entry point with custom symbol or address (use preceeding '+' for relative adressing)" },
				{'l',  "library",          "NAME",   &Opts::libload,          false, "Add library namespec to link/load (in the given order). Prefix with ':' for filename" },
				{'x',  "exclude",          "FILE",   &Opts::libexclude,       false, "Skip and exclude library, even when required as dependency. By default, this is used for the systems default RTLD 'ld-linux-x86-64.so.2' and 'libdl'. This can also be specified using the environment variable LD_EXCLUDE - separate mutliple files by semicolon." },
				{'L',  "library-path",     "DIR",    &Opts::libpath,          false, "Add library search path (this parameter may be used multiple times to specify additional directories). This can also be specified with the environment variable LD_LIBRARY_PATH - separate mutliple directories by semicolon." },
				{'c',  "library-conf",     "FILE",   &Opts::libpathconf,      false, "Library path configuration" },
				{'C',  "luci-conf",        "FILE",   &Opts::luciconf,         false, "Luci loader configuration file" },
				{'P',  "preload",          "FILE",   &Opts::preload,          false, "Library to be loaded first (this parameter may be used multiple times to specify addtional libraries). This can also be specified with the environment variable LD_PRELOAD - separate mutliple files by semicolon." },
				{'S',  "statusinfo",       "FILE",   &Opts::statusinfo,       false, "File (named pipe) for logging successful and failed updates (latter would require a restart). Disabled if empty. This option can also be activated by setting the environment variable LD_STATUS_INFO" },
				{'d',  "debughash",        "SOCKET", &Opts::debughash,        false, "Socket URI (unix / tcp / udp) for retrieving debug data hashes. Disabled if empty. This option can also be activated by setting the environment variable LD_DEBUG_HASH" },
				{'u',  "update",           nullptr,  &Opts::dynamicUpdate,    false, "Enable dynamic updates. This option can also be enabled by setting the environment variable LD_DYNAMIC_UPDATE to 1" },
				{'U',  "dlupdate",         nullptr,  &Opts::dynamicDlUpdate,  false, "Enable updates of functions loaded using the DL interface -- only available if dynamic updates are enabled. This option can also be enabled by setting the environment variable LD_DYNAMIC_DLUPDATE to 1" },
				{'M',  "update-mode",      "MODE",   &Opts::updateMode,       false, "Set mode for dynamic update. This can also be done using the environment variable LD_UPDATE_MODE." },
				{'D',  "func-dep-check",   nullptr,  &Opts::dependencyCheck,  false, "Check (recursively) all dependencies of each function for patchability -- only available if dynamic updates are enabled. This option can also be enabled by setting the environment variable LD_DEPENDENCY_CHECK to 1" },
				{'i',  "relax-check",      "MODE",   &Opts::relaxPatchCheck,  false, "Relax binary comparison check (0 will use an extended ID check [default], 1 will releax check for writeable sections, 2 for all sections except executable, 3 will only use internal ID for comparison. This can also be done using the environment variable LD_RELAX_CHECK." },
				{'t',  "trap",             "MODE",   &Opts::trapMode,         false, "Set redirection trap mode. This option can also be enabled by setting the environment variable LD_TRAP_MODE" },
				{'F',  "force",            nullptr,  &Opts::forceUpdate,      false, "Force dynamic update of changed files, even if they seem to be incompatible -- only available if dynamic updates are enabled. This option can also be enabled by setting the environment variable LD_FORCE_UPDATE to 1" },
				{'I',  "skip-identical",   nullptr,  &Opts::skipIdentical,    false, "Do not apply updates if they are identical to a previously loaded version -- only available if dynamic updates are enabled. This option can also be enabled by setting the environment variable LD_SKIP_IDENTICAL to 1" },
				{'m',  "mtime",            nullptr,  &Opts::modificationTime, false, "Consider modification time when detecting identical updates libraries -- only available if identical updates are skipped. This option can also be enabled by setting the delay in environment variable LD_USE_MTIME."},
				{'T',  "tracing",          nullptr,  &Opts::tracing,          false, "Enable tracing (using ptrace) during dynamic updates to detect access of outdated functions. This option can also be enabled by setting the environment variable LD_TRACING to 1" },
				{'r',  "reloc-check",      nullptr,  &Opts::relocateCheck,    false, "Check if contents of relocation targets in data section have been altered during execution by the user. This option can also be enabled by setting the environment variable LD_RELOCATE_CHECK to 1"},
				{'R',  "reloc-outdated",   nullptr,  &Opts::relocateOutdated, false, "Fix relocations of outdated versions as well. This option can also be enabled by setting the environment variable LD_RELOCATE_OUTDATED to 1"},
				{'o',  "detect-outdated",  "MODE",   &Opts::detectOutdated,   false, "Detect access in old versions, allowed values are 'disabled' (default), 'userfaultfd', 'uprobes', 'uprobes_deps' and 'ptrace'. This option can also be enabled by setting the in environment variable LD_DETECT_OUTDATED."},
				{'O',  "delay-outdated",   "DELAY",  &Opts::delayOutdated,    false, "Delay the installation for outdated access -- default is 1 second. This option can also be set using the environment variable LD_DETECT_OUTDATED_DELAY."},
				{'w',  "weak",             nullptr,  &Opts::dynamicWeak,      false, "Enable weak symbol references in dynamic files (nonstandard!). This option can also be enabled by setting the environment variable LD_DYNAMIC_WEAK to 1" },
				{'n',  "bind-now",         nullptr,  &Opts::bindNow,          false, "Resolve all symbols at program start (instead of lazy resolution). This option can also be enabled by setting the environment variable LD_BIND_NOW to 1" },
				{'N',  "bind-not",         nullptr,  &Opts::bindNot,          false, "Do not update GOT after resolving a symbol. This option cannot be used in conjunction with bind-now. It can be enabled by setting the environment variable LD_BIND_NOT to 1" },
				{'V',  "version",          nullptr,  &Opts::showVersion,      false, "Show version information" },
				{'s',  "static",           nullptr,  &Opts::linkstatic,       false, "Act as static linker as well - this mode allows binding and loading relocatable object files (.o)." },
				{'\0', "stop-on-update",   nullptr,  &Opts::stopOnUpdate,     false, "Stop the process during update according to Intels requirements for cross processor code modification. Make sure to disable job control. This option can also be enabled by setting the environment variable LD_STOP_ON_UPDATE to 1" },
				{'\0', "early-statusinfo", nullptr,  &Opts::earlyStatusInfo,  false, "Output status info during loading the binary, so that it will also contain details about the initial libraries. This option can also be enabled by setting the environment variable LD_EARLY_STATUS_INFO to 1" },
				{'\0', "dbgsym",           nullptr,  &Opts::debugSymbols,     false, "Search for external debug symbols to improve detection of binary updatability. This option can also be enabled by setting the environment variable LD_DEBUG_SYMBOLS to 1" },
				{'\0', "dbgsym-root",      nullptr,  &Opts::debugSymbolsRoot, false, "Set root directory for external debug symbols. This option can also be configured using the environment variable LD_DEBUG_SYMBOLS_ROOT" },
				{'\0', "argv0",            nullptr,  &Opts::argv0,            false, "Explicitly specify program name (argv[0])" },
				{'\0', "pie",              nullptr,  &Opts::pie,              false, "Use position anywhere in memory for static linker - recommended if relocatable objects are compiled with position independent code. Default for Debian-like distributions. Cannot be used together with --no-pie" },
				{'\0', "no-pie",           nullptr,  &Opts::noPie,            false, "Use position in lower 2 GB region for static linker - required if relocatable objects are not compiled with position independent code. Default for RedHat-like distributions. Cannot be used together with --pie" },
				{'\0', "show-args",        nullptr,  &Opts::showArgs,         false, "Show the arguments passed to the process (on standard error). It can be enabled by setting the environment variable LD_SHOW_ARGS to 1" },
				{'\0', "show-auxv",        nullptr,  &Opts::showAuxv,         false, "Show the auxiliary array passed to the process (on standard error). It can be enabled by setting the environment variable LD_SHOW_AUXV to 1" },
				{'\0', "show-env",         nullptr,  &Opts::showEnv,          false, "Show the environment variables passed to the process (on standard error). It can be enabled by setting the environment variable LD_SHOW_ENV to 1" },
				{'g',  "debug",            nullptr,  &Opts::debug,            false, "Provide debugger like GDB with file images of each version of the employed binaries (even when they've been overwritten meanwhile) for better debugging. This can be enabled by setting the environment variable LD_DEBUG to 1" },
				{'h',  "help",             nullptr,  &Opts::showHelp,         false, "Show this help" }
			},
			File::exists,
			[](const char *) -> bool { return true; });

		// Check arguments / show help
		if (!args.parse(argc, argv)) {
			LOG_ERROR << endl << "Parsing Arguments failed -- run " << endl << "   " << argv[0] << " --help" << endl << "for more information!" << endl;
			return EXIT_FAILURE;
		} else if (args.showVersion) {
			BuildInfo::print(cout, true);
			return EXIT_SUCCESS;
		} else if (args.showHelp) {
			args.help(cout, "\e[1mLuci\e[0m\nA linker/loader daemon experiment for academic purposes", argv[0], "Written 2021 - 2023 by Bernhard Heinloth <heinloth@cs.fau.de>\nFurther information: https://gitlab.cs.fau.de/luci-project/luci", "file[s]", "target args");
			return EXIT_SUCCESS;
		}

		// Setup
		Loader * loader = setup(base, argv[0], args, preload);

		// Binary Arguments
		if (args.has_positional()) {
			Vector<const char *> start_args;
			ObjectIdentity * start = nullptr;
			for (auto & bin : args.get_positional()) {
				auto flags = loader->default_flags;
				flags.updatable = 1;
				ObjectIdentity * o = loader->open(bin, flags, true);
				if (o == nullptr) {
					LOG_ERROR << "Failed loading " << bin << endl;
					return EXIT_FAILURE;
				} else if (start == nullptr) {
					start = o;
					start_args.push_back(bin);
				}
			}

			assert(start != nullptr);

			for (auto & lib : preload) {
				if (ObjectIdentity * loaded = loader->library(lib, loader->default_flags, true)) {
					LOG_INFO << "Preloaded " << lib << ": " << *loaded << endl;
				} else {
					LOG_WARNING << "Unable to preload '" << lib << "'!" << endl;
				}
			}

			if (args.linkstatic)
				for (auto & lib : args.libload)
					if ((lib[0] == ':' && !load_library_by_name(loader, lib, LIB_NAME_COLON)) || (!load_library_by_name(loader, lib, LIB_NAME_SHARED) && !load_library_by_name(loader, lib, LIB_NAME_STATIC))) {
						LOG_ERROR << "Cannot proceed since library '" << lib << "' is missing!" << endl;
						return EXIT_FAILURE;
					}

			LOG_DEBUG << "Library search order:" << endl;
			for (auto & obj : loader->lookup) {
				LOG_DEBUG_APPEND << " - " << obj << endl;
			}

			if (args.argv0 != nullptr) {
				start_args[0] = args.argv0;
			}

			start_args += args.get_terminal();
			if (!loader->run(start, start_args, 0, 0, args.entry)) {
				LOG_ERROR << "Start failed" << endl;
				return EXIT_FAILURE;
			}
		} else {
			LOG_ERROR << "No objects to run" << endl;
			return EXIT_FAILURE;
		}
	} else {
		// Interpreter uses default settings
		Opts opts;

		// Setup interpreter (luci)
		uintptr_t luci_base = Auxiliary::vector(Auxiliary::AT_BASE).value();
		Loader * loader = setup(luci_base == 0 ? BASEADDRESS : luci_base, nullptr, opts, preload);

		// Load target binary
		const char * bin = reinterpret_cast<const char *>(Auxiliary::vector(Auxiliary::AT_EXECFN).pointer());
		auto flags = loader->default_flags;
		// TODO: We currently ignore the premapped binary if updates are enabled (for dynamic Elfs).
		if (loader->config.dynamic_update && reinterpret_cast<Elf::Header*>(base)->type() == Elf::ET_DYN) {
			flags.updatable = true;
			flags.premapped = false;
		} else {
			flags.updatable = false;
			flags.premapped = true;
		}
		flags.executed_binary = true;
		ObjectIdentity * start = loader->open(bin, flags, true, NAMESPACE_BASE, flags.premapped == 1 ? base : 0);

		if (start == nullptr) {
			LOG_ERROR << "Failed loading " << bin << endl;
			return EXIT_FAILURE;
		}

		for (auto & lib : preload) {
			if (ObjectIdentity * loaded = loader->library(lib, loader->default_flags, true)) {
				LOG_INFO << "Preloaded " << lib << ": " << *loaded << endl;
			} else {
				LOG_WARNING << "Unable to preload '" << lib << "'!" << endl;
			}
		}

		LOG_DEBUG << "Library search order:" << endl;
		for (auto & obj : loader->lookup) {
			LOG_DEBUG_APPEND << " - " << obj << endl;
		}

		// Use initial luci stack (contents are the same anyways)
		extern uintptr_t __dlh_stack_pointer;
		if (!loader->run(start, __dlh_stack_pointer)) {
			LOG_ERROR << "Start failed" << endl;
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
