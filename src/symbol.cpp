#include "symbol.hpp"

#include "object.hpp"
#include "generic.hpp"

Symbol::Symbol(const Object & object, const Elf::Symbol & sym, const char * version_name, bool version_weak)
 : Elf::Symbol(sym), object(object), version(version_name, version_weak) {
	assert(sym.valid());
}

Symbol::Symbol(const Object & object, const Elf::Symbol & sym, const char * version_name, uint32_t version_hash, bool version_weak)
 : Elf::Symbol(sym), object(object), version(version_name, version_hash, version_weak) {
	assert(sym.valid());
}

Symbol::Symbol(const Object & object) : Elf::Symbol(object.elf), object(object) {}
