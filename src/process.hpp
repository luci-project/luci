#pragma once

#include <bits/auxv.h>

#include <map>
#include <vector>

class Process {
	/*! \brief argument count
	 */
	int argc = 0;

	/*! \brief Pointer to argument variables
	 */
	const char ** argv = nullptr;

	/*! \brief Pointer to environment variables
	 */
	const char ** envp = nullptr;

 public:
	/*! \brief Environment variables
	 */
	std::vector<std::string> env;

	/*! \brief Auxilary vectors
	 */
	std::map<unsigned long, unsigned long> aux;

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
	Process(uintptr_t stack_pointer = 0, size_t stack_size = 0);

	/*! \brief Initialize process frame
	 * \brief stack_pointer top adress of allocated stack
	 * \brief stack_size    size of stack
	 * \brief env           environment variables
	 * \brief aux           auxiliary vectors
	 */
	Process(uintptr_t stack_pointer, size_t stack_size, std::vector<std::string> & env, std::map<unsigned long, unsigned long> & aux)
	 : env(env), aux(aux), stack_pointer(stack_pointer), stack_size(stack_size) {}

	/*! \brief Setup process frame
	 * construct stack for process start
	 * \brief arg arguments
	 */
	void init(const std::vector<std::string> &arg);

	/*! \brief Start Process
	 * \param entry Start address
	 */
	void start(uintptr_t entry);


	/*! \brief Dump environment
	 * \param argc argument count
	 * \param argc argument values
	 * \param envp pointer to enironment variable
	 */
	static void dump(int argc, const char **argv, const char ** envp);

	/*! \brief Dump environment
	 * \param p process
	 */
	static void dump(const Process &p) {
		dump(p.argc, p.argv, p.envp);
	}
};
