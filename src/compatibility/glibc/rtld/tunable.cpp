#include <dlh/assert.hpp>
#include <dlh/types.hpp>
#include <dlh/macro.hpp>
#include <dlh/log.hpp>


// TODO: always check glibc `cat elf/dl-tunables.list | awk -f scripts/gen-tunables.awk`!
enum TunableID {
	glibc_elision_skip_lock_after_retries,
	glibc_malloc_trim_threshold,
	glibc_malloc_perturb,
	glibc_cpu_x86_shared_cache_size,
	glibc_elision_tries,
	glibc_elision_enable,
	glibc_malloc_mxfast,
	glibc_elision_skip_lock_busy,
	glibc_malloc_top_pad,
	glibc_cpu_x86_non_temporal_threshold,
	glibc_cpu_x86_shstk,
	glibc_cpu_hwcap_mask,
	glibc_malloc_mmap_max,
	glibc_elision_skip_trylock_internal_abort,
	glibc_malloc_tcache_unsorted_limit,
	glibc_cpu_x86_ibt,
	glibc_cpu_hwcaps,
	glibc_elision_skip_lock_internal_abort,
	glibc_malloc_arena_max,
	glibc_malloc_mmap_threshold,
	glibc_cpu_x86_data_cache_size,
	glibc_malloc_tcache_count,
	glibc_malloc_arena_test,
	glibc_pthread_mutex_spin_count,
	glibc_malloc_tcache_max,
	glibc_malloc_check,

	_count
};

#ifndef HWCAP_IMPORTANT
#define HWCAP_IMPORTANT ((1 << 0) | (1 << 1) | (1 << 2))
#endif

static struct Tunable	{
	/* Internal name of the tunable.  */
	const char *name;

	/* Data type of the tunable.  */
	enum Type {
		TUNABLE_TYPE_INT_32,
		TUNABLE_TYPE_UINT_64,
		TUNABLE_TYPE_SIZE_T,
		TUNABLE_TYPE_STRING
	};
	struct {
		Type type_code;
		int64_t min;
		uint64_t max;
	} type;

	/* The value.  */
	union Val {
		int64_t numval;
		const char *strval;
	} val;

	/* Flag to indicate that the tunable is initialized.  */
	bool initialized;

	/* Specify the security level for the tunable with respect to AT_SECURE programs */
	enum {
		/* Erase the tunable for AT_SECURE binaries so that child processes don't read it.  */
		TUNABLE_SECLEVEL_SXID_ERASE = 0,

		/* Ignore the tunable for AT_SECURE binaries, but don't erase it, so that child processes can read it.  */
		TUNABLE_SECLEVEL_SXID_IGNORE = 1,

		/* Read the tunable.  */
		TUNABLE_SECLEVEL_NONE = 2,
	} security_level;

	/* The compatibility environment variable name.  */
	const char *env_alias;
} tunables[] = {
	{ "glibc_elision_skip_lock_after_retries", {Tunable::TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX}, {3}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_malloc_trim_threshold", {Tunable::TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_TRIM_THRESHOLD_" },
	{ "glibc_malloc_perturb", {Tunable::TUNABLE_TYPE_INT_32, 0, 0xff}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_PERTURB_" },
	{ "glibc_cpu_x86_shared_cache_size", {Tunable::TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_elision_tries", {Tunable::TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX}, {3}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_elision_enable", {Tunable::TUNABLE_TYPE_INT_32, 0, 1}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_malloc_mxfast", {Tunable::TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_IGNORE, nullptr},
	{ "glibc_elision_skip_lock_busy", {Tunable::TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX}, {3}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_malloc_top_pad", {Tunable::TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_TOP_PAD_" },
	{ "glibc_cpu_x86_non_temporal_threshold", {Tunable::TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_cpu_x86_shstk", {Tunable::TUNABLE_TYPE_STRING, 0, 0}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_cpu_hwcap_mask", {Tunable::TUNABLE_TYPE_UINT_64, 0, UINT64_MAX}, {HWCAP_IMPORTANT}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, "LD_HWCAP_MASK" },
	{ "glibc_malloc_mmap_max", {Tunable::TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_MMAP_MAX_" },
	{ "glibc_elision_skip_trylock_internal_abort", {Tunable::TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX}, {3}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_malloc_tcache_unsorted_limit", {Tunable::TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_cpu_x86_ibt", {Tunable::TUNABLE_TYPE_STRING, 0, 0}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_cpu_hwcaps", {Tunable::TUNABLE_TYPE_STRING, 0, 0}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_elision_skip_lock_internal_abort", {Tunable::TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX}, {3}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_malloc_arena_max", {Tunable::TUNABLE_TYPE_SIZE_T, 1, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_ARENA_MAX" },
	{ "glibc_malloc_mmap_threshold", {Tunable::TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_MMAP_THRESHOLD_" },
	{ "glibc_cpu_x86_data_cache_size", {Tunable::TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_malloc_tcache_count", {Tunable::TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_malloc_arena_test", {Tunable::TUNABLE_TYPE_SIZE_T, 1, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_ARENA_TEST" },
	{ "glibc_pthread_mutex_spin_count", {Tunable::TUNABLE_TYPE_INT_32, 0, 32767}, {100}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_malloc_tcache_max", {Tunable::TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, nullptr },
	{ "glibc_malloc_check", {Tunable::TUNABLE_TYPE_INT_32, 0, 3}, {}, false, Tunable::TUNABLE_SECLEVEL_SXID_ERASE, "MALLOC_CHECK_"}
};

static_assert(sizeof(tunables) / sizeof(Tunable) == _count, "Tunables struct and enum do not match");

EXPORT void __tunable_get_val (TunableID id, void * valp, void (*callback) (Tunable::Val *)) {
	assert(id < TunableID::_count);
	auto &cur = tunables[id];
	LOG_TRACE << "GLIBC __tunable_get_val " << (int)id << " (" << cur.name << ") = ";

	switch (cur.type.type_code) {
		case Tunable::TUNABLE_TYPE_UINT_64:
			LOG_TRACE_APPEND << (uint64_t) cur.val.numval;
			*((uint64_t *) valp) = (uint64_t) cur.val.numval;
			break;
		case Tunable::TUNABLE_TYPE_INT_32:
			LOG_TRACE_APPEND << (int32_t) cur.val.numval;
			*((int32_t *) valp) = (int32_t) cur.val.numval;
			break;
		case Tunable::TUNABLE_TYPE_SIZE_T:
			LOG_TRACE_APPEND << (size_t) cur.val.numval;
			*((size_t *) valp) = (size_t) cur.val.numval;
			break;
		case Tunable::TUNABLE_TYPE_STRING:
			LOG_TRACE_APPEND << cur.val.strval;
			*((const char **)valp) = cur.val.strval;
			break;
		default:
			assert(false);
	}
	if (cur.initialized && callback != nullptr)
		callback(&cur.val);
	LOG_TRACE_APPEND << endl;
}
