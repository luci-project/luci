// Copyright 2020, Bernhard Heinloth
// SPDX-License-Identifier: AGPL-3.0-only

#include <cstring>
#include <cassert>
#include <sys/stat.h>
#include <sys/mman.h>
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

#include "page.hpp"
#include "memory.hpp"
#include "process.hpp"
#include "argparser.hpp"
#include "allocation.hpp"
#include "object.hpp"
#include "relocation.hpp"
#include "section.hpp"
#include "symbol.hpp"

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Formatters/CsvFormatter.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>

using namespace ELFIO;


std::vector<Object*> objects;
std::unordered_map<std::string, Symbol*> globSymbols;

std::set<std::string> ignoredSymbols({"_GLOBAL_OFFSET_TABLE_"});
uintptr_t start = 0;

static void load(const std::string & file) {
	Object * obj = new Object(file);
	objects.push_back(obj);

	elfio& elf = obj->elf;

	switch (elf.get_type()) {
		case ET_EXEC:
			break;
		case ET_DYN:
			if (start == 0)
				start = 0x500000;
			break;
		default:
			std::cerr << "Unsupported ELF type" << std::endl;
			::exit(EXIT_FAILURE);
	}

	Elf_Half segments = elf.segments.size();
	// Do we have a Segment Header (Executables & Shared Objects only)
	uintptr_t next = start;
	if (segments > 0) {
		for (Elf_Half i = 0; i < segments; ++i ) {
			segment* seg = elf.segments[i];
			if (PT_LOAD == seg->get_type()) {
				Page virt(start + seg->get_virtual_address(), seg->get_memory_size());
				if (obj->base == 0) {
					obj->base = start + seg->get_virtual_address();
				}
				std::cout << "Page " << (void*)virt.base() << " (" << std::dec << virt.length() << " B) from "
				          << (void*)seg->get_virtual_address();
				if (start > 0)
					std::cout << " + " << (void*)start;
				std::cout << " (" << seg->get_memory_size() << " B)" << std::endl;
				next = virt.end();

				void * base = mmap((void*)virt.base(), virt.length(), PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_SHARED | MAP_ANONYMOUS, 0, 0);
				if (base == MAP_FAILED) {
					std::cerr << "Mapping " << std::dec << virt.length() << " Bytes at " << (void*)virt.base() << " failed: " << strerror(errno) << std::endl;
					exit(EXIT_FAILURE);
				} else if (base != (void*)virt.base()) {
					std::cerr << "Requested Mapping at " << (void*)virt.base() << " but got " << base << std::endl;
					exit(EXIT_FAILURE);
				}
				::memset((void*)virt.base(), 0, virt.length());

				std::cout << "Copy " << (void*)seg->get_data() << " to " << (void*)seg->get_virtual_address();
				if (start > 0)
					std::cout << " + " << (void*)start;
				std::cout << " (" << seg->get_file_size() << " B)"<< std::endl;
				::memcpy((void*)(start + seg->get_virtual_address()), seg->get_data(), seg->get_file_size());
			}
		}
		/* TODO
		//elf.get_dyn
		for (Elf_Half i = 0; i < segments; ++i ) {
			segment* seg = elf.segments[i];
			if (PT_LOAD == seg->get_type()) {
			}
		}
		*/
	}
	std::cout << "all copied " << std::endl;
dump::dynamic_tags( std::cout, elf );
	return;

	dump::header( std::cout, elf );
	   dump::section_headers( std::cout, elf );
    dump::segment_headers( std::cout, elf );

	Elf_Half n = elf.sections.size();

	std::vector<section *> symbols;
	std::vector<section *> relocations;
	for (Elf_Half i = 0; i < n; ++i) {
		section* sec = elf.sections[i];

		// Store all allocatable sections
		if (sec->get_flags() & SHF_ALLOC) {
			obj->sections[sec->get_index()] = new Section(obj, sec->get_index());
		}

		// Examine section
		switch (sec->get_type()) {
			case SHT_SYMTAB:
				symbols.push_back(sec);
				break;
			case SHT_RELA:
			case SHT_REL:
				relocations.push_back(sec);
				break;
		}
	}

	// Load Symbols
	assert(symbols.size() == 1);
	for (auto sec: symbols) {
		symbol_section_accessor symbols(elf, sec);

		Elf_Xword sym_no = symbols.get_symbols_num();
		for (Elf_Xword i = 0; i < sym_no; ++i ) {
			Symbol * sym = new Symbol();
			Elf_Half section_index;
			symbols.get_symbol(i, sym->name, sym->value, sym->size, sym->bind, sym->type, section_index, sym->other);

			if (section_index == SHN_UNDEF) {
				if (sym->bind & STB_GLOBAL) {
				/*	assert(sym->type == STT_NOTYPE);
					assert(sym->size == 0);
					assert(sym->value == 0);
				*/	if (ignoredSymbols.contains(sym->name)) {
						std::cerr << "Ignoring undefined symbol " << sym->name << " in " << obj->path << std::endl;
					} else {
						obj->undefSymbols[sym->name] = i;
					}
				}
				delete(sym);
			} else if (obj->sections.contains(section_index)) {
				sym->section = obj->sections[section_index];

				assert(!obj->symbols.contains(i));
				obj->symbols[i] = sym;

				if (sym->bind & STB_GLOBAL) {
					if (globSymbols.contains(sym->name) && !(sym->bind & STB_WEAK)) {
						if (globSymbols[sym->name]->bind & STB_WEAK) {
							globSymbols[sym->name] = sym;
						} else {
							std::cerr << "Multiple occurence of Symbol " << sym->name << std::endl;
							exit(EXIT_FAILURE);
						}
					} else {
						globSymbols[sym->name] = sym;
					}
				}
			}
		}
	}

	// Preserve relocations
	for (auto sec: relocations) {
		relocation_section_accessor rel(elf, sec);
		Elf_Xword no_rel = rel.get_entries_num();
		for (Elf_Xword j = 0; j < no_rel; ++j) {
			if (obj->sections.contains(sec->get_info())) {
				Section * target = obj->sections[sec->get_info()];
				Relocation * r = new Relocation(target);
				if (rel.get_entry(j, r->offset, r->symbol, r->type, r->addend)) {
					target->relocations.push_back(r);
				} else {
					std::cerr << "Invalid relocation entry " << j << std::endl;
				}
			} else  {
				std::cerr << "Invalid info entry " << sec->get_info() << " in " << j  << std::endl;
			}
		}
	}
}

int main(int argc, const char* argv[]) {
	const int defaultLogLevel = plog::debug;

	static plog::ColorConsoleAppender<plog::TxtFormatter> log_console;
	plog::init(plog::debug, &log_console);

	struct Opts {
		int loglevel{defaultLogLevel};
		std::string logfile{};
		int logsize{};
		bool showHelp{};
	};

	auto args = ArgParser<Opts>({
			{"--log LEVEL", &Opts::loglevel, "Set log level (0 = none, 3 = warning, 5 = debug)" , {}, [](const std::string & str) -> bool { int level = std::stoi(str); return level >= plog::none && level <= plog::verbose; } },
			{"--logfile FILE", &Opts::logfile, "log to file" },
			{"--logsize SIZE", &Opts::logsize, "set maximum log file size" },
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

	if (args.has_positional()) {
		for (auto & bin : args.get_positional()) {
			LOG_INFO << "Loading object " << bin;
			load(bin);
		}

		auto & obj = objects[0];

		Process p;
		p.aux[AT_PHDR] = obj->base + obj->elf.get_segments_offset();
		p.aux[AT_PHNUM] = obj->elf.segments.size();

		auto proc_args = args.get_terminal();
		proc_args.insert(proc_args.begin(), 1, obj->path);
		p.init(proc_args);

		Process::dump(p);

		uintptr_t entry = obj->elf.get_entry();
		std::cerr << "Entry at " << (void*)(entry);
		if (start > 0)
			std::cerr << " + " << (void*)(start);
		std::cerr << std::endl;
		p.start(entry + start);
	} else {
		LOG_ERROR << "No objects to run";
		return EXIT_FAILURE;
	}
/*

	try {
		cxxopts::Options options("lilo", " - Link'n'Load");
		options
		       .positional_help("[binaries/objects]")
		       .show_positional_help();

		bool do_alloc = false;
		bool do_reloc = false;
		bool do_hash = false;
		bool do_dump = false;
		bool do_start = false;

		options.add_options()
		                     ("a, alloc", "allocate & load into memory", cxxopts::value<bool>(do_alloc))
		                     ("r, reloc", "relocate symbols", cxxopts::value<bool>(do_reloc))
		                     ("h, hash", "hash symbols", cxxopts::value<bool>(do_hash))
		                     ("d, dump", "dump memory", cxxopts::value<bool>(do_dump))
		                     ("s, start", "start application", cxxopts::value<bool>(do_start))
		                     ("binary", "Target binary", cxxopts::value<std::string>(), "FILE")
		                     ("args", "Arguments for the target", cxxopts::value<std::vector<std::string>>())
		                     ("help", "Print this help")
		;

		options.parse_positional({"binary", "args"});

		auto result = options.parse(argc, argv);

		if (result.count("help")) {
			std::cout << options.help() << std::endl;
			exit(EXIT_SUCCESS);
		}


		if (result.count("binary")) {
			auto &bin = result["binary"].as<std::string>();
			std::cout << "Loading " << bin << "..." << std::endl;
			load(bin);
			/*auto& bins = result["binary"].as<std::vector<std::string>>();
			for (const auto& bin : bins) {
				std::cout << "Loading " << bin << "..." << std::endl;
				load(bin);
			}

			for (auto obj : objects) {
				// Resolve undefined Symbols
				for (auto sym : obj->undefSymbols) {
					auto glob = globSymbols.find(sym.first);
					if (glob != globSymbols.end()) {
						obj->symbols[sym.second] = glob->second;
					} else {
						std::cerr << "No occurence of Symbol " << sym.first
						          << " required by " << obj->path << std::endl;
					}
				}

				for (auto sec : obj->sections) {
					// Connect relocations to symbols
					for (auto rel : sec.second->relocations) {
						auto sym = rel->getSymbol();
						if (sym != nullptr) {
							sym->relocations.push_back(rel);
						}
					}

					// Store all allocatable sections
					Allocation::add(sec.second);
				}
			}
			*//*
		} else {
			std::cerr << "Usage: " << argv[0] << " [binaries/objects]" << std::endl;
			exit(EXIT_FAILURE);
		}

		if (result.count("args")) {
			auto & obj = objects[0];

			ProcessFrame pf;
			pf.aux[AT_PHDR] = obj->base + obj->elf.get_segments_offset();
			pf.aux[AT_PHNUM] = obj->elf.segments.size();

			auto args = result["args"].as<std::vector<std::string>>();
			args.insert(args.begin(), 1, obj->path);
			pf.setup(args);
			ProcessFrame::print(pf.argc, pf.argv, pf.envp);

			uintptr_t entry = obj->elf.get_entry();
			std::cerr << "Entry at " << (void*)(entry);
			if (start > 0)
				std::cerr << " + " << (void*)(start);
			std::cerr << std::endl;
			pf.run(entry + start);


		}


		// Allocate && load Memory
		if (do_alloc) {
			Allocation::loadAll();
		}

		if (do_reloc) {
			if (!do_alloc) {
				std::cerr << "Cannot relocate without allocate (-a)!" << std::endl;
				exit(EXIT_FAILURE);
			}
			// relocate
			Allocation::relocateAll();
		}

		if (do_hash) {
			for (auto obj : objects) {
				std::cout << obj->path << ":" << std::endl;
				// Resolve undefined Symbols
				for (auto sym : obj->undefSymbols) {
					auto glob = globSymbols.find(sym.first);
					if (glob != globSymbols.end()) {
						obj->symbols[sym.second] = glob->second;
					} else {
						std::cerr << "No occurence of Symbol " << sym.first
						          << " required by " << obj->path << std::endl;
					}
				}

				for (auto symp : obj->symbols) {
					std::cout << "\t" << symp.second->name << ": " << symp.second->hash() << std::endl;


				}
			}
		}

		if (do_dump) {
			if (!do_alloc) {
				std::cerr << "Cannot dump without allocate (-a)!" << std::endl;
				exit(EXIT_FAILURE);
			}
			// Show Dump of full Memory
			Allocation::dumpAll();
		}

		if (do_start) {
			if (!do_alloc) {
				std::cerr << "Cannot start without allocate (-a)!" << std::endl;
				exit(EXIT_FAILURE);
			}
			if (!do_reloc) {
				std::cerr << "Starting without reloc is dangerous!" << std::endl;
			}
			// Show Dump of full Memory
			globSymbols["_start"]->execute();
		}

		// Clean
		Allocation::clearAll();

	} catch (const cxxopts::OptionException& e) {
		std::cerr << "error parsing options: " << e.what() << std::endl;
		exit(EXIT_FAILURE);
	}
*/
	return EXIT_SUCCESS;
}
