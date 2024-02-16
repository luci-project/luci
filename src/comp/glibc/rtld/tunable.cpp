// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <dlh/assert.hpp>
#include <dlh/types.hpp>
#include <dlh/macro.hpp>
#include <dlh/log.hpp>

/* Default tunable configuration from glibc */

#ifndef HWCAP_IMPORTANT
#define HWCAP_IMPORTANT ((1 << 0) | (1 << 1) | (1 << 2))
#endif

#define GLIBC_CPU_HWCAP_MASK \
	TUNABLE_VALUE(glibc, cpu, hwcap_mask, TUNABLE_TYPE_UINT_64, 0, UINT64_MAX, {HWCAP_IMPORTANT}, false, TUNABLE_SECLEVEL_SXID_ERASE, "LD_HWCAP_MASK")

#define GLIBC_CPU_HWCAPS \
	TUNABLE_VALUE(glibc, cpu, hwcaps, TUNABLE_TYPE_STRING, 0, 0, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_CPU_X86_DATA_CACHE_SIZE \
	TUNABLE_VALUE(glibc, cpu, x86_data_cache_size, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_CPU_X86_IBT \
	TUNABLE_VALUE(glibc, cpu, x86_ibt, TUNABLE_TYPE_STRING, 0, 0, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_CPU_X86_NON_TEMPORAL_THRESHOLD \
	TUNABLE_VALUE(glibc, cpu, x86_non_temporal_threshold, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_CPU_X86_REP_MOVSB_THRESHOLD \
	TUNABLE_VALUE(glibc, cpu, x86_rep_movsb_threshold, TUNABLE_TYPE_SIZE_T, 1, SIZE_MAX, {2048}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_CPU_X86_REP_STOSB_THRESHOLD \
	TUNABLE_VALUE(glibc, cpu, x86_rep_stosb_threshold, TUNABLE_TYPE_SIZE_T, 1, SIZE_MAX, {2048}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_CPU_X86_SHARED_CACHE_SIZE \
	TUNABLE_VALUE(glibc, cpu, x86_shared_cache_size, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_CPU_X86_SHSTK \
	TUNABLE_VALUE(glibc, cpu, x86_shstk, TUNABLE_TYPE_STRING, 0, 0, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_ELISION_ENABLE \
	TUNABLE_VALUE(glibc, elision, enable, TUNABLE_TYPE_INT_32, 0, 1, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
	TUNABLE_VALUE(glibc, elision, skip_lock_after_retries, TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX, {3}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_ELISION_SKIP_LOCK_BUSY \
	TUNABLE_VALUE(glibc, elision, skip_lock_busy, TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX, {3}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_ELISION_SKIP_LOCK_INTERNAL_ABORT \
	TUNABLE_VALUE(glibc, elision, skip_lock_internal_abort, TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX, {3}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_ELISION_SKIP_TRYLOCK_INTERNAL_ABORT \
	TUNABLE_VALUE(glibc, elision, skip_trylock_internal_abort, TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX, {3}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_ELISION_TRIES \
	TUNABLE_VALUE(glibc, elision, tries, TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX, {3}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_GMON_MINARCS \
	TUNABLE_VALUE(glibc, gmon, minarcs, TUNABLE_TYPE_INT_32, 0x32, INT32_MAX, {0x32}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_GMON_MAXARCS \
	TUNABLE_VALUE(glibc, gmon, maxarcs, TUNABLE_TYPE_INT_32, 0x32, INT32_MAX, {0x100000}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_MALLOC_ARENA_MAX \
	TUNABLE_VALUE(glibc, malloc, arena_max, TUNABLE_TYPE_SIZE_T, 1, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_ARENA_MAX")

#define GLIBC_MALLOC_ARENA_TEST \
	TUNABLE_VALUE(glibc, malloc, arena_test, TUNABLE_TYPE_SIZE_T, 1, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_ARENA_TEST")

#define GLIBC_MALLOC_CHECK \
	TUNABLE_VALUE(glibc, malloc, check, TUNABLE_TYPE_INT_32, 0, 3, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, "MALLOC_CHECK_")

#define GLIBC_MALLOC_HUGETLB \
	TUNABLE_VALUE(glibc, malloc, hugetlb, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_MALLOC_MMAP_MAX \
	TUNABLE_VALUE(glibc, malloc, mmap_max, TUNABLE_TYPE_INT_32, INT32_MIN, INT32_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_MMAP_MAX_")

#define GLIBC_MALLOC_MMAP_THRESHOLD \
	TUNABLE_VALUE(glibc, malloc, mmap_threshold, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_MMAP_THRESHOLD_")

#define GLIBC_MALLOC_MXFAST \
	TUNABLE_VALUE(glibc, malloc, mxfast, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_IGNORE, {0})

#define GLIBC_MALLOC_PERTURB \
	TUNABLE_VALUE(glibc, malloc, perturb, TUNABLE_TYPE_INT_32, 0, 0xff, {0}, false, TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_PERTURB_")

#define GLIBC_MALLOC_TCACHE_COUNT \
	TUNABLE_VALUE(glibc, malloc, tcache_count, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_MALLOC_TCACHE_MAX \
	TUNABLE_VALUE(glibc, malloc, tcache_max, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_MALLOC_TCACHE_UNSORTED_LIMIT \
	TUNABLE_VALUE(glibc, malloc, tcache_unsorted_limit, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_MALLOC_TOP_PAD \
	TUNABLE_VALUE(glibc, malloc, top_pad, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_TOP_PAD_")

#define GLIBC_MALLOC_TRIM_THRESHOLD \
	TUNABLE_VALUE(glibc, malloc, trim_threshold, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_IGNORE, "MALLOC_TRIM_THRESHOLD_")

#define GLIBC_MEM_TAGGING \
	TUNABLE_VALUE(glibc, mem, tagging, TUNABLE_TYPE_INT_32, 0, 255, {0}, false, TUNABLE_SECLEVEL_SXID_IGNORE, {0})

#define GLIBC_PTHREAD_MUTEX_SPIN_COUNT \
	TUNABLE_VALUE(glibc, pthread, mutex_spin_count, TUNABLE_TYPE_INT_32, 0, 32767, {100}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_PTHREAD_RSEQ \
	TUNABLE_VALUE(glibc, pthread, rseq, TUNABLE_TYPE_INT_32, 0, 1, {1}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_PTHREAD_STACK_CACHE_SIZE \
	TUNABLE_VALUE(glibc, pthread, stack_cache_size, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {41943040}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_PTHREAD_STACK_HUGETLB \
	TUNABLE_VALUE(glibc, pthread, stack_hugetlb, TUNABLE_TYPE_INT_32, 0, 1, {1}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_RTLD_DYNAMIC_SORT \
	TUNABLE_VALUE(glibc, rtld, dynamic_sort, TUNABLE_TYPE_INT_32, 1, 2, {2}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_RTLD_NNS \
	TUNABLE_VALUE(glibc, rtld, nns, TUNABLE_TYPE_SIZE_T, 1, 16, {4}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_RTLD_OPTIONAL_STATIC_TLS \
	TUNABLE_VALUE(glibc, rtld, optional_static_tls, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {512}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_TUNE_HWCAP_MASK \
	TUNABLE_VALUE(glibc, tune, hwcap_mask, TUNABLE_TYPE_UINT_64, 0, UINT64_MAX, {HWCAP_IMPORTANT}, false, TUNABLE_SECLEVEL_SXID_ERASE, "LD_HWCAP_MASK")

#define GLIBC_TUNE_HWCAPS \
	TUNABLE_VALUE(glibc, tune, hwcaps, TUNABLE_TYPE_STRING, 0, 0, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_TUNE_X86_DATA_CACHE_SIZE \
	TUNABLE_VALUE(glibc, tune, x86_data_cache_size, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_TUNE_X86_IBT \
	TUNABLE_VALUE(glibc, tune, x86_ibt, TUNABLE_TYPE_STRING, 0, 0, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_TUNE_X86_NON_TEMPORAL_THRESHOLD  \
	TUNABLE_VALUE(glibc, tune, x86_non_temporal_threshold, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_TUNE_X86_SHARED_CACHE_SIZE \
	TUNABLE_VALUE(glibc, tune, x86_shared_cache_size, TUNABLE_TYPE_SIZE_T, 0, SIZE_MAX, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})

#define GLIBC_TUNE_X86_SHSTK \
	TUNABLE_VALUE(glibc, tune, x86_shstk, TUNABLE_TYPE_STRING, 0, 0, {0}, false, TUNABLE_SECLEVEL_SXID_ERASE, {0})


/* Tunables in GLIBC versions (in the required order
 * Generated using glibc source with
 * cat elf/dl-tunables.list sysdeps/x86/dl-tunables.list sysdeps/nptl/dl-tunables.list | awk -f scripts/gen-tunables.awk | sed -n "s/  TUNABLE_ENUM_NAME(\(.*\), \(.*\), \(.*\)),/\t\U\1_\2_\3 \\\\/p"
*/

#define TUNABLE_LIST_GLIBC_225 \
	GLIBC_MALLOC_ARENA_MAX \
	GLIBC_MALLOC_MMAP_MAX \
	GLIBC_MALLOC_MMAP_THRESHOLD \
	GLIBC_MALLOC_CHECK \
	GLIBC_MALLOC_PERTURB \
	GLIBC_MALLOC_TRIM_THRESHOLD \
	GLIBC_MALLOC_ARENA_TEST \
	GLIBC_MALLOC_TOP_PAD

#define TUNABLE_LIST_GLIBC_226 \
	GLIBC_TUNE_HWCAPS \
	GLIBC_MALLOC_TRIM_THRESHOLD \
	GLIBC_MALLOC_PERTURB \
	GLIBC_MALLOC_TOP_PAD \
	GLIBC_MALLOC_MMAP_MAX \
	GLIBC_MALLOC_TCACHE_UNSORTED_LIMIT \
	GLIBC_TUNE_X86_NON_TEMPORAL_THRESHOLD \
	GLIBC_TUNE_X86_SHARED_CACHE_SIZE \
	GLIBC_TUNE_HWCAP_MASK \
	GLIBC_MALLOC_ARENA_MAX \
	GLIBC_MALLOC_MMAP_THRESHOLD \
	GLIBC_MALLOC_TCACHE_COUNT \
	GLIBC_MALLOC_ARENA_TEST \
	GLIBC_TUNE_X86_DATA_CACHE_SIZE \
	GLIBC_MALLOC_TCACHE_MAX \
	GLIBC_MALLOC_CHECK

#define TUNABLE_LIST_GLIBC_227 \
	GLIBC_TUNE_HWCAPS \
	GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
	GLIBC_MALLOC_TRIM_THRESHOLD \
	GLIBC_MALLOC_PERTURB \
	GLIBC_ELISION_TRIES \
	GLIBC_ELISION_ENABLE \
	GLIBC_ELISION_SKIP_LOCK_BUSY \
	GLIBC_MALLOC_TOP_PAD \
	GLIBC_MALLOC_MMAP_MAX \
	GLIBC_ELISION_SKIP_TRYLOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_TCACHE_UNSORTED_LIMIT \
	GLIBC_TUNE_X86_NON_TEMPORAL_THRESHOLD \
	GLIBC_TUNE_X86_SHARED_CACHE_SIZE \
	GLIBC_ELISION_SKIP_LOCK_INTERNAL_ABORT \
	GLIBC_TUNE_HWCAP_MASK \
	GLIBC_MALLOC_ARENA_MAX \
	GLIBC_MALLOC_MMAP_THRESHOLD \
	GLIBC_MALLOC_TCACHE_COUNT \
	GLIBC_MALLOC_ARENA_TEST \
	GLIBC_TUNE_X86_DATA_CACHE_SIZE \
	GLIBC_MALLOC_TCACHE_MAX \
	GLIBC_MALLOC_CHECK

#define TUNABLE_LIST_GLIBC_228 \
	GLIBC_TUNE_HWCAPS \
	GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
	GLIBC_MALLOC_TRIM_THRESHOLD \
	GLIBC_MALLOC_PERTURB \
	GLIBC_TUNE_X86_SHSTK \
	GLIBC_ELISION_TRIES \
	GLIBC_ELISION_ENABLE \
	GLIBC_MALLOC_MXFAST \
	GLIBC_TUNE_X86_IBT \
	GLIBC_ELISION_SKIP_LOCK_BUSY \
	GLIBC_MALLOC_TOP_PAD \
	GLIBC_MALLOC_MMAP_MAX \
	GLIBC_ELISION_SKIP_TRYLOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_TCACHE_UNSORTED_LIMIT \
	GLIBC_TUNE_X86_NON_TEMPORAL_THRESHOLD \
	GLIBC_TUNE_X86_SHARED_CACHE_SIZE \
	GLIBC_ELISION_SKIP_LOCK_INTERNAL_ABORT \
	GLIBC_TUNE_HWCAP_MASK \
	GLIBC_MALLOC_ARENA_MAX \
	GLIBC_MALLOC_MMAP_THRESHOLD \
	GLIBC_MALLOC_TCACHE_COUNT \
	GLIBC_MALLOC_ARENA_TEST \
	GLIBC_TUNE_X86_DATA_CACHE_SIZE \
	GLIBC_MALLOC_TCACHE_MAX \
	GLIBC_MALLOC_CHECK

#define TUNABLE_LIST_GLIBC_229 \
	GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
	GLIBC_MALLOC_TRIM_THRESHOLD \
	GLIBC_MALLOC_PERTURB \
	GLIBC_CPU_X86_SHARED_CACHE_SIZE \
	GLIBC_ELISION_TRIES \
	GLIBC_ELISION_ENABLE \
	GLIBC_MALLOC_MXFAST \
	GLIBC_ELISION_SKIP_LOCK_BUSY \
	GLIBC_MALLOC_TOP_PAD \
	GLIBC_CPU_X86_NON_TEMPORAL_THRESHOLD \
	GLIBC_CPU_X86_SHSTK \
	GLIBC_CPU_HWCAP_MASK \
	GLIBC_MALLOC_MMAP_MAX \
	GLIBC_ELISION_SKIP_TRYLOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_TCACHE_UNSORTED_LIMIT \
	GLIBC_CPU_X86_IBT \
	GLIBC_CPU_HWCAPS \
	GLIBC_ELISION_SKIP_LOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_ARENA_MAX \
	GLIBC_MALLOC_MMAP_THRESHOLD \
	GLIBC_CPU_X86_DATA_CACHE_SIZE \
	GLIBC_MALLOC_TCACHE_COUNT \
	GLIBC_MALLOC_ARENA_TEST \
	GLIBC_PTHREAD_MUTEX_SPIN_COUNT \
	GLIBC_MALLOC_TCACHE_MAX \
	GLIBC_MALLOC_CHECK

#define TUNABLE_LIST_GLIBC_230 TUNABLE_LIST_GLIBC_229

#define TUNABLE_LIST_GLIBC_231 TUNABLE_LIST_GLIBC_230

#define TUNABLE_LIST_GLIBC_232 \
	GLIBC_RTLD_NNS \
	GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
	GLIBC_MALLOC_TRIM_THRESHOLD \
	GLIBC_MALLOC_PERTURB \
	GLIBC_CPU_X86_SHARED_CACHE_SIZE \
	GLIBC_ELISION_TRIES \
	GLIBC_ELISION_ENABLE \
	GLIBC_CPU_X86_REP_MOVSB_THRESHOLD \
	GLIBC_MALLOC_MXFAST \
	GLIBC_ELISION_SKIP_LOCK_BUSY \
	GLIBC_MALLOC_TOP_PAD \
	GLIBC_CPU_X86_REP_STOSB_THRESHOLD \
	GLIBC_CPU_X86_NON_TEMPORAL_THRESHOLD \
	GLIBC_CPU_X86_SHSTK \
	GLIBC_CPU_HWCAP_MASK \
	GLIBC_MALLOC_MMAP_MAX \
	GLIBC_ELISION_SKIP_TRYLOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_TCACHE_UNSORTED_LIMIT \
	GLIBC_CPU_X86_IBT \
	GLIBC_CPU_HWCAPS \
	GLIBC_ELISION_SKIP_LOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_ARENA_MAX \
	GLIBC_MALLOC_MMAP_THRESHOLD \
	GLIBC_CPU_X86_DATA_CACHE_SIZE \
	GLIBC_MALLOC_TCACHE_COUNT \
	GLIBC_MALLOC_ARENA_TEST \
	GLIBC_PTHREAD_MUTEX_SPIN_COUNT \
	GLIBC_RTLD_OPTIONAL_STATIC_TLS \
	GLIBC_MALLOC_TCACHE_MAX \
	GLIBC_MALLOC_CHECK

#define TUNABLE_LIST_GLIBC_233 \
	GLIBC_RTLD_NNS \
	GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
	GLIBC_MALLOC_TRIM_THRESHOLD \
	GLIBC_MALLOC_PERTURB \
	GLIBC_CPU_X86_SHARED_CACHE_SIZE \
	GLIBC_MEM_TAGGING \
	GLIBC_ELISION_TRIES \
	GLIBC_ELISION_ENABLE \
	GLIBC_CPU_X86_REP_MOVSB_THRESHOLD \
	GLIBC_MALLOC_MXFAST \
	GLIBC_ELISION_SKIP_LOCK_BUSY \
	GLIBC_MALLOC_TOP_PAD \
	GLIBC_CPU_X86_REP_STOSB_THRESHOLD \
	GLIBC_CPU_X86_NON_TEMPORAL_THRESHOLD \
	GLIBC_CPU_X86_SHSTK \
	GLIBC_CPU_HWCAP_MASK \
	GLIBC_MALLOC_MMAP_MAX \
	GLIBC_ELISION_SKIP_TRYLOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_TCACHE_UNSORTED_LIMIT \
	GLIBC_CPU_X86_IBT \
	GLIBC_CPU_HWCAPS \
	GLIBC_ELISION_SKIP_LOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_ARENA_MAX \
	GLIBC_MALLOC_MMAP_THRESHOLD \
	GLIBC_CPU_X86_DATA_CACHE_SIZE \
	GLIBC_MALLOC_TCACHE_COUNT \
	GLIBC_MALLOC_ARENA_TEST \
	GLIBC_PTHREAD_MUTEX_SPIN_COUNT \
	GLIBC_RTLD_OPTIONAL_STATIC_TLS \
	GLIBC_MALLOC_TCACHE_MAX \
	GLIBC_MALLOC_CHECK

#define TUNABLE_LIST_GLIBC_234 \
	GLIBC_RTLD_NNS \
	GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
	GLIBC_MALLOC_TRIM_THRESHOLD \
	GLIBC_MALLOC_PERTURB \
	GLIBC_CPU_X86_SHARED_CACHE_SIZE \
	GLIBC_MEM_TAGGING \
	GLIBC_ELISION_TRIES \
	GLIBC_ELISION_ENABLE \
	GLIBC_CPU_X86_REP_MOVSB_THRESHOLD \
	GLIBC_MALLOC_MXFAST \
	GLIBC_ELISION_SKIP_LOCK_BUSY \
	GLIBC_MALLOC_TOP_PAD \
	GLIBC_PTHREAD_STACK_CACHE_SIZE \
	GLIBC_CPU_X86_REP_STOSB_THRESHOLD \
	GLIBC_CPU_X86_NON_TEMPORAL_THRESHOLD \
	GLIBC_CPU_X86_SHSTK \
	GLIBC_CPU_HWCAP_MASK \
	GLIBC_MALLOC_MMAP_MAX \
	GLIBC_ELISION_SKIP_TRYLOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_TCACHE_UNSORTED_LIMIT \
	GLIBC_CPU_X86_IBT \
	GLIBC_CPU_HWCAPS \
	GLIBC_ELISION_SKIP_LOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_ARENA_MAX \
	GLIBC_MALLOC_MMAP_THRESHOLD \
	GLIBC_CPU_X86_DATA_CACHE_SIZE \
	GLIBC_MALLOC_TCACHE_COUNT \
	GLIBC_MALLOC_ARENA_TEST \
	GLIBC_PTHREAD_MUTEX_SPIN_COUNT \
	GLIBC_RTLD_OPTIONAL_STATIC_TLS \
	GLIBC_MALLOC_TCACHE_MAX \
	GLIBC_MALLOC_CHECK

#define TUNABLE_LIST_GLIBC_235 \
	GLIBC_RTLD_NNS \
	GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
	GLIBC_MALLOC_TRIM_THRESHOLD \
	GLIBC_MALLOC_PERTURB \
	GLIBC_CPU_X86_SHARED_CACHE_SIZE \
	GLIBC_PTHREAD_RSEQ \
	GLIBC_MEM_TAGGING \
	GLIBC_ELISION_TRIES \
	GLIBC_ELISION_ENABLE \
	GLIBC_MALLOC_HUGETLB \
	GLIBC_CPU_X86_REP_MOVSB_THRESHOLD \
	GLIBC_MALLOC_MXFAST \
	GLIBC_RTLD_DYNAMIC_SORT \
	GLIBC_ELISION_SKIP_LOCK_BUSY \
	GLIBC_MALLOC_TOP_PAD \
	GLIBC_CPU_X86_REP_STOSB_THRESHOLD \
	GLIBC_CPU_X86_NON_TEMPORAL_THRESHOLD \
	GLIBC_CPU_X86_SHSTK \
	GLIBC_PTHREAD_STACK_CACHE_SIZE \
	GLIBC_CPU_HWCAP_MASK \
	GLIBC_MALLOC_MMAP_MAX \
	GLIBC_ELISION_SKIP_TRYLOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_TCACHE_UNSORTED_LIMIT \
	GLIBC_CPU_X86_IBT \
	GLIBC_CPU_HWCAPS \
	GLIBC_ELISION_SKIP_LOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_ARENA_MAX \
	GLIBC_MALLOC_MMAP_THRESHOLD \
	GLIBC_CPU_X86_DATA_CACHE_SIZE \
	GLIBC_MALLOC_TCACHE_COUNT \
	GLIBC_MALLOC_ARENA_TEST \
	GLIBC_PTHREAD_MUTEX_SPIN_COUNT \
	GLIBC_RTLD_OPTIONAL_STATIC_TLS \
	GLIBC_MALLOC_TCACHE_MAX \
	GLIBC_MALLOC_CHECK \

#define TUNABLE_LIST_GLIBC_236 \
	GLIBC_RTLD_NNS \
	GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
	GLIBC_MALLOC_TRIM_THRESHOLD \
	GLIBC_MALLOC_PERTURB \
	GLIBC_CPU_X86_SHARED_CACHE_SIZE \
	GLIBC_PTHREAD_RSEQ \
	GLIBC_MEM_TAGGING \
	GLIBC_ELISION_TRIES \
	GLIBC_ELISION_ENABLE \
	GLIBC_MALLOC_HUGETLB \
	GLIBC_CPU_X86_REP_MOVSB_THRESHOLD \
	GLIBC_MALLOC_MXFAST \
	GLIBC_RTLD_DYNAMIC_SORT \
	GLIBC_ELISION_SKIP_LOCK_BUSY \
	GLIBC_MALLOC_TOP_PAD \
	GLIBC_CPU_X86_REP_STOSB_THRESHOLD \
	GLIBC_CPU_X86_NON_TEMPORAL_THRESHOLD \
	GLIBC_CPU_X86_SHSTK \
	GLIBC_PTHREAD_STACK_CACHE_SIZE \
	GLIBC_GMON_MINARCS \
	GLIBC_CPU_HWCAP_MASK \
	GLIBC_MALLOC_MMAP_MAX \
	GLIBC_ELISION_SKIP_TRYLOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_TCACHE_UNSORTED_LIMIT \
	GLIBC_CPU_X86_IBT \
	GLIBC_CPU_HWCAPS \
	GLIBC_ELISION_SKIP_LOCK_INTERNAL_ABORT \
	GLIBC_MALLOC_ARENA_MAX \
	GLIBC_MALLOC_MMAP_THRESHOLD \
	GLIBC_CPU_X86_DATA_CACHE_SIZE \
	GLIBC_MALLOC_TCACHE_COUNT \
	GLIBC_MALLOC_ARENA_TEST \
	GLIBC_PTHREAD_MUTEX_SPIN_COUNT \
	GLIBC_GMON_MAXARCS \
	GLIBC_RTLD_OPTIONAL_STATIC_TLS \
	GLIBC_MALLOC_TCACHE_MAX \
	GLIBC_MALLOC_CHECK \


/* Do some preprocessor *magic*, part 0: select list */

#include "comp/glibc/version.hpp"

#if GLIBC_TUNABLE_SIZE > 0

#ifndef TUNABLE_LIST
#define TUNABLE_GET_LIST(MAJOR, MINOR) TUNABLE_GET_LIST_HELPER(MAJOR, MINOR)
#define TUNABLE_GET_LIST_HELPER(MAJOR, MINOR) TUNABLE_LIST_ ## MAJOR ## _ ## MINOR
#define TUNABLE_LIST TUNABLE_GET_LIST(GLIBC, GLIBC_VERSION)
#endif

/* Do some preprocessor *magic*, part 1: enum with tunable ids */

#define TUNABLE_VALUE(TOP, NS, ID, TYPE, MIN, MAX, VAL, INIT, SECURITY, ALIAS) TUNABLE_ENUM_NAME(TOP, NS, ID)
#define TUNABLE_ENUM_NAME(TOP, NS, ID) TOP ## _ ## NS ## _ ## ID,

// #define TUNABLE_LIST(VERSION) TUNABLE_LIST_HELPER(VERSION)
// #define TUNABLE_LIST_HELPER(VERSION) TUNABLE_LIST_GLIBC_ ## VERSION


enum TunableID {
	TUNABLE_LIST
	_count
};
#if GLIBC_TUNABLE_COUNT
static_assert(_count == GLIBC_TUNABLE_COUNT, "Wrong number of tunables for " OSNAME " " OSVERSION " (" PLATFORM ")");
#endif
#undef TUNABLE_VALUE


/* Do some preprocessor *magic*, part 2: tunable list */

#define TUNABLE_VALUE(TOP, NS, ID, TYPE, MIN, MAX, VAL, INIT, SECURITY, ALIAS) { #TOP "." #NS "." #ID, {Tunable::TYPE, static_cast<int64_t>(MIN), static_cast<int64_t>(MAX) }, VAL, INIT, Tunable::SECURITY, ALIAS },

struct Tunable {  // NOLINT
	/* Internal name of the tunable.  */
#if GLIBC_VERSION >= GLIBC_2_33
	char name[42];
#else
	const char *name;
#endif

	/* Data type of the tunable.  */
	enum TypeCode {
		TUNABLE_TYPE_INT_32,
		TUNABLE_TYPE_UINT_64,
		TUNABLE_TYPE_SIZE_T,
		TUNABLE_TYPE_STRING
	};
	struct Type {
		TypeCode type_code;
		int64_t min;
		int64_t max;
	} type;

	/* The value.  */
	union Val {
		int64_t numval;
		const char *strval;
	} val;

	/* Flag to indicate that the tunable is initialized.  */
	bool initialized;

	/* Specify the security level for the tunable with respect to AT_SECURE programs */
	enum SecLevel {
		/* Erase the tunable for AT_SECURE binaries so that child processes don't read it.  */
		TUNABLE_SECLEVEL_SXID_ERASE = 0,

		/* Ignore the tunable for AT_SECURE binaries, but don't erase it, so that child processes can read it.  */
		TUNABLE_SECLEVEL_SXID_IGNORE = 1,

		/* Read the tunable.  */
		TUNABLE_SECLEVEL_NONE = 2,
	} security_level;

	/* The compatibility environment variable name.  */
#if GLIBC_VERSION >= GLIBC_2_33
	char env_alias[23];
#else
	const char *env_alias;
#endif

	constexpr Tunable(const char * name, Type type, Val val, bool initialized, SecLevel security_level, const char *env_alias)
#if GLIBC_VERSION >= GLIBC_2_33
	  : type(type), val(val), initialized(initialized), security_level(security_level) {
			for (size_t i = 0; i < 42; i++)
				if ((this->name[i] = name[i]) == '\0')
					break;

			if (env_alias == nullptr) {
				this->env_alias[0] = '\0';
			} else {
				for (size_t i = 0; i < 23; i++)
					if ((this->env_alias[i] = env_alias[i]) == '\0')
						break;
			}
		}
#else
	  : name(name), type(type), val(val), initialized(initialized), security_level(security_level), env_alias(env_alias) {}
#endif
} tunables[] = {
	TUNABLE_LIST
};
static_assert(sizeof(tunables) / sizeof(Tunable) == _count, "Tunables struct and enum do not match for " OSNAME " " OSVERSION " (" PLATFORM ")");
#if GLIBC_TUNABLE_SIZE
static_assert(sizeof(tunables) == GLIBC_TUNABLE_SIZE, "Wrong size of tunables for " OSNAME " " OSVERSION " (" PLATFORM ")");
#endif

extern __attribute__((alias("tunables"), visibility("default"))) struct Tunable tunable_list;

EXPORT void __tunable_get_val(TunableID id, void * valp, void (*callback)(Tunable::Val *)) {
	assert(id < TunableID::_count);
	auto &cur = tunables[id];
	LOG_TRACE << "GLIBC __tunable_get_val " << static_cast<int>(id) << " (" << cur.name << ") = ";

	switch (cur.type.type_code) {
		case Tunable::TUNABLE_TYPE_UINT_64:
			LOG_TRACE_APPEND << static_cast<uint64_t>(cur.val.numval);
			*reinterpret_cast<uint64_t *>(valp) = static_cast<uint64_t>(cur.val.numval);
			break;
		case Tunable::TUNABLE_TYPE_INT_32:
			LOG_TRACE_APPEND << static_cast<int32_t>(cur.val.numval);
			*reinterpret_cast<int32_t *>(valp) = static_cast<int32_t>(cur.val.numval);
			break;
		case Tunable::TUNABLE_TYPE_SIZE_T:
			LOG_TRACE_APPEND << static_cast<size_t>(cur.val.numval);
			*reinterpret_cast<size_t *>(valp) = static_cast<size_t>(cur.val.numval);
			break;
		case Tunable::TUNABLE_TYPE_STRING:
			LOG_TRACE_APPEND << cur.val.strval;
			*reinterpret_cast<const char **>(valp) = cur.val.strval;
			break;
		default:
			assert(false);
	}
	if (cur.initialized && callback != nullptr)
		callback(&cur.val);
	LOG_TRACE_APPEND << endl;
}
#endif
