#include "versioned_symbol.hpp"

#include "object/base.hpp"

VersionedSymbol::VersionedSymbol(const Elf::Symbol & sym, const char * version_name, bool version_weak)
 : Elf::Symbol(sym), version(version_name, version_weak) {
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
