#pragma once

#include <elfo/elf.hpp>

namespace GLIBC {
bool patch(const Elf::SymbolTable & symtab, uintptr_t base = 0);
}  // namespace GLIBC
