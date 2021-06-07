#pragma once

#ifndef STRINGIFY
/*! \def STRINGIFY(S)
 *  \brief Converts a macro parameter into a string
 *  \ingroup debug
 *  \param S Expression to be converted
 *  \return stringified version of S
 */
#define STRINGIFY(S) #S
#endif

/*! \def assert(EXP)
 *  \brief Ensure (at execution time) an expression evaluates to `true`, print an error message and stop the CPU otherwise.
 *  \ingroup debug
 *  \param EXP The expression to be checked
 */
#ifdef NDEBUG
#define assert(EXP) ((void)0)
#else
#define assert(EXP) \
	do { \
		if (__builtin_expect(!(EXP), 0)) { \
			__assert_fail(STRINGIFY(EXP), __FILE__, __LINE__, __PRETTY_FUNCTION__); \
		} \
	} while (false)

/*! \brief Handles a failed assertion
 *
 *  This function will print a message containing further information about the
 *  failed assertion and stops the current CPU permanently.
 *
 *  \note This function should never be called directly, but only via the macro `assert`.
BEGIN_TEMPLATE(1)
 *
 *  \todo Implement Remainder of Method (output & CPU stopping)
END_TEMPLATE(1)
 *
 *  \param exp  Expression that did not hold
 *  \param file Name of the file in which the assertion failed
 *  \param line Line in which the assertion failed
 *  \param func Name of the function in which the assertion failed
 */
[[noreturn]] void __assert_fail(const char * exp, const char * file, int line, const char * func);
#endif
