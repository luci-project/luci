#pragma once

#include "loader.hpp"
#include <elfo/elf.hpp>

namespace GLIBC {
void init_start(const Loader & loader);
bool patch(const Elf::SymbolTable & symtab, uintptr_t base = 0);
void init_end();
void stack_end(void*);
}  // nammespace GLIBC
