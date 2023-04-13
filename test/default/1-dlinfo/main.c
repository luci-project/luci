#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <link.h>
#include <assert.h>

int main(int argc, char * argv[]) {
	printf("\n[%s]", argv[argc - 1]);

	void *handle = dlopen(argc == 1 ? NULL : argv[argc - 1], RTLD_NOW);
	if (!handle) {
		fprintf(stderr, "dlopen: %s\n", dlerror());
		return EXIT_FAILURE;
	}

	puts("\nLink map:");
	struct link_map * info_map = NULL;
	if (dlinfo(handle, RTLD_DI_LINKMAP, &info_map) != 0) {
		fprintf(stderr, "dlinfo (linkmap): %s\n", dlerror());
		return EXIT_FAILURE;
	}
	int pos = 0;
	struct link_map * map = info_map;
	for (; map->l_prev != NULL; map = map->l_prev)
		pos--;
	for (; map != NULL; map = map->l_next) {
		assert(map->l_name != NULL);
		char * name = pos == 0 ? argv[argc - 1] : map->l_name;
		void * addr = (void*)(map->l_addr);
		void * dynamic = (void*)(((uintptr_t)map->l_ld) - ((uintptr_t)map->l_addr));
		printf("%3d: %s @ %p (dynamic +%p)\n", pos++, name, addr, dynamic);
	}

	puts("\nSearch paths:");
	Dl_serinfo serinfo;
	if (dlinfo(handle, RTLD_DI_SERINFOSIZE, &serinfo) != 0) {
		fprintf(stderr, "dlinfo (serinfosize dls_size): %s\n", dlerror());
		return EXIT_FAILURE;
	}
	Dl_serinfo *sip = malloc(serinfo.dls_size);
	if (sip == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	if (dlinfo(handle, RTLD_DI_SERINFOSIZE, sip) != 0) {
		fprintf(stderr, "dlinfo (serinfosize buffer size / count): %s\n", dlerror());
		return EXIT_FAILURE;
	}
	if (dlinfo(handle, RTLD_DI_SERINFO, sip) == -1) {
		fprintf(stderr, "dlinfo (serinfosize buffer): %s\n", dlerror());
		return EXIT_FAILURE;
	}
	for (unsigned i = 0; i < serinfo.dls_cnt; i++)
		printf("%3d. %s (%x)\n", i, sip->dls_serpath[i].dls_name, sip->dls_serpath[i].dls_flags);

	dlclose(handle);
	return EXIT_SUCCESS;
}
