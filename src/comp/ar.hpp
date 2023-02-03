#pragma once

#include <dlh/types.hpp>

/*! \brief Parser for .a archives (created by the AR util) */
class AR {
	/*! \brief Pointer to data area */
	char * data;

	/*! \brief Size of data area */
	size_t size;

	/*! \brief Pointer to area containing long file names (for feature reference, sys-v extension) */
	const char * extended_names = nullptr;

	/*! \brief Size of area containing long file names (for sanity checks) */
	size_t extended_names_size = 0;

 public:
	/*! \brief Iteratable archive entry */
	class Entry {
		struct Header;
		friend class AR;

		/*! \brief Reference to the archive it belongs to */
		AR & archive;

		/*! \brief Header of entry */
		Header * header;

		/*! \brief Construct new archive entry with header beginning at `start` */
		Entry(AR & archive, char * start);

		/*! \brief Helper to read (and slightly modify) the area containing long file names */
		void read_extended_filenames();

		/*! \brief Are the boundaries sound? */
		bool is_out_of_range() const;

	 public:
		/*! \brief Does the entry contain a global symbol table (sys-v extension)? */
		bool is_symbol_table() const;

		/*! \brief Does the entry contain an list of long file names (sys-v extension)? */
		bool is_extended_filenames() const;

		/*! \brief Is the entry some sort of special entry (symbol table or file name list)? */
		bool is_special() const;

		/*! \brief Are the ending characters of the header correct? */
		bool is_valid() const;

		/*! \brief Does the entry contain a regular file (opposite of `is_special`)? */
		bool is_regular() const;

		/*! \brief Name of file (or `nullptr` if special) */
		const char * name() const;

		/*! \brief Size of entries data */
		size_t size() const;

		/*! \brief Pointer to entries data (or nullptr if invalid) */
		void * data() const;

		/*! \brief switch to next entry (for iteration) */
		Entry& operator++();

		/*! \brief switch to next entry (postfix) */
		Entry operator++(int);

		/*! \brief Compare with other entry (for iteration) */
		bool operator!=(const Entry& other) const;

		/*! \brief Get self reference */
		Entry& operator*();
	};

	/*! \brief Open archive already mapped in memory (must be writable / cow!) */
	AR(uintptr_t addr, size_t size);

	/*! \brief Open archive from filesystem (it will never be unmapped!) */
	AR(const char * path);

	/*! \brief Is the signature correct? */
	bool is_valid() const;

	/*! \brief Get first entry of archive */
	Entry begin();

	/*! \brief Get end (after last entry) of archive */
	Entry end();
};
