// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <dlh/types.hpp>

#include "object/base.hpp"

namespace Redirect {

enum Mode {
	MODE_DEBUG_TRAP,                ///< int1 instruction
	MODE_BREAKPOINT_TRAP,           ///< int3 instruction
	MODE_INVALID_OPCODE,            ///< ud2 two byte!)
	MODE_INVALID_OPCODE_HACK,       ///< push es (which is not available on x64)
	MODE_GENERAL_PROTECTION_FAULT,  ///< hlt (which is not allowed in ring 3)

	MODE_NONE,                      ///< disabled redirection

	MODE_NOT_CONFIGURED             ///< not set yet
};

/*! \brief Setup redirection
 *  installs the trap signal handler
 *  \param mode set the instructions should be used for interruption
 *  \return `true` if signal handler was installed
 */
bool setup(Mode mode = MODE_BREAKPOINT_TRAP);

/*! \brief Add redirection
 *  \param from_object Object containing the address to be redirected
 *  \param from_address memory address to be redirected
 *  \param to_address target address for redirection
 *  \param from_size available bytes for redirection
 *  \param make_static if `true`, this will be rewriten to a jump as soon as possible
 *  \param finalize apply the changes made to the compositing buffer
 *  \return `true` if redirection was installed
 */
bool add(Object & from_object, uintptr_t from_address, uintptr_t to_address, size_t from_size = 0, bool make_static = false, bool finalize = false);

/*! \brief Add redirection
 *  \param from_object Object containing the address to be redirected
 *  \param from_address memory address to be redirected
 *  \param to_address target address for redirection
 *  \param from_size available bytes for redirection
 *  \param make_static if `true`, this will be rewriten to a jump as soon as possible
 *  \param finalize apply the changes made to the compositing buffer
 *  \return `true` if redirection was installed
 */
bool add(uintptr_t from, uintptr_t to, size_t from_size = 0, bool make_static = false, bool finalize = false);

/*! \brief Add redirection
 *  \param from Symbol to be redirected
 *  \param to target address for redirection
 *  \param make_static if `true`, this will be rewriten to a jump as soon as possible
 *  \param finalize apply the changes made to the compositing buffer
 *  \return `true` if redirection was installed
 */
template<typename Symbol>
bool add(const Symbol & from, uintptr_t to, bool make_static = false, bool finalize = false) {
	return from.valid() ? add(from.object(), reinterpret_cast<uintptr_t>(from.pointer()), to, from.size(), make_static, finalize) : false;
}

/*! \brief Add redirection
 *  \param from Symbol to be redirected
 *  \param to target address for redirection
 *  \param make_static if `true`, this will be rewriten to a jump as soon as possible
 *  \param finalize apply the changes made to the compositing buffer
 *  \return `true` if redirection was installed
 */
template<typename Symbol>
bool add(Symbol & from, Symbol & to, bool make_static = false, bool finalize = false) {
	return from.valid() && to.valid() ? add(from.object(), reinterpret_cast<uintptr_t>(from.pointer()), reinterpret_cast<uintptr_t>(to.pointer()), from.size(), make_static, finalize) : false;
}

/*! \brief Remove redirection
 *  \param from_address memory address being redirected
 *  \param finalize apply the changes made to the compositing buffer
 *  \return `true` if redirection was removed
 */
bool remove(uintptr_t address, bool finalize = false);

/*! \brief Remove redirection
 *  \param from symbol being redirected
 *  \param finalize apply the changes made to the compositing buffer
 *  \return `true` if redirection was removed
 */
template<typename Symbol>
bool remove(Symbol & from, bool finalize = false) {
	return from.valid() ? remove(reinterpret_cast<uintptr_t>(from.pointer()), finalize) : false;
}

/*! \brief check if there is a redirection set
 *  \param from memory address to check
 *  \param to pointer to variable to store the target address (or nullptr to ignore)
 *  \return `true` if a redirection is set to this address
 */
bool is_set(uintptr_t from, uintptr_t * to = nullptr);

/*! \brief Remove redirection
 *  \param from symbol to check
 *  \param to pointer to variable to store the target address (or nullptr to ignore)
 *  \return `true` if a redirection is set to this address
 */
template<typename Symbol>
bool is_set(Symbol & from, uintptr_t * to = nullptr) {
	return from.valid() ? is_set(reinterpret_cast<uintptr_t>(from.pointer()), to) : false;
}

}  // namespace Redirect
