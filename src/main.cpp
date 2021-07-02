// Copyright 2020, Bernhard Heinloth
// SPDX-License-Identifier: AGPL-3.0-only

#include <dlh/container/initializer_list.hpp>
#include <dlh/container/vector.hpp>
#include <dlh/container/tree.hpp>
#include <dlh/container/hash.hpp>
#include <dlh/stream/output.hpp>
#include <dlh/parser/arguments.hpp>
#include <dlh/utils/auxiliary.hpp>
#include <dlh/utils/environ.hpp>
#include <dlh/utils/string.hpp>
#include <dlh/utils/file.hpp>
#include <dlh/utils/log.hpp>

#include "object/base.hpp"

#include "loader.hpp"
#include "init.hpp"

#ifndef LIBPATH_CONF
#define LIBPATH_CONF libpath.conf
#endif
#define XSTR(var) #var
#define STR(var) XSTR(var)


// Parse Arguments
struct Opts {
	int loglevel{Log::DEBUG};
	const char * logfile{};
	Vector<const char *> libpath{};
	const char * libpathconf{ STR(LIBPATH_CONF) };
	Vector<const char *> preload{};
	bool dynamicUpdate{};
	bool showHelp{};
};

// Symbols defined by the linker
extern uintptr_t _DYNAMIC[];
extern const uintptr_t _GLOBAL_OFFSET_TABLE_[];

// Get ELF base from program header (Hack)
static void * base_from_phdr(void * phdr_ptr, long int entries = 1) {
	Elf::Phdr * phdr = reinterpret_cast<Elf::Phdr *>(phdr_ptr);
	uintptr_t ptr = reinterpret_cast<uintptr_t>(phdr_ptr);
	bool valid = false;
	uintptr_t base = phdr[0].p_vaddr;
	for (long int i = 0; i < entries; i++)
		switch(phdr[i].p_type) {
			case Elf::PT_PHDR:
				return reinterpret_cast<void*>(ptr - phdr[i].p_offset);
			case Elf::PT_LOAD:
				if (ptr >= phdr[i].p_vaddr && ptr <= phdr[i].p_vaddr + phdr[i].p_memsz)
					valid = true;
				if (base > phdr[i].p_vaddr)
					base = phdr[i].p_vaddr;
			default:
				continue;
		}
	assert(valid);
	return reinterpret_cast<void*>(base);
}

static Loader * setup(void * luci_base, struct Opts & opts) {
	// Logger
	LOG_DEBUG << "Setting log level to " << opts.loglevel << endl;
	LOG.set(static_cast<Log::Level>(opts.loglevel));

	if (opts.logfile != nullptr) {
		LOG_DEBUG << "Log to file " << opts.logfile << endl;
		LOG.output(opts.logfile);
	}

	// New Loader
	Loader * loader = new Loader(luci_base, opts.dynamicUpdate);
	assert(loader != nullptr);

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

	LOG_DEBUG << "Adding contents of '" << opts.libpathconf << "' to library search path..." << endl;
	loader->library_path_config += File::lines(opts.libpathconf);
	LOG_DEBUG << "Config has " << loader->library_path_config.size() << " entries!" << endl;

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

	return loader;
}

static void * const baseaddress = reinterpret_cast<void *>(BASEADDRESS);
__attribute__ ((visibility ("default"))) int main(int argc, char* argv[]) {
	// We do no (implicit) self relocation, hence make sure it is already correct
	assert(reinterpret_cast<uintptr_t>(&_DYNAMIC) - _GLOBAL_OFFSET_TABLE_[0] == 0);

	// Initialize
	if (!init())
		return EXIT_FAILURE;

	assert(Auxiliary::vector(Auxiliary::AT_PHENT).value() == sizeof(Elf::Phdr));
	void * base = base_from_phdr(Auxiliary::vector(Auxiliary::AT_PHDR).pointer(), Auxiliary::vector(Auxiliary::AT_PHNUM).value());

	// Luci explicitly started from command line?
	if (base == baseaddress) {
		// Available commandline options
		auto args = Parser::Arguments<Opts>({
				/* short & long name,  argument, element            required, help text,  optional validation function */
				{'l',  "log",          "LEVEL", &Opts::loglevel,      false, "Set log level (0 = none, 3 = warning, 6 = debug)", [](const char * str) -> bool { int l = 0; return Parser::string(l, str) ? l >= Log::NONE && l <= Log::TRACE : false; }},
				{'f',  "logfile",      "FILE",  &Opts::logfile,       false, "Log to file" },
				{'p',  "library-path", "DIR",   &Opts::libpath,       false, "Add library search path (this parameter may be used multiple times to specify additional directories)" },
				{'c',  "library-conf", "FILE",  &Opts::libpathconf,   false, "library path configuration" },
				{'P',  "preload",      "FILE",  &Opts::preload,       false, "Library to be loaded first (this parameter may be used multiple times to specify addtional libraries)" },
				{'d',  "dynamic",      nullptr, &Opts::dynamicUpdate, false, "Enable dynamic updates" },
				{'h',  "help",         nullptr, &Opts::showHelp,      false, "Show this help" }
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
		Loader * loader = setup(base, args);

		// Binary Arguments
		if (args.has_positional()) {
			Vector<const char *> start_args;
			ObjectIdentity * start = nullptr;
			for (auto & bin : args.get_positional()) {
				ObjectIdentity * o = loader->open(bin);
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
		void * luci_base = Auxiliary::vector(Auxiliary::AT_BASE).pointer();
		Loader * loader = setup(luci_base == nullptr ? baseaddress : luci_base, opts);

		// Load target binary
		const char * bin = reinterpret_cast<const char *>(Auxiliary::vector(Auxiliary::AT_EXECFN).pointer());
		ObjectIdentity * start = loader->open(base, true, false, true, bin);
		if (start == nullptr) {
			LOG_ERROR << "Failed loading " << bin << endl;
			return EXIT_FAILURE;
		}

		// TODO: Use initial luci stack (contents are the same anyways)
		Vector<const char *> start_args = { bin };
		if (!loader->run(start, start_args)) {
			LOG_ERROR << "Start failed" << endl;
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
