#pragma once

#include <dlh/types.hpp>
#include <dlh/container/vector.hpp>
#include <dlh/container/list.hpp>
#include <dlh/utils/strptr.hpp>
#include <dlh/stream/buffer.hpp>

#include "object/base.hpp"

typedef long int namespace_t;
const namespace_t NAMESPACE_BASE = 0;
const namespace_t NAMESPACE_NEW = -1;

struct Loader;

/*! \brief virtual object */
struct ObjectIdentity {
	/*** Required by GLIBC ABI ***/
	/*! \brief base address (of the first version)
	 * \see `struct link_map` in include/link.h
	 */
	uintptr_t base = 0;

	/*! \brief Library name (\see name) */
	const char *filename = nullptr;

	/*! \brief Dynamic section of the shared object */
	uintptr_t dynamic = 0;

	/*! \brief Linked List pointer */
	ObjectIdentity *next = nullptr;
	ObjectIdentity *prev = nullptr;

	/*** Not required by the GLIBC ABI, but maybe still used by GLIBC ***/
	/*! \brief Link map for multiple mamespace */
	void * multiple_namespace = nullptr;

	/*! \brief Namespace for object */
	const namespace_t ns;

	// TODO: Padding size
	void * padding[100] = {};
	/*** End of GLIBC stuff ***/

	/*! \brief Loader */
	Loader & loader;

	/*! \brief Library name (SONAME) */
	StrPtr name;

	/*! \brief path to file */
	StrPtr path;

	/*! \brief Object specific flags*/
	union Flags {
		struct {
			unsigned bind_now         : 1,
			         updatable        : 1,  // Object can be updated during runtime
			         immutable_source : 1,  // ELF Source (file / memory) is immutable (no changes during runtime)
			         ignore_mtime     : 1,  // Do not rely on modification time when checking for file modifications
			         initialized      : 1;  // Do not executed INIT functions / constructors (again)
		};
		unsigned value;

		Flags() : value(0) {}
	} flags;

	/*! \brief Shared memory (for .data and .bss) */
	int memfd = -1;

	/*! \brief TLS/DTV module id (0 = none)*/
	size_t tls_module_id = 0;

	/*! \brief TLS offset (from thread pointer / %fs), 0 for dynamic (non-initial/static) TLS */
	intptr_t tls_offset = 0;

	/*! \brief Current (latest) version of the object */
	Object * current = nullptr;

	/*! \brief Load/get current version
	 * \param ptr use memory mapped Elf instead of file located at path
	 * \param preload preload and map object
	 * \param type ELF type (`ET_NONE` to auto determine)
	 * \return pointer to newly opened object (or nullptr on failure / if already loaded)
	 */
	Object * load(void * ptr = nullptr, bool preload = true, bool map = true, Elf::ehdr_type type = Elf::ET_NONE);

	/*! \brief constructor */
	ObjectIdentity(Loader & loader, const char * path = nullptr, namespace_t ns = NAMESPACE_BASE, const char * altname = nullptr);
	~ObjectIdentity();

 private:
	friend struct Loader;

	int wd;
	char buffer[PATH_MAX + 1];


	/*! \brief create new object instance */
	Object * create(Object::Data & data, bool preload, bool map, Elf::ehdr_type type);

	/*! \brief Make memory copy of ELF */
	bool memdup(Object::Data & data);

	bool prepare();

	bool initialize();
};

typedef List<ObjectIdentity, ObjectIdentity, &ObjectIdentity::next, &ObjectIdentity::prev> ObjectIdentityList;

static inline BufferStream& operator<<(BufferStream& bs, const ObjectIdentity & o) {
	return bs << o.name << " (" << o.path << ")";
}
static inline BufferStream& operator<<(BufferStream& bs, const Object & o) {
	return bs << "[Object " << o.file << "]";
}
