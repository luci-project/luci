#include "comp/ar.hpp"

#include <dlh/file.hpp>
#include <dlh/string.hpp>
#include <dlh/syscall.hpp>
#include <dlh/utility.hpp>

static bool parse_decimal(size_t & target, const char * buf, size_t len) {
	bool found = false;
	for (size_t i = 0; i < len; i++) {
		if (buf[i] >= '0' && buf[i] <= '9') {
			if (!found) {
				target = 0;
				found = true;
			}
			target = target * 10 + (buf[i] - '0');
		} else if (found) {
			break;
		}
	}
	return found;
}

struct AR::Entry::Header {
	char name[16];
	char mtime[12];
	char owner[6];
	char group[6];
	char mode[8];
	char size[10];
	char end[2];
}  __attribute__((packed));

AR::Entry::Entry(AR & archive, char * start) : archive(archive), header(reinterpret_cast<Header*>(start)) {
	if (is_valid() && is_extended_filenames())
		read_extended_filenames();
}

void AR::Entry::read_extended_filenames() {
	// Extended filename
	if (archive.extended_names == nullptr) {
		char * names = reinterpret_cast<char *>(header + 1);
		archive.extended_names = names;
		archive.extended_names_size = size();
		for (size_t i = 0; i < archive.extended_names_size; i++)
			if (names[i] == '/')
				names[i] = '\0';
	}
}

bool AR::Entry::is_out_of_range() const {
	return reinterpret_cast<char *>(header) <= archive.data || reinterpret_cast<char *>(header + 1) >= archive.data + archive.size;
}

bool AR::Entry::is_symbol_table() const {
	// System V symbol table
	return header->name[0] == '/' && header->name[1] == ' ';
}

bool AR::Entry::is_extended_filenames() const {
	return header->name[0] == '/' && header->name[1] == '/';
}

bool AR::Entry::is_special() const {
	return is_out_of_range() || is_symbol_table() || is_extended_filenames();
}

bool AR::Entry::is_valid() const {
	return !is_out_of_range() && header->end[0] == 0x60 && header->end[1] == 0x0a && reinterpret_cast<char *>(header) + size() <= archive.data + archive.size;
}

bool AR::Entry::is_regular() const {
	return !is_special();
}

const char * AR::Entry::name() const {
	if (!is_special()) {
		if (header->name[0] == '/') {
			size_t idx = 0;
			if (archive.extended_names != nullptr && parse_decimal(idx, header->name + 1, count(header->name) - 1) && idx < archive.extended_names_size)
				return archive.extended_names + idx;
		} else {
			for (size_t i = 0; i < count(header->name); i++)
				if (header->name[i] == '/') {
					header->name[i] = '\0';
					break;
				}
			return header->name;
		}
	}
	return nullptr;
}

size_t AR::Entry::size() const {
	size_t s = 0;
	if (!is_out_of_range())
		parse_decimal(s, header->size, count(header->size));
	return s;
}

void * AR::Entry::data() const {
	return is_valid() ? reinterpret_cast<void*>(header + 1) : nullptr;
}

AR::Entry& AR::Entry::operator++() {
	if (is_valid()) {
		// Next data
		char * data = reinterpret_cast<char*>(header + 1) + size();
		// Skip newline padding
		while (data < archive.data + archive.size && *data == '\n')
			data++;
		header = reinterpret_cast<Header*>(data);
		// read extendend filenames
		if (is_valid() && is_extended_filenames())
			read_extended_filenames();
	}
	return *this;
}

// Postfix ++ overload
AR::Entry AR::Entry::operator++(int) {
	AR::Entry i = *this;
	++*this;
	return i;
}


bool AR::Entry::operator!=(const AR::Entry& other) const {
	return &archive != &other.archive || header != other.header;
}

AR::Entry& AR::Entry::operator*() {
	return *this;
}


AR::AR(uintptr_t addr, size_t size) : data(reinterpret_cast<char *>(addr)), size(size) {
	if (!is_valid() && data != nullptr) {
		Syscall::munmap(addr, size);
		data = nullptr;
	}
}

AR::AR(const char * path) : data(File::contents::get(path, size)) {
	if (!is_valid() && data != nullptr) {
		Syscall::munmap(reinterpret_cast<uintptr_t>(data), size);
		data = nullptr;
	}
}

bool AR::is_valid() const {
	return data != nullptr && size > 8 && String::compare(data, "!<arch>\n", 8) == 0;
}

AR::Entry AR::begin() {
	return is_valid() ? AR::Entry(*this, data + 8) : end();

}

AR::Entry AR::end() {
	return AR::Entry(*this, data + size);
}
