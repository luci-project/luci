#pragma once

#include <dlh/types.hpp>
#include <dlh/container/vector.hpp>
#include <dlh/container/list.hpp>
#include <dlh/strptr.hpp>
#include <dlh/stream/buffer.hpp>

#include "compatibility/glibc/libdl/interface.hpp"
#include "object/base.hpp"

struct Loader;

/*! \brief virtual object */
struct ObjectIdentity {
	/*** Required by GLIBC ABI ***/
	union {
		struct {
			/*! \brief base address (of the first version)
			 * \see `struct link_map` in include/link.h
			 */
			uintptr_t base = 0;

			/*! \brief Library name (\see name) */
			const char * filename = nullptr;

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

			GLIBC::DL::link_map::libname_list * libname = nullptr;

			const Elf::Dyn *libinfo[80] = {};
		};
		GLIBC::DL::link_map glibc_link_map;
	};
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
			unsigned bind_now         : 1,  // Resolve all relocations on load (or, if not set, lazy)
			         bind_not         : 1,  // Do not fix GOT (for debugging)
			         bind_global      : 1,  // Use symbol for lookup
			         bind_deep        : 1,  // First lookup in own scope
			         persistent       : 1,  // This cannot be unloaded
			         updatable        : 1,  // Object can be updated during runtime
			         immutable_source : 1,  // ELF Source (file / memory) is immutable (no changes during runtime)
			         ignore_mtime     : 1,  // Do not rely on modification time when checking for file modifications
			         initialized      : 1,  // Do not executed INIT functions / constructors (again)
			         premapped        : 1;  // Already mapped into target memory
		};
		unsigned value;

		Flags() : value(0) {}
	} flags;

	/*! \brief TLS/DTV module id (0 = none)*/
	size_t tls_module_id = 0;

	/*! \brief TLS offset (from thread pointer / %fs), 0 for dynamic (non-initial/static) TLS */
	intptr_t tls_offset = 0;

	/*! \brief Current (latest) version of the object */
	Object * current = nullptr;

	/*! \brief Storage for comparing relocated values in data section to detect changes by the user [program] */
	HashMap<uintptr_t,uintptr_t> data_check;

private:
	friend struct Loader;

	enum Info {
		INFO_CONTINUE_LOAD,
		INFO_ERROR_OPEN,
		INFO_ERROR_STAT,
		INFO_ERROR_MAP,
		INFO_ERROR_CREATE,
		INFO_ERROR_ELF,
		INFO_ERROR_INOTIFY,
		INFO_IDENTICAL_TIME,
		INFO_IDENTICAL_HASH,
		INFO_UPDATE_DISABLED,
		INFO_UPDATE_INCOMPATIBLE,
		INFO_UPDATE_MODIFIED,
		INFO_FAILED_PRELOADING,
		INFO_FAILED_MAPPING,
		INFO_FAILED_REUSE,
		INFO_SUCCESS_LOAD,
		INFO_SUCCESS_UPDATE
	};

	/*! \brief inotify descriptor for file modifications */
	int wd;

	/*! \brief filename buffer */
	char buffer[PATH_MAX + 1];
	GLIBC::DL::link_map::libname_list libname_buffer[2];

	/*! \brief watch for file modification */
	bool watch(bool force = false, bool close_existing = true);

	/*! \brief Open file (map into memory) */
	Info open(uintptr_t addr, Object::Data & data, Elf::ehdr_type & type);

	/*! \brief create new object instance */
	Pair<Object *, enum Info> create(Object::Data & data, Elf::ehdr_type type);

	/*! \brief Make memory copy of ELF */
	bool memdup(Object::Data & data);

	/*! \brief Prepare dependencies */
	bool prepare();

	/*! \brief Update relocations */
	bool update();

	/*! \brief Protect segments */
	bool protect();

	/*! \brief Unprotect (make writable) relro segments */
	bool unprotect();

	/*! \brief call initializer */
	bool initialize();

	/*! \brief send status info message */
	void status(Info msg);

 public:
	/*! \brief Load/get current version
	 * \param addr use memory mapped Elf instead of file located at path
	 * \param type ELF type (`ET_NONE` to auto determine)
	 * \return pointer to newly opened object (or nullptr on failure / if already loaded)
	 */
	Object * load(uintptr_t addr = 0, Elf::ehdr_type type = Elf::ET_NONE);

	/*! \brief constructor */
	ObjectIdentity(Loader & loader, const Flags flags, const char * path = nullptr, namespace_t ns = NAMESPACE_BASE, const char * altname = nullptr);
	~ObjectIdentity();
};

typedef List<ObjectIdentity, ObjectIdentity, &ObjectIdentity::next, &ObjectIdentity::prev> ObjectIdentityList;

static inline BufferStream& operator<<(BufferStream& bs, const ObjectIdentity & o) {
	return bs << o.name << " (" << o.path << ")";
}
static inline BufferStream& operator<<(BufferStream& bs, const Object & o) {
	return bs << "[Object " << o.file << "]";
}
