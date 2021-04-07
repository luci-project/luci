#include "symbol.hpp"

#include "object.hpp"
#include "generic.hpp"

Symbol::Symbol(const Object & object, const Elf::Symbol & sym, const char * version_name, bool version_weak)
 : Elf::Symbol(sym), object(object), version(version_name, version_weak) {
	assert(sym.valid());
}

Symbol::Symbol(const Object & object, const Elf::Symbol & sym, const Version & version)
 : Elf::Symbol(sym), object(object), version(version) {
	assert(sym.valid());
}

Symbol::Symbol(const Object & object) : Elf::Symbol(object.elf), object(object) {}

std::ostream& operator<<(std::ostream& os, const Symbol & s) {
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
	os << " (" << s.object.file_name() << ")";
	return os;
}
