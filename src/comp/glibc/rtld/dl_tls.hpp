#pragma once

#include <dlh/types.hpp>
#include <dlh/thread.hpp>

#include "comp/glibc/libdl/interface.hpp"

struct tls_index {
	size_t ti_module;
	uintptr_t ti_offset;
};

extern "C" uintptr_t __tls_get_addr(struct tls_index *ti);
extern "C" void _dl_get_tls_static_info(size_t *size, size_t *align);
extern "C" Thread * _dl_allocate_tls_init(Thread *thread, bool init_tls);
extern "C" Thread * _dl_allocate_tls(Thread * thread);
extern "C" void _dl_deallocate_tls(Thread * thread, bool free_thread_struct);
extern "C" void * _dl_tls_get_addr_soft(GLIBC::DL::link_map *l);
