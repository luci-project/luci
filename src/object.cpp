#include <iostream>
#include <filesystem>

#include "object.hpp"

namespace fs = std::filesystem;

Object::Object(const std::string & path) : path(path) {
	if (!elf.load(path)) {
		std::cerr << "File '" << path
		          << "' is not found or at least not a valid ELF file"
		          << std::endl;
		exit(EXIT_FAILURE);
	}
}

std::string Object::getFileName() {
	return fs::path(path).filename();
}
