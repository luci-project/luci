// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "comp/glibc/libgcc/iterate_phdr.hpp"

#include <dlh/log.hpp>
#include <dlh/macro.hpp>
#include <dlh/container/vector.hpp>

#include "object/base.hpp"
#include "object/identity.hpp"
#include "tls.hpp"
#include "loader.hpp"


EXPORT int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data) {
	LOG_TRACE << "LIBGCC dl_iterate_phdr" << endl;
	auto loader = Loader::instance();
	assert(loader != nullptr);


	// According to libc this should be limited to the namespace of the caller:
	void *caller = __builtin_extract_return_addr(__builtin_return_address(0));
	// see https://elixir.bootlin.com/glibc/latest/source/elf/dl-iteratephdr.c
	// We will ignore this for the moment.

	// Prevent changes by caching all results
	Vector<dl_phdr_info> infos;
	loader->lookup_sync.read_lock();
	for (const auto & object_file : loader->lookup)
		for (Object * obj = object_file.current; obj != nullptr; obj = obj->file_previous)
			infos.emplace_back(
				/* dlpi_addr = */ obj->base,
				/* dlpi_name = */ object_file.filename,
				/* dlpi_phdr = */ obj->Elf::data(obj->header.e_phoff),
				/* dlpi_phnum = */ obj->header.e_phnum,
				/* info.dlpi_adds = */ 0,  // TODO
				/* info.dlpi_subs = */ 0,  // TODO
				/* dlpi_tls_modid = */ object_file.tls_module_id,
				/* dlpi_tls_data = */ object_file.tls_module_id == 0 ? 0 : loader->tls.get_addr(Thread::self(), object_file.tls_module_id, false));
	loader->lookup_sync.read_unlock();

	int ret = 0;
	for (auto & info : infos)
		if ((ret = callback(&info, sizeof(info), data)))
			break;

	return ret;
}
