#pragma once

#include "elf.hpp"
#include "object.hpp"
#include "symbol.hpp"

struct Relocations {
	const Object & object;
	union {
		const Elf::Array<Elf::Relocation> rel;
		const Elf::Array<Elf::RelocationWithAddend> rela;
	};
	const uint8_t type;

	Relocations(const Object & object) : object(object), type(Elf::DT_NULL) {}
	Relocations(const Object & object, const Elf::Array<Elf::Relocation> & rel) : object(object), rel(rel), type(Elf::DT_REL) {}
	Relocations(const Object & object, const Elf::Array<Elf::RelocationWithAddend> & rela) : object(object), rela(rela), type(Elf::DT_RELA) {}


	struct Entry {
		const Relocations & table;
		union {
			const Elf::Relocation rel;
			const Elf::RelocationWithAddend rela;
		};

		Entry(const Relocations & table) : table(table) {}
		Entry(const Relocations & table, const Elf::Relocation & rel) : table(table), rel(rel) {}
		Entry(const Relocations & table, const Elf::RelocationWithAddend & rela) : table(table), rela(rela) {}

		/*! \brief Valid relocation */
		bool valid() const {
			switch (table.type) {
				case Elf::DT_REL: return rel.valid();
				case Elf::DT_RELA: return rela.valid();
				default: return false;
			}
		}

		/*! \brief Get index of entry in table*/
		size_t index() const {
			switch (table.type) {
				case Elf::DT_REL: return table.rel.index(rel);
				case Elf::DT_RELA: return table.rela.index(rela);
				default: return 0;
			}
		}

		/*! \brief Address */
		uintptr_t offset() const {
			switch (table.type) {
				case Elf::DT_REL: return rel.offset();
				case Elf::DT_RELA: return rela.offset();
				default: return 0;
			}
		}

		/*! \brief Relocation type and symbol index */
		uintptr_t info() const {
			switch (table.type) {
				case Elf::DT_REL: return rel.info();
				case Elf::DT_RELA: return rela.info();
				default: return 0;
			}
		}

		/*! \brief Target symbol */
		Elf::Symbol symbol() const {
			switch (table.type) {
				case Elf::DT_REL: return rel.symbol();
				case Elf::DT_RELA: return rela.symbol();
				default: return Elf::Symbol(table.object.elf);
			}
		}

		/*! \brief Index of target symbol in corresponding symbol table */
		uint32_t symbol_index() const {
			switch (table.type) {
				case Elf::DT_REL: return rel.symbol_index();
				case Elf::DT_RELA: return rela.symbol_index();
				default: return 0;
			}
		}

		/*! \brief Relocation type (depends on architecture) */
		uint32_t type() const {
			switch (table.type) {
				case Elf::DT_REL: return rel.type();
				case Elf::DT_RELA: return rela.type();
				default: return 0;
			}
		}

		/*! \brief Addend */
		intptr_t addend() const {
			switch (table.type) {
				case Elf::DT_REL: return rel.addend();
				case Elf::DT_RELA: return rela.addend();
				default: return 0;
			}
		}

		/*! \brief Relocate this entry */
		uintptr_t relocate(const Elf::Symbol & sym, const uintptr_t sym_base = 0) const {
			switch (table.type) {
				case Elf::DT_REL: return table.relocate(rel, sym, sym_base);
				case Elf::DT_RELA: return table.relocate(rela, sym, sym_base);;
				default: return 0;
			}
		}
		uintptr_t relocate(const Symbol & sym) const {
			return relocate(sym, sym.object.base);
		}

		/*! \brief Size of relocation */
		size_t size() const {
			return table.size(type());
		}
	};

	/*! \brief check if valid */
	bool valid() const {
		return type == Elf::DT_REL || type == Elf::DT_REL;
	}

	/*! \brief get entry of relocation table */
	Entry operator[](size_t index) const {
		assert(index < count());
		switch (type) {
			case Elf::DT_REL: return Entry(*this, rel[index]);
			case Elf::DT_RELA: return Entry(*this, rela[index]);
			default: return Entry(*this);
		}
	}

	/*! \brief Number of elements in array
	 */
	size_t count() const {
		switch (type) {
			case Elf::DT_REL: return rel.count();
			case Elf::DT_RELA: return rela.count();
			default: return 0;
		}
	}

	/*! \brief Get symbol of relocation table entry */
	Elf::Symbol symbol(size_t index) const {
		switch (type) {
			case Elf::DT_REL: return rel[index].symbol();
			case Elf::DT_RELA: return rela[index].symbol();
			default: return Elf::Symbol(object.elf);
		}
	}

	/*! \brief Get index at symbol table of relocation table entry */
	uint32_t symbol_index(size_t index) const {
		switch (type) {
			case Elf::DT_REL: return rel[index].symbol_index();
			case Elf::DT_RELA: return rela[index].symbol_index();
			default: return 0;
		}
	}

	uintptr_t relocate(size_t index, const Elf::Symbol & sym, const uintptr_t sym_base = 0) const {
		switch (type) {
			case Elf::DT_REL: return relocate(rel[index], sym, sym_base);
			case Elf::DT_RELA: return relocate(rela[index], sym, sym_base);
			default: return 0;
		}
	}
	uintptr_t relocate(size_t index, const Symbol & sym) const {
		return relocate(index, sym, sym.object.base);
	}

	uint16_t symbol_table() const {
		switch (type) {
			case Elf::DT_REL: return rel.accessor().symtab;
			case Elf::DT_RELA: return rela.accessor().symtab;
			default: return 0;
		}
	}

 protected:
	size_t size(uint32_t type) const;

	uintptr_t relocate(const Elf::Relocation & entry, const Elf::Symbol & sym, const uintptr_t sym_base) const;
	uintptr_t relocate(const Elf::RelocationWithAddend & entry, const Elf::Symbol & sym, const uintptr_t sym_base) const;
};
