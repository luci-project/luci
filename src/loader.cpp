#include "loader.hpp"

#include <utility>

#include "xxhash64.h"

#include "process.hpp"
#include "object_dyn.hpp"
#include "object_rel.hpp"
#include "object_exec.hpp"

Loader::~Loader() {
	for (auto & obj : lookup)
		delete obj;
}

Object * Loader::library(const char * filename, const std::vector<const char *> & search, DL::Lmid_t ns) const {
	Object * lib;
	for (auto & dir : search)
		if ((lib = file(filename, dir, ns)) != nullptr)
			return lib;
	return nullptr;
}

Object * Loader::library(const char * filename, const std::vector<const char *> & rpath, const std::vector<const char *> & runpath, DL::Lmid_t ns)   const {
	Object * lib;
	for (const std::vector<const char *> & path : { rpath, library_path_runtime, runpath, library_path_config, library_path_default })
		if ((lib = library(filename, path, ns)) != nullptr)
			return lib;
	LOG_ERROR << "Library '" << filename << "' cannot be found";
	return nullptr;
}

Object * Loader::file(const char * filename, const char * directory, DL::Lmid_t ns) const {
	auto filename_len = strlen(filename);
	auto directory_len = strlen(directory);
	char path[directory_len + filename_len + 2];
	strncpy(path, directory, directory_len);
	path[directory_len] = '/';
	strncpy(path + directory_len + 1, filename, filename_len + 1);
	return file(path, ns);
}


Object * Loader::file(const char * filepath, DL::Lmid_t ns) const {
	// New namespace?
	if (ns == DL::LM_ID_NEWLN) {
		ns = DL::LM_ID_BASE + 1;
		for (const auto & o : lookup)
			if (o->file.ns >= ns)
				ns = o->file.ns + 1;
	}

	// Create file
	Object::File file(this, ns);

	// Get real path (if valid)
	errno = 0;
	file.path = realpath(filepath, nullptr);
	if (file.path == nullptr) {
		LOG_ERROR << "Opening file " << filepath << " failed: " << strerror(errno);
		return nullptr;
	}

	LOG_DEBUG << "Loading " << file.path << "...";
	// Open file
	errno = 0;
	file.fd = ::open(file.path, O_RDONLY);
	if (file.fd < 0) {
		LOG_ERROR << "Reading file " << file.path << " failed: " << strerror(errno);
		::free(const_cast<char*>(file.path));
		return nullptr;
	}

	// Determine file size and inode
	struct stat sb;
	if (::fstat(file.fd, &sb) == -1) {
		LOG_ERROR << "Stat file " << file.path << " failed: " << strerror(errno);
		::close(file.fd);
		::free(const_cast<char*>(file.path));
		return nullptr;
	}
	size_t length = sb.st_size;
	int inode = sb.st_ino;

	// Map file
	file.data = ::mmap(NULL, length, PROT_READ, MAP_PRIVATE, file.fd, 0);
	if (file.data == MAP_FAILED) {
		LOG_ERROR << "Mmap file " << file.path << " failed: " << strerror(errno);
		::close(file.fd);
		::free(const_cast<char*>(file.path));
		return nullptr;
	}

	// Hash file contents
	XXHash64 filehash(ELF_Def::hash(file.path));  // Path as seed
	filehash.add(file.data, length);
	file.hash = filehash.hash();
	LOG_DEBUG << "File " << file.path << " has hash " << std::hex << file.hash << std::dec;

	// Check if already loaded
	for (auto & obj : lookup)
		if (obj->file.hash == file.hash && strcmp(obj->file.path, file.path) == 0) {
			LOG_INFO << "Already loaded " << file.path << "...";
			::close(file.fd);
			::free(const_cast<char*>(file.path));
			return obj;
		}

	// Check ELF
	const Elf::Header * header = reinterpret_cast<const Elf::Header *>(file.data);

	if (length < sizeof(Elf::Header) || !header->valid()) {
		LOG_ERROR << "No valid ELF identification header in " << file.path << "!";
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

	// TODO copy in memory if required
	// calculate checksum with XXhash

	// Create object
	Object * o = nullptr;
	switch (header->type()) {
		case Elf::ET_EXEC:
			LOG_DEBUG << "Executable (standalone)";
			assert(lookup.empty());
			o = new ObjectExecutable(file);
			break;
		case Elf::ET_DYN:
			LOG_DEBUG << "Dynamic";
			o = new ObjectDynamic(file);
			break;
		case Elf::ET_REL:
			LOG_DEBUG << "Relocatable";
			o = new ObjectRelocatable(file);
			break;
		default:
			LOG_ERROR << "Unsupported ELF type!";
			return nullptr;
	}
	if (o == nullptr) {
		LOG_ERROR << "Object is a nullptr";
		::close(file.fd);
		::free(const_cast<char*>(file.path));
		return nullptr;
	}

	// Add to lookup list
	lookup.push_back(o);

	// perform preload
	if (o->preload()) {
		LOG_INFO << "Successfully loaded " << file.path << " at " << (void*)o ;
		return o;
	} else {
		LOG_ERROR << "Loading of " << file.path << " failed (while preloading)...";
		delete o;
		return nullptr;
	}
}

bool Loader::run(const Object * start, std::vector<const char *> args, uintptr_t stack_pointer, size_t stack_size) const {
	if (!valid(start))
		return false;

	if (start->memory_map.size() == 0)
		return false;

	// Allocate
	LOG_DEBUG << "Allocate memory";
	for (auto & obj : lookup)
		if (!obj->run_allocate())
			return false;

	// Relocate
	LOG_DEBUG << "Relocate";
	for (auto & obj : lookup)
		if (!obj->run_relocate())
			return false;

	// Protect
	LOG_DEBUG << "Protect memory";
	for (auto & obj : lookup)
		if (!obj->run_protect())
			return false;

	// Init (not binary itself)
	for (auto & obj : lookup)
		if (obj == start)
			continue;
		else if (!obj->run_init())
			return false;

	// Prepare execution
	Process p(stack_pointer, stack_size);

	// TODO: Should not be hard coded...
	p.aux[Auxiliary::AT_PHDR] = start->base + start->elf.header.e_phoff;
	p.aux[Auxiliary::AT_PHNUM] = start->elf.header.e_phnum;

	args.insert(args.begin(), 1, start->file.path);
	p.init(args);

	uintptr_t entry = start->elf.header.entry();
	LOG_INFO << "Start at " << (void*)start->base << " + " << (void*)(entry);
	p.start(start->base + entry);

	return true;
}

bool Loader::valid(const Object & o) const {
	return valid(&o);
}

bool Loader::valid(const Object * o) const {
	if (o != nullptr)
		for (const auto & obj : lookup)
			if (obj == o)
				return true;
	return false;
}


std::optional<Symbol> Loader::resolve_symbol(const Symbol & sym, DL::Lmid_t ns) const {
	for (const auto & o : lookup) {
		if (o->file.ns == ns) {
			auto s = o->resolve_symbol(sym);
			if (s) {
				assert(s->valid());
				return s;
			}
		}
	}
	return std::nullopt;
}

uintptr_t Loader::next_address() const {
	uintptr_t next = 0;
	for (auto obj : lookup) {
		LOG_DEBUG << "obj = " << (void*)obj;
		uintptr_t start = 0, end = 0;
		assert(valid(obj));
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
