// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <dlh/log.hpp>

#include "build_info.hpp"

const char * __attribute__((weak)) build_elfo_version() { return nullptr; }
const char * __attribute__((weak)) build_bean_version() { return nullptr; }
const char * __attribute__((weak)) build_bean_date() { return nullptr; }
const char * __attribute__((weak)) build_bean_flags() { return nullptr; }
const char * __attribute__((weak)) build_capstone_version() { return nullptr; }
const char * __attribute__((weak)) build_capstone_flags() { return nullptr; }
const char * __attribute__((weak)) build_dlh_version() { return nullptr; }
const char * __attribute__((weak)) build_dlh_date() { return nullptr; }
const char * __attribute__((weak)) build_dlh_flags() { return nullptr; }
const char * __attribute__((weak)) build_luci_version() { return nullptr; }
const char * __attribute__((weak)) build_luci_date() { return nullptr; }
const char * __attribute__((weak)) build_luci_flags() { return nullptr; }
const char * __attribute__((weak)) build_luci_compatibility() { return nullptr; }

namespace BuildInfo {

void print(BufferStream & stream, bool verbose) {
	stream << "Luci";
	if (build_luci_version() != nullptr)
		stream << ' ' << build_luci_version();
	if (build_luci_date() != nullptr)
		stream << " (built " << build_luci_date() << ')';
	stream << endl;
	if (verbose) {
		if (build_luci_flags() != nullptr)
			stream << " with flags: " << build_luci_flags() << endl;
		if (build_luci_compatibility() != nullptr)
			stream << " with glibc compatibility to " << build_luci_compatibility() << endl;
	}

	if (build_bean_version() != nullptr) {
		stream << "Using Bean " << build_bean_version();
		if (build_elfo_version() != nullptr)
			stream << " and Elfo " << build_elfo_version();
		if (build_bean_date() != nullptr)
			stream << " (built " << build_bean_date() << ')';
		stream << endl;
		if (verbose && build_bean_flags() != nullptr)
			stream << " with flags: " << build_bean_flags() << endl;
	}
	if (build_capstone_version() != nullptr) {
		stream << "Using Capstone " << build_capstone_version() << endl;
		if (verbose && build_capstone_flags() != nullptr)
			stream << " with flags: " << build_capstone_flags() << endl;
	}

	if (build_dlh_version() != nullptr) {
		stream << "Using DirtyLittleHelper (DLH) " << build_dlh_version();
		if (build_dlh_date() != nullptr)
			stream << " (built " << build_dlh_date() << ')';
		stream << endl;
		if (verbose && build_dlh_flags() != nullptr)
			stream << " with flags: " << build_dlh_flags() << endl;
	}
}

void log() {
	LOG_INFO << "Luci";
	if (build_luci_version() != nullptr)
		LOG_INFO_APPEND << ' ' << build_luci_version();
	if (build_luci_date() != nullptr)
		LOG_INFO_APPEND << " (built " << build_luci_date() << ')';
	LOG_INFO_APPEND << endl;
	if (build_luci_flags() != nullptr)
		LOG_TRACE << " with flags: " << build_luci_flags() << endl;
	if (build_luci_compatibility() != nullptr)
		LOG_DEBUG << " with glibc compatibility to " << build_luci_compatibility() << endl;

	if (build_bean_version() != nullptr) {
		LOG_DEBUG << "Using Bean " << build_bean_version();
		if (build_elfo_version() != nullptr)
			LOG_DEBUG_APPEND << " and Elfo " << build_elfo_version();
		if (build_bean_date() != nullptr)
			LOG_DEBUG_APPEND << " (built " << build_bean_date() << ')';
		LOG_DEBUG_APPEND << endl;
		if (build_bean_flags() != nullptr)
			LOG_TRACE << " with flags: " << build_bean_flags() << endl;
	}
	if (build_capstone_version() != nullptr) {
		LOG_DEBUG << "Using Capstone " << build_capstone_version() << endl;
		if (build_capstone_flags() != nullptr)
			LOG_TRACE << " with flags: " << build_capstone_flags() << endl;
	}

	if (build_dlh_version() != nullptr) {
		LOG_DEBUG << "Using DirtyLittleHelper (DLH) " << build_dlh_version();
		if (build_dlh_date() != nullptr)
			LOG_DEBUG_APPEND << " (built " << build_dlh_date() << ')';
		LOG_DEBUG_APPEND << endl;
		if (build_dlh_flags() != nullptr)
			LOG_TRACE << " with flags: " << build_dlh_flags() << endl;
	}
}

}  // namespace BuildInfo
