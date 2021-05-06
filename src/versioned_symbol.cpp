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

std::ostream& operator<<(std::ostream& os, const VersionedSymbol & s) {
	if (s.valid()) {
		os << s.name();
		if (s.version.valid) {
			if (s.version.name != nullptr)
				os << "@" << s.version.name;
		} else {
			os << " [invalid version]";
		}
	} else {
		os << "*invalid*";
	}
	os << " (" << s.object().file.name() << ")";
	return os;
}

std::ostream& operator<<(std::ostream& os, const std::optional<VersionedSymbol> & s) {
	if (s)
		os << s.value();
	else
		os << "[no symbol]";
	return os;
}
