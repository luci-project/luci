// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "object/identity.hpp"

#include <dlh/log.hpp>
#include <dlh/file.hpp>
#include <dlh/string.hpp>
#include <dlh/xxhash.hpp>
#include <dlh/utility.hpp>
#include <dlh/syscall.hpp>
#include <dlh/datetime.hpp>
#include <dlh/stream/output.hpp>

#include <bean/helper/debug_sym.hpp>

#include "object/base.hpp"
#include "object/dynamic.hpp"
#include "object/executable.hpp"
#include "object/relocatable.hpp"
#include "comp/glibc/patch.hpp"

#include "loader.hpp"

static bool supported(const Elf::Header * header) {
	if (!header->valid()) {
		LOG_ERROR << "No valid ELF identification header!" << endl;
		return false;
	}

	switch (header->ident_abi()) {
		case Elf::ELFOSABI_NONE:
		case Elf::ELFOSABI_LINUX:
			break;

		default:
			LOG_ERROR << "Unsupported OS ABI " << static_cast<int>(header->ident_abi()) << endl;
			return false;
	}

	// Supported architecture?
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
			LOG_ERROR << "Unsupported machine!" << endl;
			return false;
	}

	// supported
	return true;
}


Object * ObjectIdentity::load(uintptr_t addr, Elf::ehdr_type type) {
	Object::Data data;
	Object * object = nullptr;
	// Open...
	enum Info info = open(addr, data, type);
	if (info == INFO_CONTINUE_LOAD) {
		// ... and create object
		create(data, type).assign(object, info);
		// Clean up on failure
		if (object == nullptr && data.fd != -1) {
			if (flags.premapped == 0)
				Syscall::munmap(data.addr, data.size);
			Syscall::close(data.fd);
		} else if (!watch(flags.executed_binary, flags.executed_binary)) {
			info = INFO_ERROR_INOTIFY;
		}
	}

	status(info);

	return object;
}


ObjectIdentity::Info ObjectIdentity::open(uintptr_t addr, Object::Data & data, Elf::ehdr_type & type) const {
	// Not updatable: Allow only one (current) version
	if (!flags.updatable && current != nullptr) {
		LOG_WARNING << "Cannot load new version of " << *this << " - updates are not allowed!" << endl;
		return INFO_UPDATE_DISABLED;
	}

	LOG_DEBUG << "Loading " << name << "..." << endl;

	if (addr == 0) {
		assert(!path.empty());
		assert(flags.premapped == 0);

		// Open file
		if (auto open = Syscall::open(path.str, O_RDONLY)) {
			data.fd = open.value();
		} else {
			LOG_VERBOSE << "Opening " << *this << " failed: " << open.error_message() << endl;
			return INFO_ERROR_OPEN;
		}

		// Determine file size and inode
		if (struct stat sb; auto fstat = Syscall::fstat(data.fd, &sb)) {
			data.modification_time = sb.st_mtim;
			data.size = sb.st_size;
		} else {
			LOG_ERROR << "Stat file " << *this << " failed: " << fstat.error_message() << endl;
			Syscall::close(data.fd);
			return INFO_ERROR_STAT;
		}

		// Check if already loaded (using modification time)
		if (loader.config.use_mtime && !flags.ignore_mtime && flags.skip_identical) {
			for (Object * obj = current; obj != nullptr; obj = obj->file_previous) {
				if (obj->data.modification_time.tv_sec == data.modification_time.tv_sec && obj->data.modification_time.tv_nsec == data.modification_time.tv_nsec && obj->data.size == data.size) {
					LOG_INFO << "Already loaded " << *this << " with same modification time -- abort loading..." << endl;
					Syscall::close(data.fd);
					return INFO_IDENTICAL_TIME;
				}
			}
		}

		// Map file
		if (auto mmap = Syscall::mmap(NULL, data.size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, data.fd, 0)) {
			if (flags.immutable_source) {
				data.addr = mmap.value();
			} else {
				// It seems like a populated privated mapping is not enough if the underlying file is changed (= undefined behaviour in linux).
				// So we have to copy the content into an anonymous, non-file backed memory
				if (auto anon = Syscall::mmap(NULL, data.size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) {
					LOG_INFO << "Creating a full in-memory copy of " << *this << endl;
					data.addr = Memory::copy(anon.value(), mmap.value(), data.size);
					// cleaning up
					Syscall::munmap(mmap.value(), data.size);
					Syscall::close(data.fd);
					data.fd = -1;
				} else {
					LOG_ERROR << "Mapping anonymous memory for " << *this << " failed: " << anon.error_message() << endl;
					Syscall::close(data.fd);
					return INFO_ERROR_MAP;
				}
			}
		} else {
			LOG_ERROR << "Mapping " << *this << " failed: " << mmap.error_message() << endl;
			Syscall::close(data.fd);
			return INFO_ERROR_MAP;
		}
	} else {
		data.addr = addr;
		data.size = Elf(addr).size(true);
	}

	// Check ELF
	const Elf::Header * header = reinterpret_cast<const Elf::Header *>(data.addr);
	if (data.size < sizeof(Elf::Header) || !supported(header)) {
		LOG_ERROR << "Unsupported ELF header in " << *this << "!" << endl;
		if (flags.premapped == 0)
			Syscall::munmap(data.addr, data.size);
		Syscall::close(data.fd);
		return INFO_ERROR_ELF;
	} else if (type == Elf::ET_NONE) {
		type = header->type();
	}

	// Adjust permission if required
	if (data.addr != 0 && flags.premapped == 0 && ((flags.immutable_source && type == Elf::ET_REL) || (!flags.immutable_source && type != Elf::ET_REL))) {
		// Keep it writable for relocatable Objects only
		if (auto mprotect = Syscall::mprotect(data.addr, data.size, PROT_READ | (type == Elf::ET_REL ? PROT_WRITE : 0)); mprotect.failed())
			LOG_WARNING << "Unable to adjust protection of target memory: " << mprotect.error_message() << endl;
	}

	return INFO_CONTINUE_LOAD;
}


bool ObjectIdentity::watch(bool force, bool close_existing) {
	if (flags.updatable == 1 && (wd == -1 || force)) {
		if (wd != -1 && close_existing) {
			if (auto inotify = Syscall::inotify_rm_watch(loader.filemodification_inotifyfd, wd)) {
				LOG_DEBUG << "Remove old watch for modifications at " << this->path << endl;
			} else if (force) {
				LOG_TRACE << "Cannot remove old watch for modification of " << this->path << " (" << inotify.error_message() << "), however we have expected this in force mode... (ignored)" << endl;
			} else  {
				LOG_WARNING << "Cannot remove old watch for modification of " << this->path << ": " << inotify.error_message() << endl;
			}
		}
		if (auto inotify = Syscall::inotify_add_watch(loader.filemodification_inotifyfd, this->path.str, IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF | IN_DONT_FOLLOW | (flags.executed_binary ? IN_ATTRIB : 0))) {
			LOG_DEBUG << "Watching for modifications at " << this->path << endl;
			wd = inotify.value();
		} else {
			LOG_INFO << "Cannot watch for modification of " << this->path << ": " << inotify.error_message() << endl;
			wd = -1;
		}
	}
	return wd != -1;
}

ObjectIdentity::ObjectIdentity(Loader & loader, const Flags flags, const char * path, namespace_t ns, const char * altname) : ns(ns), loader(loader), name(altname), flags(flags), wd(-1) {
	assert(ns != NAMESPACE_NEW);

	// Dynamic updates?
	if (!loader.config.dynamic_update) {
		this->flags.updatable = 0;
		this->flags.immutable_source = 1;  // Without updates, don't expect changes to the binaries during runtime
	} else if (this->flags.updatable == 1) {
		this->flags.update_outdated |= loader.config.update_outdated_relocations;
		this->flags.skip_identical = loader.config.skip_identical;
		assert(this->flags.initialized == 0 && this->flags.premapped == 0);
	}

	// File based?
	if (path == nullptr) {
		buffer[0] = '\0';

		libname_buffer[0].name = buffer;
		libname_buffer[0].next = nullptr;
		libname_buffer[0].dont_free = 1;
		libname = libname_buffer;
	} else {
		// We need the absolute path to the directory (GLIBC requirement...)
		auto pathlen = String::len(path) + 1;
		char tmp[pathlen];  // NOLINT
		String::copy(tmp, path, pathlen);
		char * tmpfilename = const_cast<char*>(String::find_last(tmp, '/'));
		size_t bufferlen;
		bool success;
		if (tmpfilename == nullptr) {
			success = File::absolute(".", buffer, PATH_MAX, bufferlen);
			tmpfilename = tmp;
		} else {
			*(tmpfilename++) = '\0';
			success = File::absolute(tmp, buffer, PATH_MAX, bufferlen);
		}
		if (success && File::exists(tmpfilename)) {
			if (bufferlen > 0)
				buffer[bufferlen++] = '/';
			String::copy(buffer + bufferlen, tmpfilename, PATH_MAX - bufferlen);
		} else {
			String::copy(buffer, path, PATH_MAX);
		}
		this->path = StrPtr(buffer);
		if (altname == nullptr)
			this->name = this->path.find_last('/');

		libname_buffer[0].name = this->path.c_str();
		libname_buffer[0].next = libname_buffer + 1;
		libname_buffer[0].dont_free = 1;
		libname_buffer[1].name = this->name.c_str();
		libname_buffer[1].next = nullptr;
		libname_buffer[1].dont_free = 1;
		libname = libname_buffer;

		// Observe file?
		if (this->flags.updatable == 1) {
			struct stat sb;
			auto lstat = Syscall::lstat(this->path.str, &sb);
			if (lstat.failed()) {
				LOG_ERROR << "Lstat of " << path << " failed: " << lstat.error_message() << endl;
			} else if (S_ISLNK(sb.st_mode)) {
				LOG_DEBUG << "Library " << this->path << " is a symbolic link (hence we do not expect changes to the binary itself)" << endl;
				this->flags.immutable_source = 1;
			}
		}
	}
	// GLIBC related
	filename = buffer;
}


ObjectIdentity::~ObjectIdentity() {
	if (loader.config.dynamic_update) {
		if (auto inotify = Syscall::inotify_rm_watch(loader.filemodification_inotifyfd, wd); inotify.failed())
			LOG_ERROR << "Removing watch for " << this->path << " failed: " << inotify.error_message() << endl;
	}
	// Delete all versions
	while (current != nullptr) {
		Object * prev = current->file_previous;
		delete current;
		current = prev;
	}
}


Pair<Object *, ObjectIdentity::Info> ObjectIdentity::create(Object::Data & data, Elf::ehdr_type type) {
	// Hash file contents
	if (flags.updatable && flags.skip_identical) {
		XXHash64 datahash(name.hash);  // Name hash as seed
		datahash.add(data.addr, data.size);
		data.hash = datahash.hash();
		LOG_DEBUG << "Elf " << *this << " has hash " << hex << data.hash << dec << endl;

		// Check if already loaded (using hash)
		for (Object * obj = current; obj != nullptr; obj = obj->file_previous)
			if (obj->data.hash == data.hash && obj->data.size == data.size) {
				LOG_INFO << "Already loaded " << name << " with same hash -- abort loading..." << endl;
				return { nullptr, INFO_IDENTICAL_HASH };
			}
	}

	/* TODO:
	 * 1. Load file
	 * 2. hash & bean (if required)
	 * 3. Map (load or copy) to target memory if EXEC or DYN && current = 0
	 * 4. Create object
	 */

	/*
	// Temporary Elf object
	Elf tmp(data.addr);
	uintptr_t base = 0;
	switch (type) {
		case Elf::ET_DYN:
			base = file.loader.next_address();
			[[fallthrough]];
		case Elf::ET_EXEC:
			// load segments
			for (const auto & segment : tmp.segments)
				if (Elf::PT_LOAD == segment.type() && segment.virt_size() > 0)
					memory_map.emplace_back(tmp, segment, base);

	*/

	// Create object
	Object * o = nullptr;
	switch (type) {
		case Elf::ET_EXEC:
			// Take care for dynamic linked executables:
			for (const auto &s : Elf(data.addr).segments)
				if (s.type() == Elf::PT_DYNAMIC && (o = new ObjectDynamic{*this, data, false}) != nullptr) {
					LOG_DEBUG << "Executable " << *this << " has dynamic section" << endl;
					break;
				}
			if (o == nullptr) {
				o = new ObjectExecutable{*this, data};
			}
			break;
		case Elf::ET_DYN:
			o = new ObjectDynamic{*this, data, true};
			break;
		case Elf::ET_REL:
			o = new ObjectRelocatable{*this, data};
			break;
		default:
			LOG_ERROR << "Unsupported ELF type!" << endl;
			return { nullptr, INFO_ERROR_ELF };
	}
	if (o == nullptr) {
		LOG_ERROR << "unable to create object" << endl;
		return { nullptr, INFO_ERROR_CREATE };
	}
	assert(o->valid(data.size, true));
	o->file_previous = current;

	// If dynamic updates are enabled, compare hashes
	if (flags.updatable == 1 && type != Elf::ET_REL) {
		// Debug symbols
		if (loader.config.find_debug_symbols) {
			DebugSymbol dbgsym{path.c_str(), loader.config.debug_symbols_root};
			const char * debug_link = DebugSymbol::link(*o);
			const char * debug_symbol_path = dbgsym.find(debug_link, o->build_id);
			if (debug_symbol_path != nullptr) {
				void * debug_data = File::contents::get(debug_symbol_path, o->debug_size);
				if (debug_data != nullptr) {
					o->debug_symbols = new Elf(debug_data);
					if (o->debug_symbols != nullptr && o->debug_symbols->valid(o->debug_size)) {
						LOG_INFO << "Loaded external debug symbols at " << debug_symbol_path << " for " << *o << endl;
					} else {
						LOG_WARNING << "Loading external debug symbols at " << debug_symbol_path << " for " << *o << " failed!" << endl;
						delete(o->debug_symbols);
						o->debug_symbols = nullptr;
						if (auto unmap = Syscall::munmap(reinterpret_cast<uintptr_t>(debug_data), o->debug_size); unmap.failed())
							LOG_WARNING << "Unmapping debug symbols data at " << debug_data << " in " << *o << " failed: " << unmap.error_message() << endl;
						o->debug_size = 0;
					}
				} else {
					LOG_WARNING << "Opening external debug symbols at " << debug_symbol_path << " for " << *o << " failed!" << endl;
					o->debug_size = 0;
				}
			} else {
				LOG_DEBUG << "No external debug symbols for " << *o << " found!" << endl;
				o->debug_size = 0;
			}
		}
		LOG_INFO << "Calculate Binary hash of " << *o << endl;
		uint32_t bean_flags = Bean::FLAG_NONE;
		// Resolve internal relocations to improve patchable detection
		bean_flags |= Bean::FLAG_RESOLVE_INTERNAL_RELOCATIONS;
		if (loader.config.update_mode >= Loader::Config::UPDATE_MODE_CODEREL) {
			bean_flags |= Bean::FLAG_RECONSTRUCT_RELOCATIONS;
			bean_flags |= Bean::FLAG_HASH_ATTRIBUTES_FOR_ID;
		}

		o->binary_hash.emplace(*o, o->debug_symbols, bean_flags);
		// if previous version exist, check if we can patch it
		if (current != nullptr) {
			assert(current->binary_hash && o->binary_hash);
			if (!o->patchable()) {
				LOG_WARNING << "Got new version of " << path << ", however, it is incompatible with current version and hence cannot be employed..." << endl;
				delete o;
				return { nullptr, INFO_UPDATE_INCOMPATIBLE };
			}
		}

		// Query for DWARF hash, if corresponding socket is connected)
		if (!loader.config.force_update && current != nullptr && o->query_debug_hash() != nullptr && current->query_debug_hash() != nullptr) {
			if (String::compare(o->debug_hash, current->debug_hash) == 0) {
				LOG_INFO << "DWARF hash (" << o->debug_hash << ") is identical" << endl;
			} else {
				LOG_WARNING << "Got new version of " << path << ", however, according to its DWARF (" << o->debug_hash << ") it seems to be incompatible with current version (" << current->debug_hash << ") and hence cannot be employed..." << endl;
				delete o;
				return { nullptr, INFO_UPDATE_INCOMPATIBLE };
			}
		}
	}
	// Add to list
	current = o;

	// perform preload
	if (!o->preload()) {
		LOG_ERROR << "Loading of " << path << " failed (while preloading)..." << endl;
		current = o->file_previous;
		delete o;
		return { nullptr, INFO_FAILED_PRELOADING };
	}

	// Map memory
	if (flags.premapped == 0 && !o->map()) {
		LOG_ERROR << "Loading of " << path << " failed (while mapping into memory)..." << endl;
		current = o->file_previous;
		delete o;
		return { nullptr, INFO_FAILED_MAPPING };
	}

	// Apply (Luci specific) fixes
	if (!o->fix()) {
		LOG_ERROR << "Applying fixes at " << *this << " failed!" << endl;
	}

	// Prepare
	if (flags.initialized == 1) {
		if (o->file_previous != nullptr) {
			LOG_INFO << "Prepare new version of " << path << endl;
			// Lazy evaluation is not possible for updated files!
			flags.bind_now = 1;
			// Fix relocations in dynamic objects
			if (!o->prepare()) {
				LOG_WARNING << "Preparing updated object " << path << " failed!" << endl;
			}
		} else {
			assert(type == Elf::ET_EXEC || o->base == data.addr);
		}
		o->status = Object::STATUS_PREPARED;
	}

	LOG_INFO << "Successfully loaded " << path << " v" << o->version();
	if (o->build_id.available())
		LOG_INFO_APPEND << " (Build ID " << o->build_id.value << ")";
	LOG_INFO_APPEND << " with base " << reinterpret_cast<void*>(o->base) << endl;

	// Initialize GLIBC specific stuff
	base = o->base;
	dynamic = o->dynamic_address();

	return { o, o->file_previous == nullptr ? INFO_SUCCESS_LOAD : INFO_SUCCESS_UPDATE };
}


bool ObjectIdentity::prepare() const {  // NOLINT
	assert(current != nullptr);
	switch (current->status) {
		case Object::STATUS_MAPPED:
			current->status = Object::STATUS_PREPARING;

			// First initialize all dependencies
			for (auto & dep : current->dependencies)
				if (!dep->prepare())
					return false;

			LOG_DEBUG << "Preparing " << *current << endl;
			if (current->prepare())
				current->status = Object::STATUS_PREPARED;
			else
				return false;
			break;

		case Object::STATUS_PREPARING:
			LOG_WARNING << "Circular dependency on " << *current << endl;
			break;

		case Object::STATUS_PREPARED:
			break;
	}
	return true;
}


bool ObjectIdentity::update() const {
	bool success = true;
	for (Object * c = current; c != nullptr; c = c->file_previous) {
		LOG_DEBUG << "Updating relocations at " << *c << endl;
		success &= c->update();
		if (!flags.update_outdated)
			break;
	}
	return success;
}


bool ObjectIdentity::finalize() const {
	bool success = true;
	for (Object * c = current; c != nullptr; c = c->file_previous) {
		LOG_DEBUG << "Finalizing " << *c << endl;
		success &= c->finalize();
	}
	return success;
}


bool ObjectIdentity::initialize() {  // NOLINT
	assert(current != nullptr);
	if (flags.initialized == 0) {
		flags.initialized = 1;

		// First initialize all dependencies
		for (auto & dep : current->dependencies)
			if (!dep->initialize())
				return false;

		LOG_DEBUG << "Initializing " << *current << endl;
		if (!current->initialize())
			return false;
	}
	return true;
}


void ObjectIdentity::status(ObjectIdentity::Info msg) const {
	if (loader.statusinfofd >= 0 && (loader.config.early_statusinfo || loader.target != nullptr)) {
		OutputStream<512> out(loader.statusinfofd);
		switch (msg) {
			case INFO_ERROR_OPEN:          out << "ERROR (opening file failed)"; break;
			case INFO_ERROR_STAT:          out << "ERROR (retrieving file status failed)"; break;
			case INFO_ERROR_MAP:           out << "ERROR (mapping whole file into memory failed)"; break;
			case INFO_ERROR_CREATE:        out << "ERROR (not able to create object)"; break;
			case INFO_ERROR_ELF:           out << "ERROR (unsupported format)"; break;
			case INFO_ERROR_INOTIFY:       out << "ERROR (not able to watch for file modifications)"; break;
			case INFO_IDENTICAL_TIME:      out << "IGNORED (new version has same modification time)"; break;
			case INFO_IDENTICAL_HASH:      out << "IGNORED (new version has same hash)"; break;
			case INFO_UPDATE_DISABLED:     out << "FAILED (dynamic updates are disabled)"; break;
			case INFO_UPDATE_INCOMPATIBLE: out << "FAILED (new version is incompatible)"; break;
			case INFO_UPDATE_MODIFIED:     out << "FAILED (relocated data was altered)"; break;
			case INFO_FAILED_PRELOADING:   out << "FAILED (preload was unsuccessful)"; break;
			case INFO_FAILED_MAPPING:      out << "FAILED (mapping of segments was unsuccessful)"; break;
			case INFO_FAILED_REUSE:        out << "FAILED (reusing outdated code)"; break;
			case INFO_CONTINUE_LOAD:       out << "ERROR (open was successful, but not continued load)"; break;
			case INFO_SUCCESS_LOAD:        out << "SUCCESS (loaded initial version)"; break;
			case INFO_SUCCESS_UPDATE:      out << "SUCCESS (updated to new version)"; break;
			default:                       out << "ERROR (invalid info type)"; break;
		}
		out << " for " << name << " [" << path << "] in PID " << Syscall::getpid() << " at " << DateTime::now() << endl;
	}
}
