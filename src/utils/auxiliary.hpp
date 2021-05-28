#pragma once

/*! \brief Auxiliary vector */
struct alignas(16) Auxiliary {
	enum type : long int {
		AT_NULL              =  0,  ///< End of vector
		AT_IGNORE            =  1,  ///< Entry should be ignored
		AT_EXECFD            =  2,  ///< File descriptor of program
		AT_PHDR              =  3,  ///< Program headers for program
		AT_PHENT             =  4,  ///< Size of program header entry
		AT_PHNUM             =  5,  ///< Number of program headers
		AT_PAGESZ            =  6,  ///< System page size
		AT_BASE              =  7,  ///< Base address of interpreter
		AT_FLAGS             =  8,  ///< Flags
		AT_ENTRY             =  9,  ///< Entry point of program
		AT_NOTELF            = 10,  ///< Program is not ELF
		AT_UID               = 11,  ///< Real uid
		AT_EUID              = 12,  ///< Effective uid
		AT_GID               = 13,  ///< Real gid
		AT_EGID              = 14,  ///< Effective gid
		AT_CLKTCK            = 17,  ///< Frequency of times()
		AT_PLATFORM          = 15,  ///< String identifying platform.
		AT_HWCAP             = 16,  ///< Machine-dependent hints about processor capabilities.
		AT_FPUCW             = 18,  ///< Used FPU control word.
		AT_DCACHEBSIZE       = 19,  ///< Data cache block size.
		AT_ICACHEBSIZE       = 20,  ///< Instruction cache block size.
		AT_UCACHEBSIZE       = 21,  ///< Unified cache block size.
		AT_IGNOREPPC         = 22,  ///< Entry should be ignored.
		AT_SECURE            = 23,  ///< Boolean, was exec setuid-like?
		AT_BASE_PLATFORM     = 24,  ///< String identifying real platforms
		AT_RANDOM            = 25,  ///< Address of 16 random bytes
		AT_HWCAP2            = 26,  ///< More machine-dependent hints about processor capabilities
		AT_EXECFN            = 31,  ///< Filename of executable
		AT_SYSINFO           = 32,  ///< Pointer to the global system page used for system calls
		AT_SYSINFO_EHDR      = 33,
		AT_L1I_CACHESHAPE    = 34,
		AT_L1D_CACHESHAPE    = 35,
		AT_L2_CACHESHAPE     = 36,
		AT_L3_CACHESHAPE     = 37,
		AT_L1I_CACHESIZE     = 40,
		AT_L1I_CACHEGEOMETRY = 41,
		AT_L1D_CACHESIZE     = 42,
		AT_L1D_CACHEGEOMETRY = 43,
		AT_L2_CACHESIZE      = 44,
		AT_L2_CACHEGEOMETRY  = 45,
		AT_L3_CACHESIZE      = 46,
		AT_L3_CACHEGEOMETRY  = 47,
		AT_MINSIGSTKSZ       = 51,  ///< Stack needed for signal delivery
	} a_type;
	union {
		long int a_val;
		void * a_ptr;
		void (*a_fcn) (void);
	} a_un;

	bool valid() const {
		return a_type != AT_NULL;
	}

	static Auxiliary vector(Auxiliary::type type);

	Auxiliary(type t = AT_NULL, long int v = 0) : a_type(t), a_un({v}) {}
} __attribute__((packed));

static_assert(sizeof(Auxiliary) == 2 * sizeof(void*), "Auxiliary vector has wrong size!");
