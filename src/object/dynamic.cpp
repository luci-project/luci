#include "object/dynamic.hpp"

#include <elfo/elf_rel.hpp>
#include <dlh/log.hpp>
#include <dlh/string.hpp>
#include <dlh/auxiliary.hpp>

#include "comp/glibc/patch.hpp"
#include "comp/glibc/init.hpp"
#include "dynamic_resolve.hpp"
#include "loader.hpp"

ObjectDynamic::ObjectDynamic(ObjectIdentity & file, const Object::Data & data, bool position_independent)
  : ObjectExecutable{file, data},
    dynamic_table{this->dynamic(file.flags.premapped == 1)},
    dynamic_symbols{dynamic_table.get_symbol_table()},
    dynamic_relocations{dynamic_table.get_relocations()},
    dynamic_relocations_plt{dynamic_table.get_relocations_plt()},
    version_needed{dynamic_table.get_version_needed()},
    version_definition{dynamic_table.get_version_definition()} {
	// Set Globale Offset Table Pointer
	auto got = dynamic_table.at(Elf::DT_PLTGOT);
	if (got.valid() && got.tag() == Elf::DT_PLTGOT)
		global_offset_table = got.value();

	// Check (set) soname
	StrPtr soname(dynamic_table.get_soname());
	if (!soname.empty() && soname != file.name) {
		if (!file.name.empty())
			LOG_WARNING << "Library file name (" << file.name << ") differs from soname (" << soname << ") -- using latter one!" << endl;
		file.name = soname;
	}

	// Set base address
	if (position_independent) {
		// Base is not defined, hence
		if (file.flags.premapped == 1) {
			this->base = data.addr;
		} else {
			size_t max_size = 0;
			for (auto & segment : this->segments)
				if (Elf::PT_LOAD == segment.type() && segment.virt_addr() + segment.virt_size() > max_size)
					max_size = segment.virt_addr() + segment.virt_size();
			this->base = file.loader.next_address(max_size);
		}
		LOG_DEBUG << "Set Base of " << file.filename << " to " << (void*)(this->base) << endl;
	}
}


void * ObjectDynamic::dynamic_resolve(size_t index) const {
	auto r = relocate(dynamic_relocations_plt.at(index), file.flags.bind_not == 0);
	return r;
}

bool ObjectDynamic::preload() {
	//base = file.flags.premapped == 1 ? data.addr : file.loader.next_address();
	return preload_segments()
	    && preload_libraries()
	    && compatibility_setup();
}


void ObjectDynamic::addpath(Vector<const char *> & vec, const char * str) {
	char str_copy[String::len(str)];
	for (auto s : String::split_inplace(String::copy(str_copy, str), ':')) {
		// Apply rpath token expansion if required
		if (String::find(str, '$') != nullptr) {
			// TODO: Here we allocate space which is never freed yet
			char * path = String::duplicate(str, PATH_MAX + 1);
			assert(path != nullptr);

			// Origin
			size_t origin_len = this->file.path.len - this->file.name.len - 1;
			char origin[origin_len + 1] = {};
			String::copy(origin, this->file.path.c_str(), origin_len);
			String::replace_inplace(path, PATH_MAX, "$ORIGIN", origin);
			String::replace_inplace(path, PATH_MAX, "${ORIGIN}", origin);

			// Lib
			const char * lib =
#if defined(__x86_64__)
				"lib64"
#elif defined(__i386__)
				"lib"
#else
				""
#error "Rpath token expension with unknown lib"
#endif
			;
			String::replace_inplace(path, PATH_MAX, "$LIB", lib);
			String::replace_inplace(path, PATH_MAX, "${LIB}", lib);

			// Platform
			static const char * platform = nullptr;
			if (platform == nullptr) {
				if (auto at_platform = Auxiliary::vector(Auxiliary::AT_PLATFORM)) {
					platform = reinterpret_cast<char *>(at_platform.pointer());
				} else {
					platform =
#if defined(__x86_64__)
					"x86_64"
#elif defined(__i386__)
					"i386"
#else
				""
#endif
					;
				}
			}
			String::replace_inplace(path, PATH_MAX, "$PLATFORM", platform);
			String::replace_inplace(path, PATH_MAX, "${PLATFORM}", platform);

			vec.emplace_back(path);
		} else {
			vec.emplace_back(String::duplicate(s));
		}
	}
}

bool ObjectDynamic::compatibility_setup() {
	if (this->file_previous == nullptr)
		for (auto &dyn: dynamic_table)
			if (dyn.tag() < 80)
				this->file.libinfo[dyn.tag()] = dyn.ptr();

	return GLIBC::init(*this);
}

bool ObjectDynamic::preload_libraries() {
	bool success = true;

	// load needed libaries
	Vector<const char *> libs;
	for (auto &dyn: dynamic_table) {
		switch (dyn.tag()) {
			case Elf::DT_NEEDED:
				libs.emplace_back(dyn.string());
				break;

			case Elf::DT_RPATH:
				addpath(this->rpath, dyn.string());
				break;

			case Elf::DT_RUNPATH:
				addpath(this->runpath, dyn.string());
				break;

			case Elf::DT_FLAGS:
				if ((dyn.value() & Elf::DF_BIND_NOW) != 0)
					file.flags.bind_now = 1;
				if ((dyn.value() & Elf::DF_STATIC_TLS) != 0 && file_previous == nullptr && this->file.tls_module_id != 0 && file.loader.tls.gen > 0) {
					LOG_ERROR << *this << " has a static TLS block which cannot be initialized dynamically. Try preloading!" << endl;
					return false;
				}
				break;

			case Elf::DT_FLAGS_1:
				if ((dyn.value() & Elf::DF_1_NOW) != 0)
					file.flags.bind_now = 1;
				if ((dyn.value() & Elf::DF_1_GLOBAL) != 0)
					file.flags.bind_global = 1;
				break;

			default:
				continue;
		}
	}
	// Adjust non-inheritable flags
	auto flags = file.flags;
	flags.immutable_source = 0;
	flags.ignore_mtime = 0;
	flags.initialized = 0;
	flags.premapped = 0;
	flags.updatable = file.loader.config.dynamic_update ? 1 : 0;

	for (auto & lib : libs) {
		// Is lib excluded? TODO: Should be done with resolved path
		bool skip = false;
		for (const auto & ex : file.loader.library_exclude)
			if (strcmp(ex, lib) == 0) {
				LOG_WARNING << "Library '" << ex << "' will be skipped (exclude list)" << endl;
				skip = true;
			}
		if (!skip) {
			auto o = file.loader.library(lib, flags, false, this->rpath, this->runpath, file.ns);
			if (o == nullptr) {
				LOG_WARNING << *this << " has an unresolved dependency: " << lib << endl;
				success = false;
			} else {
				dependencies.push_back(o);
			}
		}
	}

	return success;
}

bool ObjectDynamic::fix() {
	// Patch glibc
	if (GLIBC::patch(dynamic_symbols, base))
		LOG_INFO << "Applied GLIBC Patch at " << *this << endl;

	return true;
}

bool ObjectDynamic::prepare() {
	LOG_INFO << "Prepare " << *this << " with " << (void*)global_offset_table << endl;
	bool error = false;

	// Perform initial relocations
	for (auto & reloc : dynamic_relocations)
		if (relocate(reloc, true, error) == nullptr && error)
			break;

	// PLT relocations
	if (!error && global_offset_table != 0) {
		auto got = reinterpret_cast<uintptr_t *>(base + global_offset_table);
		// 3 predefined got entries:
		// got[0] is pointer to _DYNAMIC
		got[1] = reinterpret_cast<uintptr_t>(this);
		got[2] = reinterpret_cast<uintptr_t>(_dlresolve);

		// Remainder for relocations
		for (const auto & reloc : dynamic_relocations_plt)
			if (file.flags.bind_now == 1) {
				if (relocate(reloc, true, error) == nullptr && error)
					break;
			} else {
				Relocator(reloc).increment_value(base, base);
			}
	}

	return !error;
}

bool ObjectDynamic::in_data(const Elf::Relocation & reloc) const {
	uintptr_t target = Relocator(reloc, this->global_offset_table).address(this->base);
	for (auto &mem : memory_map)
		if (mem.target.contains(target))
			return mem.target.writable();
	return false;
}

bool ObjectDynamic::update() {
	// Apply (external) relocations
	for (const auto & r : relocations) {
		// Update only if target has changed (relocation does not point to latest version).
		// And if this is not the latest version, then omit (shared) data section relocations
		// since they are performed in the latest version of this object
		if (!r.value.object().is_latest_version() && (is_latest_version() || !in_data(r.key)))
			relocate(r.key, file.flags.bind_not == 0);
	}
	return true;
}

void* ObjectDynamic::relocate(const Elf::Relocation & reloc, bool fix, bool & fatal) const {
	// Initialize relocator object
	Relocator relocator(reloc, this->global_offset_table);

	// Detect changes in data relocation
	Pair<int,uintptr_t> datarel_key{-1,0};
	if (file.loader.config.check_relocation_content && is_latest_version())
		for (auto &mem : memory_map)
			if (mem.target.contains(relocator.address(this->base))) {
				datarel_key.first = mem.target.fd;

				// Address relative to start of segment
				assert(relocator.address(this->base) >= mem.target.address());
				datarel_key.second = relocator.address(this->base) - mem.target.address();

				auto cached = file.datarel_content.find(datarel_key);
				if (cached != file.datarel_content.end()){
					uintptr_t current_value = *reinterpret_cast<uintptr_t *>(relocator.address(this->base));
					if (current_value != cached->value) {
						LOG_WARNING << "Value at relocation target " << reinterpret_cast<void*>(relocator.address(this->base)) << " has changed: " << reinterpret_cast<void*>(current_value) << " instead of " << reinterpret_cast<void*>(cached->value) << " (skipping!)" << endl;
						return reinterpret_cast<void*>(cached->value);
					}
				}
				break;
			}

	// find symbol
	auto need_symbol_index = reloc.symbol_index();
	if (need_symbol_index == 0) {
		// Local symbol
		if (fix) {
			if (mapping_protected)
				unprotect();
			auto r = relocator.fix_internal(this->base, 0, this->file.tls_module_id, this->file.tls_offset);
			if (mapping_protected)
				protect();
			if (datarel_key.first != -1)
				file.datarel_content[datarel_key] = r;
			return reinterpret_cast<void*>(r);
		} else {
			return reinterpret_cast<void*>(relocator.value_internal(this->base, 0, this->file.tls_module_id, this->file.tls_offset));
		}
	} else /* TODO: if (!dynamic_symbols.ignored(need_symbol_index)) */ {
		auto need_symbol_version_index = dynamic_symbols.version(need_symbol_index);
		//assert(need_symbol_version_index != Elf::VER_NDX_LOCAL);
		VersionedSymbol need_symbol(dynamic_symbols[need_symbol_index], get_version(need_symbol_version_index));
		// COPY Relocations have a defined symbol with the same name
		Loader::ResolveSymbolMode mode = relocator.is_copy() ? Loader::RESOLVE_EXCEPT_OBJECT : (file.flags.bind_deep == 1 ? Loader::RESOLVE_OBJECT_FIRST : Loader::RESOLVE_DEFAULT);
		if (auto symbol = file.loader.resolve_symbol(need_symbol, file.ns, &file, mode)) {
			// Update / add symbol to cache
			relocations.insert(reloc, symbol.value());
			auto & symobj = symbol->object();
			LOG_TRACE << "Relocating " << need_symbol << " in " << *this << " with " << symbol->name() << " from " << symobj << endl;
			if (fix) {
				// Relocation read-only is protected
				if (mapping_protected)
					unprotect();
				auto r = relocator.fix_external(this->base, symbol.value(), symobj.base, 0, symobj.file.tls_module_id, symobj.file.tls_offset);
				if (mapping_protected)
					protect();
				if (datarel_key.first != -1)
					file.datarel_content[datarel_key] = r;
				return reinterpret_cast<void*>(r);
			} else {
				return reinterpret_cast<void*>(relocator.value_external(this->base, symbol.value(), symobj.base, 0, symobj.file.tls_module_id, symobj.file.tls_offset));
			}
		} else if (need_symbol.bind() == STB_WEAK) {
			LOG_DEBUG << "Unable to resolve weak symbol " << need_symbol << "..." << endl;
		} else {
			LOG_ERROR << "Unable to resolve symbol " << need_symbol << " for relocation..." << file << endl;
			fatal = true;
		}
	}
	return nullptr;
}

bool ObjectDynamic::patchable() const {
	if (file_previous == nullptr
	 || file_previous->header.identification != this->header.identification
	 || file_previous->header.machine()      != this->header.machine()
	 || file_previous->header.version()      != this->header.version())
		return false;

	assert(file_previous->binary_hash && this->binary_hash);
	LOG_INFO << "Checking if " << this->file << " can patch previous version..." << endl;

	assert(file_previous == file.current);

	// TODO: Check if TLS data size has changed
	auto diff = binary_hash->diff(*(file_previous->binary_hash), file.loader.config.dependency_check, static_cast<Bean::ComparisonMode>(file.loader.config.relax_comparison));
	LOG_DEBUG << "Found " << diff.size() << " differences in " << this->file << " (compared to the current version)" << endl;
	uint16_t ignore = Bean::Symbol::Section::SECTION_RELRO | Bean::Symbol::Section::SECTION_EH_FRAME | Bean::Symbol::Section::SECTION_DYNAMIC;

	if (!Bean::patchable(diff)) {
		LOG_WARNING << "New version of " << this->file << " has non-trivial changes in the data section..." << endl;
		if (!file.loader.config.force_update)
			return false;
	}

	// Check if all required (referenced) symbols to previous object still exist in the new version
	for (const auto & object_file : file.loader.lookup)
		// TODO: If not partial, ignore &object_file == &file
		for (Object * obj = object_file.current; obj != nullptr; obj = obj->file_previous)
			for (const auto & r : obj->relocations)
				if (&r.value.object() == file_previous) {
					// TODO: Check if relocations are in some protected memory part
					LOG_DEBUG << " - referenced symbol " << r.value.name() << endl;
					if (!this->resolve_symbol(r.value.name())) {
						LOG_WARNING << "Required symbol " << r.value.name() << " not found in new version of " << this->file << ") -- not patching the library!" << endl;
						return false;
					}
			}

	// All good
	return true;
}


Optional<VersionedSymbol> ObjectDynamic::resolve_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version) const {
	auto found = dynamic_symbols.index(name, hash, gnu_hash, version_index(version));
	if (found != Elf::STN_UNDEF) {
		auto naked_sym = dynamic_symbols.at(found);
/*
		// In case we have multiple versions, check if it is mapped here or delegate to previous version (required for partial update)
		if (file.loader.config.dynamic_update && file_previous != nullptr) {
			bool provided = false;
			for (const auto & seg : memory_map)
				if (seg.target.offset >= naked_sym.value() && seg.target.offset + seg.target.size < naked_sym.value()){
					provided = true;
					break;
				}
			if (!provided)
				return file_previous->resolve_symbol(sym);
		}
*/
		if (naked_sym.section_index() != SHN_UNDEF && naked_sym.bind() != Elf::STB_LOCAL && naked_sym.visibility() == Elf::STV_DEFAULT) {
			auto symbol_version_index = dynamic_symbols.version(found);
			VersionedSymbol vs{naked_sym, get_version(symbol_version_index), hash, gnu_hash};
			return { vs };
		}
	}
	return {};
}

Optional<VersionedSymbol> ObjectDynamic::resolve_symbol(uintptr_t addr) const {
	if (addr > base) {
		uintptr_t offset = addr - base;
		for (const auto & sym : dynamic_symbols)
			if (sym.section_index() != Elf::STN_UNDEF && offset >= sym.value() && offset <= sym.value() + sym.size()) {
				VersionedSymbol vs{sym, get_version(dynamic_symbols.version(dynamic_symbols.index(sym)))};
				return { vs };
			}
	}
	return {};
}

bool ObjectDynamic::initialize() {
	// use mapped memory (due to relocations)
	LOG_DEBUG << "Initialize " << *this << endl;
	dynamic_table.init(file.loader.argc, file.loader.argv, file.loader.envp, base);
	return true;
}
