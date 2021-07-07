#pragma once

#include <elfo/elf.hpp>

void glibc_init();
bool glibc_patch(const Elf::SymbolTable & symtab, uintptr_t base = 0);
