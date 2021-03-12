#pragma once

#include <inttypes.h>
#include <unordered_map>

struct Section;
#include "section.hpp"

struct Allocation {
	bool writeable;
	bool executable;
	uintptr_t start;
	size_t length;

	std::unordered_map<size_t, Section*> sections;

	Allocation(bool writeable, bool executable) : writeable(writeable), executable(executable), start(0), length(0) {}
	~Allocation();

	void load();
	void relocate();

	static void add(Section * sec);
	static void loadAll();
	static void relocateAll();
	static void dumpAll();
	static void clearAll();

	static std::vector<Allocation*> areas;
};
