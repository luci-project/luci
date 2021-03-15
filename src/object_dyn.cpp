#include "object_dyn.hpp"

#include "generic.hpp"

bool ObjectDynamic::load() {
	return load_segments(get_next_address())
	    && load_libraries();
}

bool ObjectDynamic::load_libraries() {
	// load needed libaries
	std::vector<std::string> libs, rpath, runpath;
	auto sec_size = elf.sections.size();
	for (ELFIO::Elf_Half i = 0; i < sec_size; ++i ) {
		ELFIO::section* sec = elf.sections[i];
		if (SHT_DYNAMIC == sec->get_type()) {
			ELFIO::dynamic_section_accessor dynamic(elf, sec);
			auto dyn_no = dynamic.get_entries_num();
			for (ELFIO::Elf_Xword i = 0; i < dyn_no; ++i ) {
				ELFIO::Elf_Xword tag = 0;
				ELFIO::Elf_Xword value = 0;
				std::string str;
				dynamic.get_entry(i, tag, value, str);
				switch (tag) {
					case DT_NEEDED:
						libs.push_back(str);
						break;
					case DT_RPATH:
						rpath.push_back(str);
						break;
					case DT_RUNPATH:
						runpath.push_back(str);
						break;
				}
			}
		}
	}

	bool success = true;
	for (auto & lib : libs) {
		if (!load_library(lib, rpath, runpath)) {
			LOG_WARNING << "Unresolved dependency: " << lib;
			success = false;
		}
	}
	return success;
}
