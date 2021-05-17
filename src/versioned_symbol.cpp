#include "versioned_symbol.hpp"

#include "object.hpp"
#include "generic.hpp"

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
