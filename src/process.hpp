// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <dlh/container/hash.hpp>
#include <dlh/container/vector.hpp>
#include <dlh/auxiliary.hpp>

class Process {
	/*! \brief allocate Stack
	 */
	uintptr_t allocate_stack(size_t stack_size);

 public:
	/*! \brief argument count
	 */
	int argc = 0;

	/*! \brief Pointer to argument variables
	 */
	const char ** argv = nullptr;

	/*! \brief Pointer to environment variables
	 */
	const char ** envp = nullptr;

	/*! \brief Environment variables
	 */
	Vector<const char *> env;

	/*! \brief Auxilary vectors
	 */
	HashMap<Auxiliary::type, long int> aux;

	/*! \brief stack pointer (on start of process)
	 */
	uintptr_t stack_pointer = 0;

	/*! \brief stack of process
	 */
	size_t stack_size;

	/*! \brief Initialize process frame by duplicating current environment
	 * \brief stack_pointer top adress of allocated stack
	 *                      (automatic allocation if `0`)
	 * \brief stack_size    size of stack
	 *                      (use system default if `0`)
	 */
	explicit Process(uintptr_t stack_pointer = 0, size_t stack_size = 0);

	/*! \brief Initialize process frame
	 * \brief stack_pointer top adress of allocated stack
	 * \brief stack_size    size of stack
	 * \brief env           environment variables
	 * \brief aux           auxiliary vectors
	 */
	Process(uintptr_t stack_pointer, size_t stack_size, Vector<const char *> & env, HashMap<Auxiliary::type, long int> & aux)
	 : env(env), aux(aux), stack_pointer(stack_pointer), stack_size(stack_size) {}

	/*! \brief Setup process frame
	 * construct stack for process start
	 * \brief arg arguments
	 */
	void init(const Vector<const char *> &arg);

	/*! \brief Start Process
	 * \param entry Start address
	 */
	void start(uintptr_t entry) {
		start(entry, stack_pointer, envp);
	}

	/*! \brief Start Process
	 * \param entry Start address
	 * \param stack_pointer Pointer to top of stack
	 * \param envp Pointer to start of environment variables on stack
	 */
	static void start(uintptr_t entry, uintptr_t stack_pointer, const char ** envp);
};
