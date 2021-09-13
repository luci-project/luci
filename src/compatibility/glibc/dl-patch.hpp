#pragma once

#include <elfo/elf.hpp>

namespace GLIBC {
namespace DL {
bool patch(const Elf::SymbolTable & symtab, uintptr_t base = 0);
}  // namespace DL
}  // namespace GLIBC
