// Copyright 2020, Bernhard Heinloth
// SPDX-License-Identifier: AGPL-3.0-only

#include <dlh/container/initializer_list.hpp>
#include <dlh/container/vector.hpp>
#include <dlh/stream/output.hpp>
#include <dlh/parser/arguments.hpp>
#include <dlh/auxiliary.hpp>
#include <dlh/environ.hpp>
#include <dlh/macro.hpp>
#include <dlh/file.hpp>
#include <dlh/log.hpp>

#include "object/base.hpp"
#include "loader.hpp"

#ifndef LIBPATH_CONF
#error Macro config LIBPATH_CONF missing
#endif
#ifndef SOPATH
#error Macro config SOPATH missing
#endif


// Parse Arguments
struct Opts {
	int loglevel{Log::WARNING};
	const char * logfile{};
	Vector<const char *> libpath{};
	const char * libpathconf{ STR(LIBPATH_CONF) };
	Vector<const char *> preload{};
	bool dynamicUpdate{};
	bool dynamicDlUpdate{};
	bool dynamicWeak{};
	bool bindNow{};
	bool bindNot{};
	bool tracing{};
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

// Show build info
const char * __attribute__((weak)) build_elfo_version() { return nullptr; }
const char * __attribute__((weak)) build_bean_version() { return nullptr; }
const char * __attribute__((weak)) build_bean_date() { return nullptr; }
const char * __attribute__((weak)) build_bean_flags() { return nullptr; }
const char * __attribute__((weak)) build_capstone_version() { return nullptr; }
const char * __attribute__((weak)) build_capstone_flags() { return nullptr; }
const char * __attribute__((weak)) build_dlh_version() { return nullptr; }
const char * __attribute__((weak)) build_dlh_date() { return nullptr; }
const char * __attribute__((weak)) build_dlh_flags() { return nullptr; }
const char * __attribute__((weak)) build_luci_version() { return nullptr; }
const char * __attribute__((weak)) build_luci_date() { return nullptr; }
const char * __attribute__((weak)) build_luci_flags() { return nullptr; }
const char * __attribute__((weak)) build_luci_compatibility() { return nullptr; }
void build_info() {
	LOG_INFO << "Luci";
	if (build_luci_version() != nullptr)
		LOG_INFO_APPEND << ' ' << build_luci_version();
	if (build_luci_date() != nullptr)
		LOG_INFO_APPEND << " (built " << build_luci_date() << ')';
	LOG_INFO_APPEND << endl;
	if (build_luci_flags() != nullptr)
		LOG_TRACE << " with flags: " << build_luci_flags() << endl;
	if (build_luci_compatibility() != nullptr)
		LOG_DEBUG << " with glibc compatibility to " << build_luci_compatibility() << endl;

	if (build_bean_version() != nullptr) {
		LOG_DEBUG << "Using Bean " << build_bean_version();
		if (build_elfo_version() != nullptr)
			LOG_DEBUG_APPEND << " and Elfo " << build_elfo_version();
		if (build_bean_date() != nullptr)
			LOG_DEBUG_APPEND << " (built " << build_bean_date() << ')';
		LOG_DEBUG_APPEND << endl;
		if (build_bean_flags() != nullptr)
			LOG_TRACE << " with flags: " << build_bean_flags() << endl;
	}
	if (build_capstone_version() != nullptr) {
		LOG_DEBUG << "Using Capstone " << build_capstone_version() << endl;
		if (build_capstone_flags() != nullptr)
			LOG_TRACE << " with flags: " << build_capstone_flags() << endl;
	}

	if (build_dlh_version() != nullptr) {
		LOG_DEBUG << "Using DirtyLittleHelper (DLH) " << build_dlh_version();
		if (build_dlh_date() != nullptr)
			LOG_DEBUG_APPEND << " (built " << build_dlh_date() << ')';
		LOG_DEBUG_APPEND << endl;
		if (build_dlh_flags() != nullptr)
			LOG_TRACE << " with flags: " << build_dlh_flags() << endl;
	}
}

// Setup commands
static Loader * setup(uintptr_t luci_base, const char * luci_path, struct Opts & opts) {
	// Logger
	auto env_loglevel = Parser::string_as<int>(Environ::variable("LD_LOGLEVEL", true)) ;

	LOG.set(static_cast<Log::Level>(env_loglevel && env_loglevel.value() > opts.loglevel ? env_loglevel.value() : opts.loglevel));
	build_info();  // should be first output
	LOG_DEBUG << "Set log level to " << static_cast<int>(LOG.get()) << endl;

	const char * logfile = opts.logfile == nullptr ? Environ::variable("LD_LOGFILE", true) : opts.logfile;
	if (logfile != nullptr) {
		LOG_DEBUG << "Log to file " << logfile << endl;
		LOG.output(logfile);
	}

	// Tracing
	auto env_tracing = Parser::string_as<bool>(Environ::variable("LD_TRACING", true));
	if (opts.tracing || (env_tracing && env_tracing.value())) {
		LOG_ERROR << "Tracing not implemented yet!" << endl;
	}

	// New Loader
	auto env_dynamicupdate = Parser::string_as<bool>(Environ::variable("LD_DYNAMIC_UPDATE", true));
	auto env_dynamicdlupdate = Parser::string_as<bool>(Environ::variable("LD_DYNAMIC_DLUPDATE", true));
	auto env_dynamicweak = Parser::string_as<bool>(Environ::variable("LD_DYNAMIC_WEAK", true));
	Loader * loader = new Loader(luci_base, luci_path,
	   opts.dynamicUpdate || (env_dynamicupdate && env_dynamicupdate.value()),
	   opts.dynamicDlUpdate || (env_dynamicdlupdate && env_dynamicdlupdate.value()),
	   opts.dynamicWeak || (env_dynamicweak && env_dynamicweak.value()));
	if (loader == nullptr) {
		LOG_ERROR << "Unable to allocate loader" << endl;
	} else {
		if (loader->dynamic_update) {
			LOG_INFO << "Dynamic updates are enabled!" << endl;
		} else {
			LOG_DEBUG << "Dynamic updates are disabled!" << endl;
		}

		if (loader->dynamic_weak) {
			LOG_INFO << "Weak references in shared library are supported (nonstandard)!" << endl;
		}

		// Flags
		auto env_bindnow = Parser::string_as<bool>(Environ::variable("LD_BIND_NOW", true));
		loader->default_flags.bind_now = opts.bindNow || (env_bindnow && env_bindnow.value());
		auto env_bindnot = Parser::string_as<bool>(Environ::variable("LD_BIND_NOT", true));
		loader->default_flags.bind_not = opts.bindNot || (env_bindnot && env_bindnot.value());

		// Library search path
		for (auto & libpath : opts.libpath) {
			LOG_DEBUG << "Add '" << libpath << "' (from --library-path) to library search path..." << endl;
			loader->library_path_runtime.push_back(libpath);
		}

		char * ld_library_path = Environ::variable("LD_LIBRARY_PATH", true);
		if (ld_library_path != nullptr && *ld_library_path != '\0') {
			LOG_DEBUG << "Add '" << ld_library_path<< "' (from LD_LIBRARY_PATH) to library search path..." << endl;
			loader->library_path_runtime = String::split(ld_library_path, ';');
		}

		// Library search path config
		if (File::readable(opts.libpathconf)) {
			LOG_DEBUG << "Adding contents of '" << opts.libpathconf << "' to library search path..." << endl;
			loader->library_path_config += File::lines(opts.libpathconf);
		}
		char * ld_library_conf = Environ::variable("LD_LIBRARY_CONF", true);
		if (ld_library_conf != nullptr && *ld_library_conf != '\0' && File::readable(ld_library_conf)) {
			LOG_DEBUG << "Adding contents of '" << ld_library_conf << "' (from LD_LIBRARY_CONF) to library search path..." << endl;
			loader->library_path_config += File::lines(ld_library_conf);
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

		char * preload = Environ::variable("LD_PRELOAD", true);
		if (preload != nullptr && *preload != '\0') {
			LOG_DEBUG << "Loading '" << preload << "' (from LD_PRELOAD)..." << endl;
			for (auto & lib : String::split(preload, ';'))
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
				/* short & long name,  argument, element              required, help text,  optional validation function */
				{'l',  "log",          "LEVEL", &Opts::loglevel,        false, "Set log level (0 = none, 3 = warning, 6 = debug). This can also be done using the environment variable LD_LOGLEVEL.", [](const char * str) -> bool { int l = 0; return Parser::string(l, str) ? l >= Log::NONE && l <= Log::TRACE : false; }},
				{'f',  "logfile",      "FILE",  &Opts::logfile,         false, "Log to the given file. This can also be specified using the environment variable LD_LOGFILE" },
				{'p',  "library-path", "DIR",   &Opts::libpath,         false, "Add library search path (this parameter may be used multiple times to specify additional directories). This can also be specified with the environment variable LD_LIBRARY_PATH - separate mutliple directories by semicolon." },
				{'c',  "library-conf", "FILE",  &Opts::libpathconf,     false, "library path configuration" },
				{'P',  "preload",      "FILE",  &Opts::preload,         false, "Library to be loaded first (this parameter may be used multiple times to specify addtional libraries). This can also be specified with the environment variable LD_PRELOAD - separate mutliple directories by semicolon." },
				{'u',  "update",       nullptr, &Opts::dynamicUpdate,   false, "Enable dynamic updates. This option can also be enabled by setting the environment variable LD_DYNAMIC_UPDATE to 1" },
				{'U',  "dlupdate",     nullptr, &Opts::dynamicDlUpdate, false, "Enable updates of functions loaded using the DL interface -- only available if dynamic updates are enabled. This option can also be enabled by setting the environment variable LD_DYNAMIC_DLUPDATE to 1 " },
				{'T',  "tracing",      nullptr, &Opts::tracing,         false, "Enable tracing (using ptrace) during dynamic updates to detect access of outdated functions. This option can also be enabled by setting the environment variable LD_TRACING to 1" },
				{'w',  "weak",         nullptr, &Opts::dynamicWeak,     false, "Enable weak symbol references in dynamic files (nonstandard!). This option can also be enabled by setting the environment variable LD_DYNAMIC_WEAK to 1" },
				{'n',  "bind-now",     nullptr, &Opts::bindNow,         false, "Resolve all symbols at program start (instead of lazy resolution). This option can also be enabled by setting the environment variable LD_BIND_NOW to 1" },
				{'N',  "bind-not",     nullptr, &Opts::bindNot,         false, "Do not update GOT after resolving a symbol. This option cannot be used in conjunction with bind-now. It can be enabled by setting the environment variable LD_BIND_NOT to 1" },
				{'h',  "help",         nullptr, &Opts::showHelp,        false, "Show this help" }
			},
			File::executable,
			[](const char *) -> bool { return true; }
		);

		// Check arguments / show help
		if (!args.parse(argc, argv)) {
			LOG_ERROR << endl << "Parsing Arguments failed -- run " << endl << "   " << argv[0] << " --help" << endl << "for more information!" << endl;
			return EXIT_FAILURE;
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
		const char * sopath = STR(SOPATH);
		const char * sopath_env = Environ::variable("LD_PATH", true);
		Loader * loader = setup(luci_base == 0 ? BASEADDRESS : luci_base, sopath_env != nullptr && *sopath_env != '\0' ? sopath_env : sopath, opts);

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
