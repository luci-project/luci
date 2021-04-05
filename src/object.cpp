#include "object.hpp"

#include <ext/stdio_filebuf.h>
#include <fstream>
#include <iostream>
#include <filesystem>

#include "auxiliary.hpp"
#include "object_dyn.hpp"
#include "object_exec.hpp"
#include "object_rel.hpp"
#include "process.hpp"
#include "utils.hpp"
#include "generic.hpp"

std::vector<Object *> Object::objects;
std::vector<std::string> Object::library_path_runtime, Object::library_path_config, Object::library_path_default = { "/lib", "/usr/lib" };

Object * Object::load_library(const std::string & file, const std::vector<std::string> & search, DL::Lmid_t ns) {
	for (auto & dir : search) {
		std::filesystem::path path = std::string(dir) + "/" + std::string(file);
		LOG_DEBUG << "Checking " << path << "...";
		if (std::filesystem::exists(path))
			return load_file(std::filesystem::absolute(path), ns);
	}
	return nullptr;
}

Object * Object::load_library(const std::string & file, const std::vector<std::string> & rpath, const std::vector<std::string> & runpath, DL::Lmid_t ns) {
	Object * lib = nullptr;
	if (runpath.empty()) {
		lib = load_library(file, rpath, ns);
	}
	if (lib == nullptr) {
		lib = load_library(file, library_path_runtime, ns);
	}
	if (lib == nullptr) {
		lib = load_library(file, runpath, ns);
	}
	if (lib == nullptr) {
		lib = load_library(file, library_path_config, ns);
	}
	if (lib == nullptr) {
		lib = load_library(file, library_path_default, ns);
	}
	if (lib == nullptr) {
		LOG_ERROR << "Library '" << lib << "' cannot be found";
	}

	return lib;
}

Object * Object::load_file(const std::string & path, DL::Lmid_t ns) {
	for (auto & obj : objects)
		if (obj->path == path) {  // TODO: Use Inode
			LOG_DEBUG << "Already loaded " << path << "...";
			return obj;
		}

	LOG_DEBUG << "Loading " << path << "...";
	int fd = -1;
	size_t length = 0;
	void * addr = Utils::mmap_file(path.c_str(), fd, length);
	if (addr == nullptr) {
		LOG_ERROR << "Unable to load " << path << "!";
		return nullptr;
	}

	// Check ELF
	Elf::Header * header = reinterpret_cast<Elf::Header *>(addr);

	if (length < sizeof(Elf::Header) || !header->valid()) {
		LOG_ERROR << "No valid ELF identification header in " << path << "!";
		return nullptr;
	}

	switch (header->ident_abi()) {
		case Elf::ELFOSABI_NONE:
		case Elf::ELFOSABI_LINUX:
			break;
		default:
			LOG_ERROR << "Unsupported OS ABI " << (int)header->ident_abi();
			return nullptr;
	}

	switch (header->machine()) {
#ifdef __i386__
		case Elf::EM_386:
		case Elf::EM_486:
			break;
#endif
#ifdef __x86_64__
		case Elf::EM_X86_64:
			break;
#endif
		default:
			LOG_ERROR << "Unsupported machine!" ;
			return nullptr;
	}

	Object * o = nullptr;
	switch (header->type()) {
		case Elf::ET_EXEC:
			assert(objects.empty());
			o = new ObjectExecutable(path, fd, addr);
			break;
		case Elf::ET_DYN:
			o = new ObjectDynamic(path, fd, addr);
			break;
		case Elf::ET_REL:
			o = new ObjectRelocatable(path, fd, addr);
			break;
		default:
			LOG_ERROR << "Unsupported ELF type!";
			return nullptr;
	}
	if (o == nullptr) {
		LOG_ERROR << "Object is a nullptr";
		return nullptr;
	}

	// Add to global list
	objects.push_back(o);

	if (o->preload()) {
		LOG_INFO << "Successfully loaded " << path;
		return o;
	} else {
		LOG_ERROR << "Loading of " << path << " failed...";
		return nullptr;
	}
}

void Object::unload_all() {
	for (auto & obj : objects)
		delete obj;
}

Object::Object(std::string path, int fd, void * mem) : path(path), fd(fd), elf(reinterpret_cast<uintptr_t>(mem)) {}

Object::~Object() {
	for (auto & seg : memory_map)
		seg.unmap();

	close(fd);
}

std::string Object::file_name() const {
	return std::filesystem::path(path).filename();
}

bool Object::memory_range(uintptr_t & start, uintptr_t & end) const {
	if (memory_map.size() > 0) {
		start = memory_map.front().target.page_start();
		end = memory_map.back().target.page_end();
		return true;
	} else {
		return false;
	}
}

bool Object::allocate() {
	for (auto & seg : memory_map)
		if (!seg.map())
			return false;

	return true;
}


bool Object::protect() {
	for (auto & seg : memory_map)
		if (!seg.protect())
			return false;

	return true;
}


bool Object::run(std::vector<std::string> args, uintptr_t stack_pointer, size_t stack_size) {
	if (memory_map.size() == 0)
		return false;

	// Allocate
	LOG_DEBUG << "Allocate memory";
	for (auto & obj : objects)
		if (!obj->allocate())
			return false;

	// Relocate
	LOG_DEBUG << "Relocate";
	for (auto & obj : objects)
		if (!obj->relocate())
			return false;

	// Protect
	LOG_DEBUG << "Protect memory";
	for (auto & obj : objects)
		if (!obj->protect())
			return false;

	// Init (not binary itself)
	for (auto & obj : objects)
		if (obj == this)
			continue;
		else if (!obj->init())
			return false;

	// Prepare binary start
	Process p(stack_pointer, stack_size);

	// TODO: Should not be hard coded...
	uintptr_t base = memory_map[0].target.base;
	p.aux[Auxiliary::AT_PHDR] = base + elf.header.e_phoff;
	p.aux[Auxiliary::AT_PHNUM] = elf.header.e_phnum;

	args.insert(args.begin(), 1, path);
	p.init(args);

	uintptr_t entry = elf.header.entry();
	LOG_INFO << "Start at " << (void*)base << " + " << (void*)(entry);
	p.start(base + entry);
	return true;
}

/*
Symbol Object::get_symbol(const std::string & name, bool only_defined) const {
	Symbol sym;
	for (auto & table : symbol_tables) {
		Symbol tmp(table, name);
		if (tmp.is_valid() && (only_defined == false || tmp.is_defined()) && (!sym.is_valid() || tmp.bind == STB_GLOBAL || (tmp.bind == STB_WEAK && sym.bind == STB_LOCAL)))
			sym = tmp;
	}
	return sym;
}
*/
uintptr_t Object::next_address() {
	uintptr_t next = 0;
	for (auto & obj : objects) {
		uintptr_t start = 0, end = 0;
		if (obj->memory_range(start, end) && end > next) {
			next = end;
		}
	}
	// Default address
	if (next == 0) {
		next = 0x500000;
	}
	return next;
}
