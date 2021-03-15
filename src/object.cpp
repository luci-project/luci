#include <bits/auxv.h>
#include <ext/stdio_filebuf.h>
#include <iostream>
#include <filesystem>

#include "object.hpp"
#include "object_dyn.hpp"
#include "object_exec.hpp"
#include "object_rel.hpp"
#include "process.hpp"
#include "generic.hpp"

// TODO: Parse /etc/ld.so.conf
std::vector<std::string> Object::librarypath = {
	"/usr/lib/x86_64-linux-gnu/libfakeroot",
	"/usr/local/lib/i386-linux-gnu",
	"/lib/i386-linux-gnu",
	"/usr/lib/i386-linux-gnu",
	"/usr/local/lib/i686-linux-gnu",
	"/lib/i686-linux-gnu",
	"/usr/lib/i686-linux-gnu",
	"/usr/local/lib",
	"/usr/local/lib/x86_64-linux-gnu",
	"/lib/x86_64-linux-gnu",
	"/usr/lib/x86_64-linux-gnu",
	"/lib32",
	"/usr/lib32",
	"/libx32",
	"/usr/libx32"
};

std::vector<Object *> Object::objects;

bool Object::load_library(std::string lib, const std::vector<std::string> & rpath, const std::vector<std::string> & runpath) {
	std::vector<std::string> search;
	search.insert(std::end(search), std::begin(rpath), std::end(rpath));
	search.insert(std::end(search), std::begin(librarypath), std::end(librarypath));
	search.insert(std::end(search), std::begin(runpath), std::end(runpath));
	for (auto & dir : search) {
		std::filesystem::path path = dir + "/" + lib;
		LOG_DEBUG << "Checking " << path << "...";
		if (std::filesystem::exists(path))
			return load_file(std::filesystem::absolute(path));
	}
	return false;
}

bool Object::load_file(std::string path) {
	LOG_DEBUG << "Loading " << path << "...";
	errno = 0;
	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		LOG_ERROR << "Reading file " << path << " failed: " << strerror(errno);
	} else {
		// TODO: This is very inefficient...
		ELFIO::elfio elf;
		if (elf.load(path)) {
			Object * o = nullptr;;
			switch (elf.get_type()) {
				case ET_EXEC:
					o = new ObjectExecutable(path, fd);
					break;
				case ET_DYN:
					o = new ObjectDynamic(path, fd);
					break;
				case ET_REL:
					o = new ObjectRelocatable(path, fd);
					break;
				default:
					LOG_ERROR << "Unsupported ELF type " << elf.get_type();
					return false;
			}
			if (o == nullptr) {
				LOG_ERROR << "Object is a nullptr";
				return false;
			}

			// Add to list
			objects.push_back(o);

			if (!o->load()) {
				LOG_ERROR << "Loading of " << path << " failed...";
				return false;
			}
			return true;
		} else {
			LOG_ERROR << "File '" << path << "' cannot be found or at least it is not a valid ELF file";
			exit(EXIT_FAILURE);
		}
	}
	LOG_INFO << "Successfully loaded " << path;
	return true;
}

bool Object::run_all(std::vector<std::string> args, uintptr_t stack_pointer, size_t stack_size) {
	// Allocate
	LOG_DEBUG << "Allocate memory";
	for (auto & obj : objects)
		if (!obj->allocate())
			return false;

	// Relocate
	LOG_DEBUG << "Relocate";

	// Protect
	LOG_DEBUG << "Protect memory";
	for (auto & obj : objects)
		if (!obj->protect())
			return false;

	// Run main binary
	return objects.front()->run(args, stack_pointer, stack_size);
}

void Object::unload_all() {
	for (auto & obj : objects)
		delete obj;
}

Object::Object(std::string path, int fd) : path(path), fd(fd) {
	// Hacky hack
	//__gnu_cxx::stdio_filebuf<char> filebuf{fd, std::ios_base::in | std::ios_base::binary};
	//std::istream fd_stream{&filebuf};
	elf.load(path);
};

Object::~Object(){
	for (auto & seg : segments)
		seg.unmap();

	close(fd);
}

std::string Object::get_file_name() {
	return std::filesystem::path(path).filename();
}

bool Object::get_memory_range(uintptr_t & start, uintptr_t & end) {
	if (segments.size() > 0) {
		start = segments.front().memory.get_start();
		end = segments.back().memory.get_end();
		return true;
	} else {
		return false;
	}
}

bool Object::allocate(bool copy) {
	for (auto & seg : segments)
		if (!seg.map(fd, copy))
			return false;

	return true;
}

bool Object::relocate() {
	return false;
}

bool Object::protect() {
	for (auto & seg : segments)
		if (!seg.protect())
			return false;

	return true;
}


bool Object::run(std::vector<std::string> args, uintptr_t stack_pointer, size_t stack_size) {
	if (segments.size() == 0)
		return false;

	uintptr_t base = segments[0].memory.base;

	Process p(stack_pointer, stack_size);

	// TODO: Should not be hard coded...
	p.aux[AT_PHDR] = base + elf.get_segments_offset();
	p.aux[AT_PHNUM] = elf.segments.size();

	args.insert(args.begin(), 1, path);
	p.init(args);

	uintptr_t entry = elf.get_entry();
	LOG_INFO << "Start at " << (void*)base << " + " << (void*)(entry);
	p.start(base + entry);
	return true;
}

uintptr_t Object::get_next_address() {
	uintptr_t next = 0;
	for (auto & obj : objects) {
		uintptr_t start = 0, end = 0;
		if (obj->get_memory_range(start, end) && end > next) {
			next = end;
		}
	}
	// Default address
	if (next == 0) {
		next = 0x500000;
	}
	return next;
}
