#include <inttypes.h>
#include <map>

struct Symbol;
struct Relocation;
#include "symbol.hpp"
#include "relocation.hpp"

namespace Memory {
void dump(uintptr_t base, size_t length, bool disassemble = false, std::map<uintptr_t, Symbol*> symbols = {}, std::map<uintptr_t, Relocation*> relocations = {});
void dump(void * base, size_t length, bool disassemble = false, std::map<uintptr_t, Symbol*> symbols = {}, std::map<uintptr_t, Relocation*> relocations = {});

uintptr_t allocate_stack(size_t stack_size);

uintptr_t allocate(size_t length, bool write, bool execute);
void free(uintptr_t base, size_t length);

void copy(uintptr_t target, const uintptr_t source, size_t length);
void copy(void * target, const void * source, size_t length);

void clear(uintptr_t target, size_t length, uint8_t value = 0);
void clear(void * target, size_t length, uint8_t value = 0);
}
