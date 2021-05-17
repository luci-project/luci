#pragma once

#include <limits.h>

#include <vector>

#include "dl.hpp"
#include "strptr.hpp"

struct Object;
struct Loader;

/*! \brief virtual object */
struct ObjectIdentity {
	/*! \brief Loader */
	Loader & loader;

	/*! \brief Library name (SONAME) */
	StrPtr name;

	/*! \brief path to file */
	StrPtr path;

	/*! \brief Namespace for object */
	const DL::Lmid_t ns;

	/*! \brief Object specific flags*/
	union Flags {
		struct {
			unsigned bind_now         : 1,
			         ignore_mtime     : 1,
			         initialized      : 1;
		};
		unsigned value;

		Flags() : value(0) {}
	} flags;

	/*! \brief Shared memory */
	int memfd = -1;

	/*! \brief Library path is a symlink, we don't expect changes on the binary itself (only used for dynamic_update)*/
	bool symlinked = false;
	// TODO: Support semantic versioning?

	/*! \brief Current (latest) version of the object */
	Object * current = nullptr;

	/*! \brief Load/get current version */
	Object * load();


	/*! \brief constructor */
	ObjectIdentity(Loader & loader, const char * path, DL::Lmid_t ns = DL::LM_ID_BASE);

	~ObjectIdentity();

 private:
	friend struct Loader;

	int wd;
	char buffer[PATH_MAX + 1];

	bool initialize();
};

std::ostream& operator<<(std::ostream& os, const ObjectIdentity & o);
