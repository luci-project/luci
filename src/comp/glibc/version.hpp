// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#define GLIBC_2_21 221
#define GLIBC_2_22 222
#define GLIBC_2_23 223
#define GLIBC_2_24 224
#define GLIBC_2_25 225
#define GLIBC_2_26 226
#define GLIBC_2_27 227
#define GLIBC_2_28 228
#define GLIBC_2_29 229
#define GLIBC_2_30 230
#define GLIBC_2_31 231
#define GLIBC_2_32 232
#define GLIBC_2_33 233
#define GLIBC_2_34 234
#define GLIBC_2_35 235
#define GLIBC_2_36 236

#if defined(PLATFORM_X64)
 #define PLATFORM "x86_64"

 #if defined(COMPATIBILITY_ARCH)
  #define OSNAME "Arch Linux"
  #if defined(COMPATIBILITY_ARCH_202211)
   #define OSVERSION "Nov 2022"
   #define GLIBC_VERSION GLIBC_2_36
   #define GLIBC_PTHREAD_IN_LIBC 1
   #define GLIBC_RTLD_GLOBAL_SIZE 4328
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 896
   #define GLIBC_LINK_MAP_SIZE 1184
   #define GLIBC_TUNABLE_COUNT 37
   #define GLIBC_TUNABLE_SIZE 4144

  #else
   #error Unsupported or unspecified arch linux version
  #endif

 #elif defined(COMPATIBILITY_DEBIAN)
  #define OSNAME "Debian"
  #if defined(COMPATIBILITY_DEBIAN_STRETCH)
   #define OSVERSION "9 (Stretch)"
   #define GLIBC_VERSION GLIBC_2_24
   #define GLIBC_PTHREAD_IN_LIBC 0
   #define GLIBC_RTLD_GLOBAL_SIZE 3968
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 376
   #define GLIBC_LINK_MAP_SIZE 1136
   #define GLIBC_TUNABLE_COUNT 0
   #define GLIBC_TUNABLE_SIZE 0

  #elif defined(COMPATIBILITY_DEBIAN_BUSTER)
   #define OSVERSION "10 (Buster)"
   // Its actually 2.28, but due to backports behaves like 2.31
   #define GLIBC_VERSION GLIBC_2_31
   #define GLIBC_PTHREAD_IN_LIBC 0
   #define GLIBC_RTLD_GLOBAL_SIZE 3992
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 432
   #define GLIBC_LINK_MAP_SIZE 1144
   #define GLIBC_TUNABLE_COUNT 24
   #define GLIBC_TUNABLE_SIZE 1344
   #define TUNABLE_LIST \
			GLIBC_TUNE_HWCAPS \
			GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
			GLIBC_MALLOC_TRIM_THRESHOLD \
			GLIBC_MALLOC_PERTURB \
			GLIBC_TUNE_X86_SHSTK \
			GLIBC_ELISION_TRIES \
			GLIBC_ELISION_ENABLE \
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

  #elif defined(COMPATIBILITY_DEBIAN_BULLSEYE)
   #define OSVERSION "11 (Bullseye)"
   #define GLIBC_VERSION GLIBC_2_31
   #define GLIBC_PTHREAD_IN_LIBC 0
   #define GLIBC_RTLD_GLOBAL_SIZE 4000
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 544
   #define GLIBC_LINK_MAP_SIZE 1152
   #define GLIBC_TUNABLE_COUNT 28
   #define GLIBC_TUNABLE_SIZE 1568
   #define TUNABLE_LIST \
			GLIBC_RTLD_NNS \
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
			GLIBC_RTLD_OPTIONAL_STATIC_TLS \
			GLIBC_MALLOC_TCACHE_MAX \
			GLIBC_MALLOC_CHECK

  #elif defined(COMPATIBILITY_DEBIAN_BOOKWORM)
   #define OSVERSION "12 (Bookworm)"
   #define GLIBC_VERSION GLIBC_2_36
   #define GLIBC_PTHREAD_IN_LIBC 1
   #define GLIBC_RTLD_GLOBAL_SIZE 4328
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 896
   #define GLIBC_LINK_MAP_SIZE 1184
   #define GLIBC_TUNABLE_COUNT 37
   #define GLIBC_TUNABLE_SIZE 4144

  #else
   #error Unsupported or unspecified debian version
  #endif


 #elif defined(COMPATIBILITY_FEDORA)
  #define OSNAME "Fedora Linux"
  #if defined(COMPATIBILITY_FEDORA_36)
   #define OSVERSION "36"
   #define GLIBC_VERSION GLIBC_2_35
   #define GLIBC_PTHREAD_IN_LIBC 1
   #define GLIBC_RTLD_GLOBAL_SIZE 4304
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 928
   #define GLIBC_LINK_MAP_SIZE 1160
   #define GLIBC_TUNABLE_COUNT 35
   #define GLIBC_TUNABLE_SIZE 3920

  #elif defined(COMPATIBILITY_FEDORA_37)
   #define OSVERSION "37"
   #define GLIBC_VERSION GLIBC_2_36
   #define GLIBC_PTHREAD_IN_LIBC 1
   #define GLIBC_RTLD_GLOBAL_SIZE 4336
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 896
   #define GLIBC_LINK_MAP_SIZE 1192
   #define GLIBC_TUNABLE_COUNT 37
   #define GLIBC_TUNABLE_SIZE 4144

  #else
   #error Unsupported or unspecified fedora version
  #endif



 #elif defined(COMPATIBILITY_RHEL) || defined(COMPATIBILITY_ALMALINUX) || defined(COMPATIBILITY_OL)
  #if defined(COMPATIBILITY_ALMALINUX)
   #define OSNAME "AlmaLinux OS"
  #elif defined(COMPATIBILITY_OL)
   #define OSNAME "Oracle Linux"
  #else
   #define OSNAME "RedHat Enterprise Linux"
  #endif
  #if defined(COMPATIBILITY_RHEL_8) || defined(COMPATIBILITY_ALMALINUX_8) || defined(COMPATIBILITY_OL_8)
   #define OSVERSION "8"
   #define COMPATIBILITY_RHEL_8_LIKE 1
   #define GLIBC_VERSION GLIBC_2_28
   #define GLIBC_PTHREAD_IN_LIBC 0
   #define GLIBC_RTLD_GLOBAL_SIZE 4152
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 688
   #define GLIBC_LINK_MAP_SIZE 1152
   #define GLIBC_TUNABLE_COUNT 29
   #define GLIBC_TUNABLE_SIZE 1624
   #define TUNABLE_LIST \
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
			GLIBC_RTLD_OPTIONAL_STATIC_TLS \
			GLIBC_MALLOC_TCACHE_MAX \
			GLIBC_MALLOC_CHECK

  #elif defined(COMPATIBILITY_RHEL_9) || defined(COMPATIBILITY_ALMALINUX_9)
   #define OSVERSION "9"
   #define COMPATIBILITY_RHEL_9_LIKE 1
   #define GLIBC_VERSION GLIBC_2_34
   #define GLIBC_PTHREAD_IN_LIBC 1
   #define GLIBC_RTLD_GLOBAL_SIZE 4176
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 912
   #define GLIBC_LINK_MAP_SIZE 1160
   #define GLIBC_TUNABLE_COUNT 34
   #define GLIBC_TUNABLE_SIZE 3808
   #define TUNABLE_LIST \
			GLIBC_RTLD_NNS \
			GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
			GLIBC_MALLOC_TRIM_THRESHOLD \
			GLIBC_MALLOC_PERTURB \
			GLIBC_CPU_X86_SHARED_CACHE_SIZE \
			GLIBC_PTHREAD_RSEQ \
			GLIBC_MEM_TAGGING \
			GLIBC_ELISION_TRIES \
			GLIBC_ELISION_ENABLE \
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

  #elif defined(COMPATIBILITY_OL_9)
   #define OSVERSION "9"
   #define COMPATIBILITY_RHEL_9_LIKE 1
   #define GLIBC_VERSION GLIBC_2_34
   #define GLIBC_PTHREAD_IN_LIBC 1
   #define GLIBC_RTLD_GLOBAL_SIZE 4176
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 912
   #define GLIBC_LINK_MAP_SIZE 1160
   #define GLIBC_TUNABLE_COUNT 35
   #define GLIBC_TUNABLE_SIZE 3920
   #define TUNABLE_LIST \
			GLIBC_RTLD_NNS \
			GLIBC_ELISION_SKIP_LOCK_AFTER_RETRIES \
			GLIBC_MALLOC_TRIM_THRESHOLD \
			GLIBC_MALLOC_PERTURB \
			GLIBC_CPU_X86_SHARED_CACHE_SIZE \
			GLIBC_PTHREAD_RSEQ \
			GLIBC_MEM_TAGGING \
			GLIBC_ELISION_TRIES \
			GLIBC_ELISION_ENABLE \
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
			GLIBC_PTHREAD_STACK_HUGETLB \

  #else
   #error Unsupported or unspecified almalinux / oracle linux / redhat enterprise linux version
  #endif



 #elif defined(COMPATIBILITY_OPENSUSELEAP)
  #define OSNAME "OpenSUSE Leap"
  #if defined(COMPATIBILITY_OPENSUSELEAP_15)
   #define OSVERSION "15"
   #define GLIBC_VERSION GLIBC_2_31
   // GLIBC_PTHREAD_IN_LIBC aka THREAD_GSCOPE_IN_TCB
   #define GLIBC_PTHREAD_IN_LIBC 0
   #define GLIBC_RTLD_GLOBAL_SIZE 4000
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 544
   #define GLIBC_LINK_MAP_SIZE 1152
   #define GLIBC_TUNABLE_COUNT 26
   #define GLIBC_TUNABLE_SIZE 1456

  #else
   #error Unsupported or unspecified opensuse leap version
  #endif



 #elif defined(COMPATIBILITY_UBUNTU)
  #define OSNAME "Ubuntu"
  #if defined(COMPATIBILITY_UBUNTU_FOCAL)
   #define OSVERSION "20.04 (Focal Fossa)"
   #define GLIBC_VERSION GLIBC_2_31
   // GLIBC_PTHREAD_IN_LIBC aka THREAD_GSCOPE_IN_TCB
   #define GLIBC_PTHREAD_IN_LIBC 0
   #define GLIBC_RTLD_GLOBAL_SIZE 3992
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 536
   #define GLIBC_LINK_MAP_SIZE 1152
   #define GLIBC_TUNABLE_COUNT 26
   #define GLIBC_TUNABLE_SIZE 1456


  #elif defined(COMPATIBILITY_UBUNTU_JAMMY)
   #define OSVERSION "22.04 (Jammy Jellyfish)"
   #define GLIBC_VERSION GLIBC_2_35
   #define GLIBC_PTHREAD_IN_LIBC 1
   #define GLIBC_RTLD_GLOBAL_SIZE 4304
   #define GLIBC_RTLD_GLOBAL_RO_SIZE 928
   #define GLIBC_LINK_MAP_SIZE 1160
   #define GLIBC_TUNABLE_COUNT 35
   #define GLIBC_TUNABLE_SIZE 3920

  #else
   #error Unsupported or unspecified ubuntu version
  #endif


 #else
   #error Unsupported or unspecified distribution
 #endif


#else
  #error Unsupported or unspecified platform
#endif
