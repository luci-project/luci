#include "object/base.hpp"

#include <dlh/syscall.hpp>
#include <dlh/auxiliary.hpp>
#include <dlh/utility.hpp>
#include <dlh/file.hpp>
#include <dlh/log.hpp>

#include "object/identity.hpp"
#include "object/dynamic.hpp"
#include "object/executable.hpp"
#include "object/relocatable.hpp"

#include "loader.hpp"


Object::Object(ObjectIdentity & file, const Data & data) : Elf(data.addr), file(file), data(data), build_id(file.flags.updatable ? this : nullptr) {
	assert(data.addr != 0);
}

Object::~Object() {
	// TODO: Not really supported yet, just a stub...

	if (debug_hash != nullptr)
		Memory::free(debug_hash);

	// Remove this version from list
	if (file.current == this)
		file.current = file_previous;
	else
		for (auto tmp = file.current; tmp != nullptr; tmp = tmp->file_previous)
			if (tmp->file_previous == this) {
				tmp->file_previous = file_previous;
				break;
			}

	// Unmap virt mem
	for (auto & seg : memory_map)
		seg.unmap();

	// Reset adress checker
	file.loader.reset_address(base);

	if (auto unmap = Syscall::munmap(data.addr, data.size); unmap.failed()) {
		LOG_ERROR << "Unmapping data from " << *this << " failed: " << unmap.error_message() << endl;
	}

	// close file descriptor
	if (data.fd > 0)
		Syscall::close(data.fd);

	// TODO: unmap file.data?
}

const char * Object::query_debug_hash() {
	// Query for DWARF hash, if corresponding socket is connected)
	if (debug_hash == nullptr) {
		auto & socket = file.loader.debug_hash_socket;
		if (socket.is_connected()) {
			char tmp[128];

			// First query for Build ID
			if (build_id.available()) {
				LOG_INFO << "Quering for debug hash by build id: " << build_id.value << endl;
				size_t sent = socket.send(build_id.value, count(build_id.value));
				if (sent != count(build_id.value)) {
					LOG_WARNING << "Debug hash socket sent " << sent << " (instead of " << sizeof(build_id.value) << ") bytes" << endl;
				} else {
					size_t recv = socket.recv(tmp, count(tmp), true);
					LOG_DEBUG << "Debug hash for build id " << build_id.value << " is " << tmp << endl;
					if (recv > 0 && tmp[0] != '-' && tmp[1] != '\0') {
						return debug_hash = String::duplicate(tmp, recv);
					}
				}
			}

			// Then try path
			if (!file.path.empty()) {
				LOG_INFO << "Quering for debug hash by path: " << file.path << endl;
				size_t sent = socket.send(file.path.str, file.path.len + 1);
				if (sent != file.path.len + 1) {
					LOG_WARNING << "Debug hash socket sent " << sent << " (instead of " << file.path.len << ") bytes" << endl;
				} else {
					size_t recv = socket.recv(tmp, count(tmp), true);
					LOG_DEBUG << "Debug hash for path " << file.path  << " is " << tmp << endl;
					if (recv > 0 && tmp[0] != '-' && tmp[1] != '\0')
						return debug_hash = String::duplicate(tmp, recv);
				}
			}
		}
	}
	return debug_hash;
}

uintptr_t Object::dynamic_address() const {
	for (const auto & segment : this->segments)
		if (segment.type() == Elf::PT_DYNAMIC)
			return (this->header.type() == Elf::ET_EXEC ? 0 : base) + segment.virt_addr();
	return 0;
}

bool Object::is_latest_version() const {
	return this == file.current;
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

bool Object::map() {
	bool success = true;
	for (auto & seg : memory_map)
		success &= seg.map();
	return success;
}

bool Object::finalize() const {
	bool success = true;
	for (auto & seg : memory_map)
		success &= seg.finalize();
	return success;
}

bool Object::disable() const {
	switch (file.loader.config.detect_outdated) {
		case Loader::Config::DETECT_OUTDATED_DISABLED:
			return false;

		case Loader::Config::DETECT_OUTDATED_VIA_USERFAULTFD:
		{
			bool success = true;
			for (auto & seg : memory_map)
				if ((seg.target.protection & PROT_EXEC) != 0)
					success &= seg.disable();
			return success;
		}

		case Loader::Config::DETECT_OUTDATED_VIA_UPROBES:
		case Loader::Config::DETECT_OUTDATED_WITH_DEPS_VIA_UPROBES:
		{
			if (!file.current->binary_hash) {
				LOG_WARNING << *(file.current) << " has no binary hash, hence no uprobe detection possible!" << endl;
				return false;
			} else if (!this->binary_hash) {
				LOG_WARNING << *this << " has no binary hash, hence no uprobe detection possible!" << endl;
				return false;
			} else if (data.fd < 0) {
				LOG_WARNING << *this << " is not loaded from a file!" << endl;
				return false;
			}

			// TODO: fix this assumption
			assert(file.current->file_previous == this);

			char path[PATH_MAX + 1];
			if (!File::absolute(data.fd, path, PATH_MAX)) {
				LOG_WARNING << "Unable to get absolute path of file for " << *this << "!" << endl;
				return false;
			}

			//File::contents::set("/sys/kernel/debug/tracing/events/uprobes/enable", "0");
			if (auto fd = Syscall::open("/sys/kernel/debug/tracing/uprobe_events", O_WRONLY | O_APPEND)) {
				OutputStream<1024> uprobe_events(fd.value());
				char name[65];
				BufferStream name_stream(name, 65);
				const auto diff = binary_hash->diff(*(file.current->binary_hash), file.loader.config.detect_outdated == Loader::Config::DETECT_OUTDATED_WITH_DEPS_VIA_UPROBES, static_cast<Bean::ComparisonMode>(file.loader.config.relax_comparison));
				size_t uprobes = 0;
				for (const auto & d : diff) {
					if (d.section.executable) {
						name_stream.clear();
						// set name
						name_stream << file.name << "_v" << dec << version() << '_';
						if (d.name != nullptr)
							name_stream << d.name;
						else
							continue;  // name_stream << "0x" << hex << d.address; -- but results to Error 524
						name_stream.str();
						// sanitize name
						for (size_t i = 0; i < 65 ; i++)
							if (name[i] == '\0')
								break;
							else if ((name[i] < 'a' || name[i] > 'z') && (name[i] < 'A' || name[i] > 'Z') && (name[i] < '0' || name[i] > '9'))
								name[i] = '_';

						// write to uprove
						uprobe_events << "p:" << name << ' ' << path << ":0x" << hex << d.address << endl;
						uprobe_events.flush();
						uprobes++;
						LOG_DEBUG << "adding uprobe 'p:" << name << ' ' << path << ":0x" << hex << d.address << '\'' << endl;
					}
				}

				Syscall::close(fd.value());
				if (uprobes > 0) {
					File::contents::set("/sys/kernel/debug/tracing/events/uprobes/enable", "1");
					File::contents::set("/sys/kernel/debug/tracing/tracing_on", "1");
					LOG_INFO << "Installed " << uprobes << " uprobes for " << *this << endl;
				}
				return true;
			} else {
				LOG_ERROR << "Opening /sys/kernel/debug/tracing/uprobe_events for " << file.name << " failed: " << fd.error_message() << endl;
				return false;
			}
		}

		case Loader::Config::DETECT_OUTDATED_VIA_PTRACE:
			LOG_ERROR << "Ptrace outdated detection not implemented yet" << endl;
			return false;

		default:
			LOG_ERROR << "Invalid disable outdated mode " << static_cast<int>(file.loader.config.detect_outdated) << endl;
			return false;
	}
}

size_t Object::version() const {
	size_t v = 0;
	for (Object * p = file_previous; p != nullptr; p = p->file_previous)
		v++;
	return v;
}


uintptr_t Object::tls_address(uintptr_t value) const {
	auto thread = Thread::self();
	assert(thread != nullptr);

	return file.loader.tls.get_addr(thread, file.tls_module_id) + file.tls_offset + value;
}


void* Object::dynamic_resolve(size_t index) const {
	LOG_ERROR << "Unable to resolve " << index << " -- Object " << file.path << " does not support dynamic loading!" << endl;
	assert(false);
	return nullptr;
}

bool Object::has_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version, Optional<VersionedSymbol> & result) const {
	auto tmp = resolve_symbol(name, hash, gnu_hash, version);
	if (tmp) {
		assert(tmp->valid());
		assert(tmp->bind() != Elf::STB_LOCAL); // should not be returned
		// Weak dynamic linkage is only taken into account, if file.loader.config.dynamic_weak is set. Otherwise it is always strong.
		bool strong = tmp->bind() != Elf::STB_WEAK || !file.loader.config.dynamic_weak;
		if (strong || !result) {
			result = tmp;
			return strong;
		}
	}
	return false;
}
