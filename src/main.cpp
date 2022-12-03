// Copyright 2020, Bernhard Heinloth
// SPDX-License-Identifier: AGPL-3.0-only

#include <dlh/container/initializer_list.hpp>
#include <dlh/container/vector.hpp>
#include <dlh/stream/output.hpp>
#include <dlh/parser/arguments.hpp>
#include <dlh/parser/config.hpp>
#include <dlh/auxiliary.hpp>
#include <dlh/macro.hpp>
#include <dlh/file.hpp>
#include <dlh/log.hpp>

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
	const char * logfile{};
	bool logfileAppend{};
	Vector<const char *> libpath{};
	const char * libpathconf{ STR(LIBPATH_CONF) };
	const char * luciconf{ STR(LDLUCI_CONF) };
	Vector<const char *> preload{};
	const char * debughash{};
	const char * statusinfo{};
	bool dynamicUpdate{};
	bool dynamicDlUpdate{};
	bool forceUpdate{};
	bool dynamicWeak{};
	bool detectOutdated{};
	bool bindNow{};
	bool bindNot{};
	bool tracing{};
	bool showVersion{};
	bool showHelp{};
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
static Loader * setup(uintptr_t luci_base, const char * luci_path, struct Opts & opts) {
	// Use config from environment vars and files
	Parser::Config config_file(opts.luciconf, Parser::Config::ENV_CONFIG, true);

	// Logger
	auto cfg_loglevel = config_file.value_as<int>("LD_LOGLEVEL");

	LOG.set(static_cast<Log::Level>(cfg_loglevel && cfg_loglevel.value() > opts.loglevel ? cfg_loglevel.value() : opts.loglevel));
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
	// Dynamic updates
	config_loader.dynamic_update = opts.dynamicUpdate || config_file.value_or_default<bool>("LD_DYNAMIC_UPDATE", false);
	// Dynamic updates of dl-funcs
	config_loader.dynamic_dlupdate = config_loader.dynamic_update ? (opts.dynamicDlUpdate || config_file.value_or_default<bool>("LD_DYNAMIC_DLUPDATE", false)) : false;
	// Force dynamic updates
	config_loader.force_update = config_loader.dynamic_update ? (opts.forceUpdate || config_file.value_or_default<bool>("LD_FORCE_UPDATE", false)) : false;
	// Weak linking
	config_loader.dynamic_weak = opts.dynamicWeak ||  config_file.value_or_default<bool>("LD_DYNAMIC_WEAK", false);
	// Detect access of outdated varsions
	config_loader.detect_outdated_access = config_loader.dynamic_update ? (opts.detectOutdated || config_file.value_or_default<bool>("LD_DETECT_OUTDATED", false)) : false;


	Loader * loader = new Loader(luci_base,	luci_path, config_loader);
	if (loader == nullptr) {
		LOG_ERROR << "Unable to allocate loader" << endl;
	} else {
		if (loader->config.dynamic_update) {
			LOG_INFO << "Dynamic updates are enabled!" << endl;
		} else {
			LOG_DEBUG << "Dynamic updates are disabled!" << endl;
		}

		if (loader->config.dynamic_weak) {
			LOG_INFO << "Weak references in shared library are supported (nonstandard)!" << endl;
		}

		// Debug Hash
		const char * debughash_uri = opts.debughash;
		if (debughash_uri == nullptr) {
			debughash_uri = config_file.value("LD_DEBUG_HASH");
		}
		if (debughash_uri != nullptr) {
			if (loader->debughash.connect(debughash_uri)) {
				LOG_DEBUG << "Using URI " << debughash_uri << " for debug (DWARF) hashing" << endl;
			} else {
				LOG_ERROR << "Debug hashing not available (invalid URI" << debughash_uri << ")" << endl;
			}
		}

		// Status info
		const char * statusinfo = opts.statusinfo;
		if (statusinfo == nullptr) {
			statusinfo = config_file.value("LD_STATUS_INFO");
		}
		if (statusinfo != nullptr) {
			if (auto open = Syscall::open(statusinfo, O_WRONLY | O_CREAT | O_TRUNC, 0644)) {
				LOG_DEBUG << "Writing status info to " << statusinfo << endl;
				loader->statusinfofd = open.value();
			} else {
				LOG_INFO << "Opening '" << statusinfo << "' for status info failed: " << open.error_message() << endl;
			}
		}
		LOG_ERROR << "status info is " << statusinfo << endl;

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

		// Preload Library
		for (auto & preload : opts.preload) {
			LOG_DEBUG << "Loading '" << preload << "' (from --preload)..." << endl;
			loader->library(preload);
		}

		char * preload = const_cast<char*>(config_file.value("LD_PRELOAD"));
		if (preload != nullptr && *preload != '\0') {
			LOG_DEBUG << "Loading '" << preload << "' (from LD_PRELOAD)..." << endl;
			for (auto & lib : String::split_inplace(preload, ';'))
				loader->library(lib);
		}
	}

	return loader;
}

int main(int argc, char* argv[]) {
	// We do no (implicit) self relocation, hence make sure it is already correct
	assert(reinterpret_cast<uintptr_t>(&_DYNAMIC) - _GLOBAL_OFFSET_TABLE_[0] == 0);

	assert(Auxiliary::vector(Auxiliary::AT_PHENT).value() == sizeof(Elf::Phdr));
	uintptr_t base = base_from_phdr(Auxiliary::vector(Auxiliary::AT_PHDR).pointer(), Auxiliary::vector(Auxiliary::AT_PHNUM).value());

	// Luci explicitly started from command line?
	if (base == BASEADDRESS) {
		// Available commandline options
		auto args = Parser::Arguments<Opts>({
				/* short & long name,    argument, element              required, help text,  optional validation function */
				{'l',  "log",            "LEVEL", &Opts::loglevel,        false, "Set log level (0 = none, 3 = warning, 6 = debug). This can also be done using the environment variable LD_LOGLEVEL.", [](const char * str) -> bool { int l = 0; return Parser::string(l, str) ? l >= Log::NONE && l <= Log::TRACE : false; }},
				{'f',  "logfile",        "FILE",  &Opts::logfile,         false, "Log to the given file. This can also be specified using the environment variable LD_LOGFILE" },
				{'a',  "logfile-append", nullptr, &Opts::logfileAppend,   false, "Append output to log file (instead of truncate). Requires logfile, can also be enabled by setting the environment variable LD_LOGFILE_APPEND to 1" },
				{'p',  "library-path",   "DIR",   &Opts::libpath,         false, "Add library search path (this parameter may be used multiple times to specify additional directories). This can also be specified with the environment variable LD_LIBRARY_PATH - separate mutliple directories by semicolon." },
				{'c',  "library-conf",   "FILE",  &Opts::libpathconf,     false, "library path configuration" },
				{'C',  "luci-conf",      "FILE",  &Opts::luciconf,        false, "Luci loader configuration file" },
				{'P',  "preload",        "FILE",  &Opts::preload,         false, "Library to be loaded first (this parameter may be used multiple times to specify addtional libraries). This can also be specified with the environment variable LD_PRELOAD - separate mutliple directories by semicolon." },
				{'s',  "statusinfo",     "FILE",  &Opts::statusinfo,      false, "File (named pipe) for logging successful and failed updates (latter would require a restart). Disabled if empty. This option can also be activated by setting the environment variable LD_STATUS_INFO" },
				{'d',  "debughash",      nullptr, &Opts::debughash,       false, "Socket URI (unix / tcp / udp) for retrieving debug data hashes. Disabled if empty. This option can also be activated by setting the environment variable LD_DEBUG_HASH" },
				{'u',  "update",         nullptr, &Opts::dynamicUpdate,   false, "Enable dynamic updates. This option can also be enabled by setting the environment variable LD_DYNAMIC_UPDATE to 1" },
				{'U',  "dlupdate",       nullptr, &Opts::dynamicDlUpdate, false, "Enable updates of functions loaded using the DL interface -- only available if dynamic updates are enabled. This option can also be enabled by setting the environment variable LD_DYNAMIC_DLUPDATE to 1" },
				{'F',  "force",          nullptr, &Opts::forceUpdate,     false, "Force dynamic update of changed files, even if they seem to be incompatible -- only available if dynamic updates are enabled. This option can also be enabled by setting the environment variable LD_FORCE_UPDATE to 1" },
				{'T',  "tracing",        nullptr, &Opts::tracing,         false, "Enable tracing (using ptrace) during dynamic updates to detect access of outdated functions. This option can also be enabled by setting the environment variable LD_TRACING to 1" },
				{'O',  "outdated",       nullptr, &Opts::detectOutdated,  false, "Unmap outdated executable segments and use user space page fault handler to detect code access in old versions.This option can also be enabled by setting the environment variable LD_DETECT_OUTDATED to 1"},
				{'w',  "weak",           nullptr, &Opts::dynamicWeak,     false, "Enable weak symbol references in dynamic files (nonstandard!). This option can also be enabled by setting the environment variable LD_DYNAMIC_WEAK to 1" },
				{'n',  "bind-now",       nullptr, &Opts::bindNow,         false, "Resolve all symbols at program start (instead of lazy resolution). This option can also be enabled by setting the environment variable LD_BIND_NOW to 1" },
				{'N',  "bind-not",       nullptr, &Opts::bindNot,         false, "Do not update GOT after resolving a symbol. This option cannot be used in conjunction with bind-now. It can be enabled by setting the environment variable LD_BIND_NOT to 1" },
				{'V',  "version",        nullptr, &Opts::showVersion,     false, "Show version information" },
				{'h',  "help",           nullptr, &Opts::showHelp,        false, "Show this help" }
			},
			File::executable,
			[](const char *) -> bool { return true; }
		);

		// Check arguments / show help
		if (!args.parse(argc, argv)) {
			LOG_ERROR << endl << "Parsing Arguments failed -- run " << endl << "   " << argv[0] << " --help" << endl << "for more information!" << endl;
			return EXIT_FAILURE;
		} else if (args.showVersion) {
			BuildInfo::print(cout, true);
			return EXIT_SUCCESS;
		} else if (args.showHelp) {
			args.help(cout, "\e[1mLuci\e[0m\nA toy linker/loader daemon experiment for academic purposes with hackability (not performance!) in mind.", argv[0], "Written 2021 by Bernhard Heinloth <heinloth@cs.fau.de>", "file[s]", "target args");
			return EXIT_SUCCESS;
		}

		// Setup
		Loader * loader = setup(base, argv[0], args);

		// Binary Arguments
		if (args.has_positional()) {
			Vector<const char *> start_args;
			ObjectIdentity * start = nullptr;
			for (auto & bin : args.get_positional()) {
				auto flags = loader->default_flags;
				flags.updatable = 0;
				ObjectIdentity * o = loader->open(bin, flags);
				if (o == nullptr) {
					LOG_ERROR << "Failed loading " << bin << endl;
					return EXIT_FAILURE;
				} else if (start == nullptr) {
					start = o;
					start_args.push_back(bin);
				}
			}

			assert(start != nullptr);

			start_args += args.get_terminal();
			if (!loader->run(start, start_args)) {
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
		Loader * loader = setup(luci_base == 0 ? BASEADDRESS : luci_base, nullptr, opts);

		// Load target binary
		const char * bin = reinterpret_cast<const char *>(Auxiliary::vector(Auxiliary::AT_EXECFN).pointer());
		auto flags = loader->default_flags;
		flags.updatable = 0;
		flags.premapped = 1;
		ObjectIdentity * start = loader->open(bin, flags, NAMESPACE_BASE, base);
		if (start == nullptr) {
			LOG_ERROR << "Failed loading " << bin << endl;
			return EXIT_FAILURE;
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
