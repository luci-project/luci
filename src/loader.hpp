#pragma once


#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <optional>

#include "elf.hpp"

#include "dl.hpp"
#include "object.hpp"


struct Loader {
	/*! \brief enable dynamic updates? */
	const bool dynamic_update;

	/*! \brief default library path via argument / environment variable */
	std::vector<const char *> library_path_runtime = {};

	/*! \brief default library path from config */
	std::vector<const char *> library_path_config = {};

	/*! \brief default library path default (by convention) */
	std::vector<const char *> library_path_default  = { "/lib", "/usr/lib" };

	/*! \brief List of all loaded objects (for symbol resolving) */
	mutable std::vector<Object *> lookup;

	/*! \brief Constructor */
	Loader(const char * self, bool dynamicUpdate = false);

	/*! \brief Destructor: Unload all files */
	~Loader();

	/*! \brief Search & load libary */
	Object * library(const char * file, const std::vector<const char *> & rpath, const std::vector<const char *> & runpath, DL::Lmid_t ns = DL::LM_ID_BASE) const;
	Object * library(const char * file, const std::vector<const char *> & search = {}, DL::Lmid_t ns = DL::LM_ID_BASE) const;

	/*! \brief Load file */
	Object * file(const char * filename, const char * directory, DL::Lmid_t ns = DL::LM_ID_BASE) const;
	Object * file(const char * path, DL::Lmid_t ns = DL::LM_ID_BASE) const;

	/*! \brief Check if valid (loaded) object */
	bool valid(const Object * o) const;
	bool valid(const Object & o) const;

	/*! \brief Run */
	bool run(const Object * object, std::vector<const char *> args, uintptr_t stack_pointer = 0, size_t stack_size = 0) const;

	/*! \brief find Symbol with same name and version from other objects in same namespace
	 */
	std::optional<Symbol> resolve_symbol(const Symbol & sym, DL::Lmid_t ns = DL::LM_ID_BASE) const;

	/*! \brief get next (page aligned) memory address */
	uintptr_t next_address() const;

 private:
	Object * file_helper(const char * path, DL::Lmid_t ns = DL::LM_ID_BASE) const;
};
