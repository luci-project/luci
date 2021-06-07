#include <dlh/utils/auxiliary.hpp>

extern char **environ;

Auxiliary Auxiliary::vector(Auxiliary::type type) {
	static int envc = -1;
	if (envc == -1)
		for (envc = 0; environ[envc] != nullptr; envc++) {}

	// Read current auxiliary vectors
	Auxiliary * auxv = reinterpret_cast<Auxiliary *>(environ + envc + 1);
	for (int auxc = 0 ; auxv[auxc].a_type != Auxiliary::AT_NULL; auxc++)
		if (auxv[auxc].a_type == type)
			return auxv[auxc];

	return {};
}
