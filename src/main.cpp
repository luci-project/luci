// Copyright 2020, Bernhard Heinloth
// SPDX-License-Identifier: AGPL-3.0-only

#include <cstring>
#include <cassert>
#include <sys/stat.h>
#include <bits/auxv.h>
#include <inttypes.h>

#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <filesystem>
#include <unordered_map>

#include "argparser.hpp"
#include "utils.hpp"
#include "object.hpp"

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Formatters/CsvFormatter.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>

extern char **environ;

int main(int argc, const char* argv[]) {
	const int defaultLogLevel = plog::debug;

	static plog::ColorConsoleAppender<plog::TxtFormatter> log_console;
	plog::init(plog::debug, &log_console);

	std::unordered_map<std::string, std::string> env;
	for (char ** e = environ; *e != NULL; e++) {
		std::string v(*e);
		size_t p = v.find('=');
		if (p != v.npos)
			env.emplace(v.substr(0, p), v.substr(p+1));
	}

	struct Opts {
		int loglevel{defaultLogLevel};
		std::string logfile{};
		int logsize{};
		std::string libpath{};
		std::string libpathconf{"libpath.conf"};
		std::string preload{};
		bool showHelp{};
	};

	auto args = ArgParser<Opts>({
			{"--log LEVEL", &Opts::loglevel, "Set log level (0 = none, 3 = warning, 5 = debug)" , {}, [](const std::string & str) -> bool { int level = std::stoi(str); return level >= plog::none && level <= plog::verbose; } },
			{"--logfile FILE", &Opts::logfile, "log to file" },
			{"--logsize SIZE", &Opts::logsize, "set maximum log file size" },
			{"--library-path DIR", &Opts::libpath, "add library search path (colon separated)" },
			{"--library-conf FILE", &Opts::libpathconf, "library path configuration" },
			{"--preload FILE", &Opts::preload, "library to be loaded first" },
			{"--help", &Opts::showHelp, "Show this help" }
		},
		[](const std::string & str) -> bool { return std::filesystem::exists(str); },
		[](const std::string &) -> bool { return true; }
	);

	if (!args.parse(argc, argv)) {
		LOG_ERROR << std::endl << "Parsing Arguments failed -- run " << std::endl << ArgParser<Opts>::TAB << argv[0] << " --help" << std::endl << "for more information!" << std::endl;
		return EXIT_FAILURE;
	} else if (args.showHelp) {
		std::cout << args.help("Luci", argv[0], "Footer", "file[s]", "[target args]");
		return EXIT_SUCCESS;
	}

	// Logger
	LOG_DEBUG << "Set log level to " << args.loglevel;
	plog::get()->setMaxSeverity(static_cast<plog::Severity>(args.loglevel));

	if (args.has("--logfile")) {
		LOG_DEBUG << "Log to file " << args.logfile;
		static plog::RollingFileAppender<plog::CsvFormatter> log_file(args.logfile.c_str(), args.logsize, 9);
		plog::get()->addAppender(&log_file);
	}

	// Library search path
	if (args.has("--library-path")) {
		if (!args.libpath.empty()) {
			LOG_DEBUG << "Add '" << args.libpath << "' (from --library-path) to library search path...";
			Object::library_path_runtime = Utils::split(args.libpath, ';');
		}
	} else {
		auto libpath = env.find("LD_LIBRARY_PATH");
		if (libpath != env.end() && !libpath->second.empty()) {
			LOG_DEBUG << "Add '" << libpath->second << "' (from " << libpath->first << ") to library search path...";
			Object::library_path_runtime = Utils::split(libpath->second, ';');
		}
	}

	Object::library_path_config = Utils::file_contents(args.libpathconf);

	// Preload Library
	if (args.has("--preload")) {
		if (!args.preload.empty()) {
			LOG_DEBUG << "Loading '" << args.preload << "' (from --preload)...";
			for (auto & lib : Utils::split(args.preload, ';'))
				Object::load_file(lib);
		}
	} else {
		auto preload = env.find("LD_PRELOAD");
		if (preload != env.end() && !preload->second.empty()) {
			LOG_DEBUG << "Loading '" << preload->second << "' (from " << preload->first << ")...";
			for (auto & lib : Utils::split(preload->second, ';'))
				Object::load_file(lib);
		}
	}

	// Binary Arguments
	if (args.has_positional()) {
		Object * start = nullptr;
		for (auto & bin : args.get_positional()) {
			LOG_INFO << "Loading object " << bin;
			Object * o = Object::load_file(bin);
			if (o == nullptr) {
				LOG_ERROR << "Failed loading " << bin;
				return EXIT_FAILURE;
			} else if (start == nullptr) {
				start = o;
			}
		}

		assert(start != nullptr);

		if (!start->run(args.get_terminal())) {
			LOG_ERROR << "Start failed";
			return EXIT_FAILURE;
		}
	} else {
		LOG_ERROR << "No objects to run";
		return EXIT_FAILURE;
	}

	Object::unload_all();
	return EXIT_SUCCESS;
}
