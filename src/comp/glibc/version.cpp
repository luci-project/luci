#include "comp/glibc/version.hpp"

#include <dlh/thread.hpp>
#include "comp/glibc/rtld/global.hpp"
#include "comp/glibc/libdl/interface.hpp"

#ifdef GLIBC_LINK_MAP_SIZE
static_assert(sizeof(GLIBC::DL::link_map) == GLIBC_LINK_MAP_SIZE, "Wrong size of link_map for " OSNAME " " OSVERSION " (" PLATFORM ")");
#else
#warning size of link_map was not checked
#endif

#ifdef GLIBC_RTLD_GLOBAL_SIZE
static_assert(sizeof(rtld_global) == GLIBC_RTLD_GLOBAL_SIZE, "Wrong size of rtld_global for " OSNAME " " OSVERSION " (" PLATFORM ")");
#else
#warning size of rtld_global was not checked
#endif

#ifdef GLIBC_RTLD_GLOBAL_RO_SIZE
static_assert(sizeof(rtld_global_ro) == GLIBC_RTLD_GLOBAL_RO_SIZE, "Wrong size of rtld_global_ro for " OSNAME " " OSVERSION " (" PLATFORM ")");
#else
#warning size of rtld_global_ro was not checked
#endif
