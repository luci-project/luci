// Copyright 2020, Bernhard Heinloth
// SPDX-License-Identifier: AGPL-3.0-only

#include <cstring>
#include <cassert>
#include <sys/stat.h>
#include <inttypes.h>
#include <unistd.h>

#include <set>
#include <map>
#include <vector>
#include <unordered_map>

#include "argparser.hpp"
#include "utils.hpp"
#include "loader.hpp"
#include "object.hpp"
#include "log.hpp"

int main(int argc, char* argv[]) {
	struct Opts {
		int loglevel{Log::DEBUG};
		const char * logfile{};
		int logsize{};
		std::vector<const char *> libpath{};
		const char * libpathconf{"libpath.conf"};
		std::vector<const char *> preload{};
		bool dynamicUpdate{};
		bool showHelp{};
	};

	auto args = ArgParser<Opts>({
			/* short & long name,  argument, element            required, help text,  optional validation function */
			{'l',  "log",          "LEVEL", &Opts::loglevel,      false, "Set log level (0 = none, 3 = warning, 6 = debug)", [](const std::string & str) -> bool { int level = std::stoi(str); return level >= Log::NONE && level <= Log::TRACE; }},
			{'f',  "logfile",      "FILE",  &Opts::logfile,       false, "Log to file" },
			{'p',  "library-path", "DIR",   &Opts::libpath,       false, "Add library search path (this parameter may be used multiple times to specify additional directories)" },
			{'c',  "library-conf", "FILE",  &Opts::libpathconf,   false, "library path configuration" },
			{'P',  "preload",      "FILE",  &Opts::preload,       false, "Library to be loaded first (this parameter may be used multiple times to specify addtional libraries)" },
			{'d',  "dynamic",      nullptr, &Opts::dynamicUpdate, false, "Enable dynamic updates" },
			{'h',  "help",         nullptr, &Opts::showHelp,      false, "Show this help" }
		},
		[](const char * str) -> bool { return ::access(str, X_OK ) == 0; },
		[](const char *) -> bool { return true; }
	);

	if (!args.parse(argc, argv)) {
		LOG_ERROR << endl << "Parsing Arguments failed -- run " << endl << "   " << argv[0] << " --help" << endl << "for more information!" << endl;
		return EXIT_FAILURE;
	} else if (args.showHelp) {
		//args.help(std::cout, "\e[1mLuci\e[0m\nA toy linker/loader daemon experiment for academic purposes with hackability (not performance!) in mind.", argv[0], "Written 2021 by Bernhard Heinloth <heinloth@cs.fau.de>", "file[s]", "target args");
		return EXIT_SUCCESS;
	}

	// Logger
	LOG_DEBUG << "Setting log level to " << args.loglevel << endl;
	LOG.set(static_cast<Log::Level>(args.loglevel));

	if (args.has("--logfile")) {
		LOG_DEBUG << "Log to file " << args.logfile << endl;
		LOG.output(args.logfile);
	}

	// New Loader
	Loader loader(argv[0], args.dynamicUpdate);

	// Library search path
	for (auto & libpath : args.libpath) {
		LOG_DEBUG << "Add '" << libpath << "' (from --library-path) to library search path..." << endl;
		loader.library_path_runtime.push_back(libpath);
	}
	char * ld_library_path = Utils::env("LD_LIBRARY_PATH", true);
	if (ld_library_path != nullptr && *ld_library_path != '\0') {
		LOG_DEBUG << "Add '" << ld_library_path<< "' (from LD_LIBRARY_PATH) to library search path..." << endl;
		loader.library_path_runtime = Utils::split(ld_library_path, ';');
	}

	LOG_DEBUG << "Adding contents of '" << args.libpathconf << "' to library search path..." << endl;
	loader.library_path_config = Utils::file_contents(args.libpathconf);
	LOG_DEBUG << "Config has " << loader.library_path_config.size() << " entries!" << endl;

	// Preload Library
	for (auto & preload : args.preload) {
		LOG_DEBUG << "Loading '" << preload << "' (from --preload)..." << endl;
		loader.library(preload);
	}
	char * preload = Utils::env("LD_PRELOAD", true);
	if (preload != nullptr && *preload != '\0') {
		LOG_DEBUG << "Loading '" << preload << "' (from LD_PRELOAD)..." << endl;
		for (auto & lib : Utils::split(preload, ';'))
			loader.library(lib);
	}

	// Binary Arguments
	if (args.has_positional()) {
		ObjectIdentity * start = nullptr;
		for (auto & bin : args.get_positional()) {
			ObjectIdentity * o = loader.open(bin);
			if (o == nullptr) {
				LOG_ERROR << "Failed loading " << bin << endl;
				return EXIT_FAILURE;
			} else if (start == nullptr) {
				start = o;
			}
		}

		assert(start != nullptr);

		if (!loader.run(start, args.get_terminal())) {
			LOG_ERROR << "Start failed" << endl;
			return EXIT_FAILURE;
		}
	} else {
		LOG_ERROR << "No objects to run" << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
