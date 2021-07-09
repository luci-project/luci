#pragma once

#include <dlh/unistd.hpp>
#include <dlh/utils/log.hpp>
#include <dlh/utils/math.hpp>
#include <dlh/utils/thread.hpp>
#include <dlh/container/vector.hpp>

#include "object/identity.hpp"

static void * const TLS_UNALLOCATED = reinterpret_cast<void*>(-1);
static const size_t TLS_THREAD_SIZE = 2304;  // sizeof(struct pthread)

struct TLS {
	/*! \brief Generation counter */
	unsigned long gen = 0;

	/*! \brief Modules in initial (static) TLS */
	size_t initial_count = 0;
	/*! \brief Alignment of initial TLS */
	size_t initial_align = 8;
	/*! \brief Size of initial TLS */
	size_t initial_size = 0;

	struct Module {
		/*! \brief Target Object */
		const ObjectIdentity & object;

		/*! \brief Target size */
		const size_t size = 0;

		/*! \brief Target alignment */
		const size_t align = 0;

		/*! \brief Source data image */
		const uintptr_t image = 0;

		/*! \brief Source data image size */
		const size_t image_size = 0;

		/*! \brief Offset to thread pointer / %fs (for initial TLS) */
		const intptr_t offset;

		Module(const ObjectIdentity & object, size_t size, size_t align, uintptr_t image, size_t image_size, intptr_t offset = 0)
		  : object(object), size(size), align(align), image(image), image_size(image_size), offset(offset) {
			assert(size >= image_size);
			assert(image_size > 0 || image == 0);
		  }
	};

	/*! \brief List of modules */
	Vector<Module> modules;

	/*! \brief Add new module (object) with tls
	 * \param object Object with TLS
	 * \param size Size of TLS block in memory
	 * \param align Alignment of TLS block in memory
	 * \param image Pointer to initialisation image (in memory)
	 * \param image_size Size of initialisation image
	 * \param offset For initial TLS this will contain the offset from thread pointer to the TLS block
	 * \return module ID of TLS block
	 */
	size_t add_module(const ObjectIdentity & object, size_t size, size_t align, uintptr_t image, size_t image_size, intptr_t & offset) {
		assert(size > 0);
		if (gen == 0) {
			initial_count++;
			// Alignment of initial TLS is the maximum alignment of its modules
			if (initial_align < align)
				initial_align = align;
			// Increase size for alignment if necessary
			size = Math::align(size, initial_align);
			// increase intital TLS size
			initial_size += size;
			// Offset to thread pointer
			offset = initial_size;
		} else {
			// dynamic modules have no fixed offset
			offset = 0;
		}
		// add to module list
		modules.emplace_back(object, size, align, image, image_size, offset);
		// return module id
		return modules.size();
	}

	/*! \brief Setup initial TLS
	 * \param thread current Thread
	 */
	void dtv_setup(Thread * thread);

	/*! \brief (re)allocate DTVs with sufficient capacity
	 * \param thread current Thread
	 */
	size_t dtv_allocate(Thread * thread);

	/*! \brief Free memory for TLS of dynamically loaded modules
	 * \param thread current Thread
	 */
	void dtv_free(Thread * thread);

	/*! \brief Get base address of an tls module (do lazy allocation if necessary)
	 * \param thread current Thread
	 * \param module_id module
	 * \return absolute address of module
	 */
	inline uintptr_t get_addr(Thread * thread, size_t module_id) {
		// Check generation
		size_t & dtv_gen = dtv_generation(thread->dtv);
		if (dtv_gen != gen) {
			assert(dtv_gen < this->gen);

			// Reallocate
			if (dtv_module_size(thread->dtv) < modules.size() && dtv_allocate(thread) == 0) {
				LOG_ERROR << "Increasing capacity for DTV of Thread " << reinterpret_cast<void*>(thread->tcb) << " failed" << endl;
				assert(false);
			}

			// Set current generation and size
			dtv_gen = this->gen;
		}

		// Lazy alloc
		assert(module_id <= dtv_module_size(thread->dtv));
		auto & dtv_ptr = thread->dtv[module_id].pointer;
		if (dtv_ptr.val == TLS_UNALLOCATED) {
			assert(module_id > initial_count && module_id <= modules.size());
			auto & module = modules[module_id - 1];

			// Allocate memory
			void * mem = malloc(module.size + module.align + sizeof(void*));
			assert(mem != nullptr);

			// Align pointer
			uintptr_t data = Math::align(reinterpret_cast<uintptr_t>(mem) + sizeof(void*), module.align);

			// Store address of pointer for free;
			*(reinterpret_cast<void**>(data) - 1) = mem;

			// Copy contents
			dtv_copy(thread, module_id, reinterpret_cast<void*>(data));
		}

		// Return pointer
		return reinterpret_cast<uintptr_t>(dtv_ptr.val);
	}

	/*! \brief Allocate initial DTV
	 * \param thread Thread (or nullptr to allocate thread struct as well)
	 * \param set_fs set current FS to thread (only required in main thread once)
	 * \return Pointer to thread
	 */
	Thread * allocate(Thread * thread, bool set_fs = false);

	/*! \brief Free TLS of thread
	 * \param thread Thread to free
	 * \param free_thread_struct free full thread structure as well (otherwise only DTV and dynamic contents)
	 */
	void free(Thread * thread, bool free_thread_struct);

 private:
	/*! \brief get current DTV generation */
	inline size_t & dtv_generation(Thread::DynamicThreadVector * dtv) {
		return dtv[0].counter;
	}

	/*! \brief get current DTVs number of modules */
	inline size_t & dtv_module_size(Thread::DynamicThreadVector * dtv) {
		return dtv[-1].counter;
	}

	/*! \brief Copy TLS data
	 * \param thread thread trying to access TLS
	 * \param module_id TLS module
	 * \param ptr Pointer to memory allocated for this module of current threads TLS
	 */
	void dtv_copy(Thread * thread, size_t module_id, void * ptr) const;
};
