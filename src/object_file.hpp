#pragma once

#include <limits.h>

#include <vector>

#include "dl.hpp"

struct Object;
struct Loader;

/*! \brief virtual object */
struct ObjectFile {
	/*! \brief Loader */
	Loader & loader;

	/*! \brief Full path to file */
	char path[PATH_MAX];

	/*! \brief hash of path string (for faster comparison) */
	const uint32_t hash;

	/*! \brief Namespace for object */
	const DL::Lmid_t ns;

	union Flags {
		struct {
			unsigned bind_now : 1;
		};
		unsigned value;
	} flags;

	/*! \brief Watch deskriptor */
	int wd;

	/*! \brief Current version of the object */
	Object * current;

	/*! \brief extract file name */
	const char * name() const {
		const char * r = path;
		if (r != nullptr)
			for (const char * i = path; *i != '\0'; ++i)
				if (*i == '/')
					r = i + 1;
		return r;
	}

	Object * load();

	/*! \brief constructor */
	ObjectFile(Loader & loader, const char * path, DL::Lmid_t ns = DL::LM_ID_BASE);

	~ObjectFile();
};
