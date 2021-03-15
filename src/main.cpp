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
#include <elfio/elfio.hpp>
#include <elfio/elfio_dump.hpp>

#include "argparser.hpp"
#include "object.hpp"

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Formatters/CsvFormatter.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>

using namespace ELFIO;

int main(int argc, const char* argv[]) {
	const int defaultLogLevel = plog::debug;

	static plog::ColorConsoleAppender<plog::TxtFormatter> log_console;
	plog::init(plog::debug, &log_console);

	struct Opts {
		int loglevel{defaultLogLevel};
		std::string logfile{};
		int logsize{};
		std::string libpath{};
		bool showHelp{};
	};

	auto args = ArgParser<Opts>({
			{"--log LEVEL", &Opts::loglevel, "Set log level (0 = none, 3 = warning, 5 = debug)" , {}, [](const std::string & str) -> bool { int level = std::stoi(str); return level >= plog::none && level <= plog::verbose; } },
			{"--logfile FILE", &Opts::logfile, "log to file" },
			{"--logsize SIZE", &Opts::logsize, "set maximum log file size" },
			{"--libpath DIR", &Opts::libpath, "add library search path" },
			{"--help", &Opts::showHelp, "Show this help" }
		},
		[](const std::string & str) -> bool { return std::filesystem::exists(str); },
		[](const std::string &) -> bool { return true; }
	);

	if (!args.parse(argc, argv)) {
		LOG_ERROR << std::endl << "Parsing Arguments failed -- run " << std::endl << ArgParser<Opts>::TAB << argv[0] << " --help" << std::endl << "for more information!" << std::endl;
		return EXIT_FAILURE;
	} else if (args.showHelp) {
		std::cout << args.help("Header", argv[0], "Footer", "file[s]", "[target args]");
		return EXIT_SUCCESS;
	}

	LOG_DEBUG << "Set log level to " << args.loglevel;
	plog::get()->setMaxSeverity(static_cast<plog::Severity>(args.loglevel));

	if (args.has("--logfile")) {
		LOG_DEBUG << "Log to file " << args.logfile;
		static plog::RollingFileAppender<plog::CsvFormatter> log_file(args.logfile.c_str(), args.logsize, 9);
		plog::get()->addAppender(&log_file);
	}

	if (args.has("--libpath")) {
		LOG_DEBUG << "Add '" << args.libpath << "' to library path ";
		Object::librarypath.push_back(args.libpath);
	}


	if (args.has_positional()) {
		for (auto & bin : args.get_positional()) {
			LOG_INFO << "Loading object " << bin;
			if (!Object::load_file(bin)) {
				LOG_ERROR << "Failed loading " << bin;
				return EXIT_FAILURE;
			}
		}

		assert(Object::objects.size() > 0);

		if (!Object::run_all(args.get_terminal())) {
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
