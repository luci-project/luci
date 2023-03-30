#include "symbol.hpp"

#include "object/identity.hpp"
#include "object/base.hpp"

VersionedSymbol::VersionedSymbol(const Elf::Symbol & sym, const char * version_name, bool version_weak, const char * version_file)
 : Elf::Symbol(sym), version(version_name, version_weak, version_file) {
	assert(sym.valid());
}

VersionedSymbol::VersionedSymbol(const Elf::Symbol & sym, const Version & version, uint32_t hash, uint32_t gnu_hash)
 : Elf::Symbol(sym), version(version), _hash_value(hash), _gnu_hash_value(gnu_hash) {
	assert(sym.valid());
}

VersionedSymbol::VersionedSymbol(const Elf::Symbol & sym, const Version & version)
 : Elf::Symbol(sym), version(version) {
	assert(sym.valid());
}

//VersionedSymbol::VersionedSymbol(const Object & object) : Elf::Symbol(object), object(object) {}

bool VersionedSymbol::operator==(const VersionedSymbol & o) const {
	return this->_data == o._data
	   || (object() == o.object() && (name() == o.name() || strcmp(name(), o.name()) == 0) && version == o.version);
}

const Object & VersionedSymbol::object() const {
	return reinterpret_cast<const Object &>(this->_elf);
}

void * VersionedSymbol::pointer() const {
	void * fptr = reinterpret_cast<void*>(object().base + value());
	switch (type()) {
		// Handle ifunc
		case Elf::STT_GNU_IFUNC:
		{
			typedef void* (*indirect_t)();
			indirect_t func = reinterpret_cast<indirect_t>(fptr);
			return func();
		}

		case Elf::STT_TLS:
			return reinterpret_cast<void*>(object().tls_address(value()));

		default:
			return fptr;
	}
}

BufferStream& operator<<(BufferStream& bs, const VersionedSymbol & s) {
	if (s.valid()) {
		bs << s.name();
		if (s.version.valid) {
			if (s.version.name != nullptr)
				bs << "@" << s.version.name;
		} else {
			bs << " [invalid version]";
		}
	} else {
		bs << "*invalid*";
	}
	return bs << " (" << s.object().file.name << ")";
}

BufferStream& operator<<(BufferStream& bs, const Optional<VersionedSymbol> & s) {
	if (s)
		bs << s.value();
	else
		bs << "[no symbol]";
	return bs;
}
