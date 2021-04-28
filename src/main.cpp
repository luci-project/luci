// Copyright 2020, Bernhard Heinloth
// SPDX-License-Identifier: AGPL-3.0-only

#include <cstring>
#include <cassert>
#include <sys/stat.h>
#include <bits/auxv.h>
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

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Formatters/CsvFormatter.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>

extern char **environ;

static char * env(const char * name, bool consume = false) {
	for (char ** ep = environ; *ep != NULL; ep++) {
		char * e = *ep;
		const char * n = name;
		while (*n == *e && *e != '\0') {
			n++;
			e++;
		}
		if (*e == '=' && *n == '\0') {
			if (consume)
				**ep = '\0';
			return e + 1;
		}
	}
	return nullptr;
}

int main(int argc, char* argv[]) {
	const int defaultLogLevel = plog::debug;

	static plog::ColorConsoleAppender<plog::TxtFormatter> log_console;
	plog::init(plog::debug, &log_console);

	struct Opts {
		int loglevel{defaultLogLevel};
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
			{'l',  "log",          "LEVEL", &Opts::loglevel,      false, "Set log level (0 = none, 3 = warning, 5 = debug)", [](const std::string & str) -> bool { int level = std::stoi(str); return level >= plog::none && level <= plog::verbose; }},
			{'\0', "logfile",      "FILE",  &Opts::logfile,       false, "Log to file" },
			{'\0', "logsize",      "SIZE",  &Opts::logsize,       false, "Set maximum log file size" },
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
		LOG_ERROR << std::endl << "Parsing Arguments failed -- run " << std::endl << "   " << argv[0] << " --help" << std::endl << "for more information!" << std::endl;
		return EXIT_FAILURE;
	} else if (args.showHelp) {
		args.help(std::cout, "\e[1mLuci\e[0m\nA toy linker/loader daemon experiment for academic purposes with hackability (not performance!) in mind.", argv[0], "Written 2021 by Bernhard Heinloth <heinloth@cs.fau.de>", "file[s]", "[target args]");
		return EXIT_SUCCESS;
	}

	// Logger
	LOG_DEBUG << "Setting log level to " << args.loglevel;
	plog::get()->setMaxSeverity(static_cast<plog::Severity>(args.loglevel));

	if (args.has("--logfile")) {
		LOG_DEBUG << "Log to file " << args.logfile;
		static plog::RollingFileAppender<plog::CsvFormatter> log_file(args.logfile, args.logsize, 9);
		plog::get()->addAppender(&log_file);
	}

	// New Loader
	Loader loader(argv[0], args.dynamicUpdate);

	// Library search path
	for (auto & libpath : args.libpath) {
		LOG_DEBUG << "Add '" << libpath << "' (from --library-path) to library search path...";
		loader.library_path_runtime.push_back(libpath);
	}
	char * ld_library_path = env("LD_LIBRARY_PATH", true);
	if (ld_library_path != nullptr && *ld_library_path != '\0') {
		LOG_DEBUG << "Add '" << ld_library_path<< "' (from LD_LIBRARY_PATH) to library search path...";
		loader.library_path_runtime = Utils::split(ld_library_path, ';');
	}

	LOG_DEBUG << "Adding contents of '" << args.libpathconf << "' to library search path...";
	loader.library_path_config = Utils::file_contents(args.libpathconf);
	LOG_DEBUG << "Config has " << loader.library_path_config.size() << " entries!";

	// Preload Library
	for (auto & preload : args.preload) {
		LOG_DEBUG << "Loading '" << preload << "' (from --preload)...";
		loader.file(preload);
	}
	char * preload = env("LD_PRELOAD", true);
	if (preload != nullptr && *preload != '\0') {
		LOG_DEBUG << "Loading '" << preload << "' (from LD_PRELOAD)...";
		for (auto & lib : Utils::split(preload, ';'))
			loader.file(lib);
	}

	// Binary Arguments
	if (args.has_positional()) {
		Object * start = nullptr;
		for (auto & bin : args.get_positional()) {
			LOG_INFO << "Loading object " << bin;
			Object * o = loader.file(bin);
			if (o == nullptr) {
				LOG_ERROR << "Failed loading " << bin;
				return EXIT_FAILURE;
			} else if (start == nullptr) {
				start = o;
			}
		}

		assert(start != nullptr);

		if (!loader.run(start, args.get_terminal())) {
			LOG_ERROR << "Start failed";
			return EXIT_FAILURE;
		}
	} else {
		LOG_ERROR << "No objects to run";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
