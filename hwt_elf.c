#include <sys/types.h>
#include <assert.h>
#include <fcntl.h>
#include <gelf.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int
hwt_elf_count_libs(const char *elf_path, int *nlibs0)
{
	const char *elfbase;
	const char *path;
	GElf_Shdr shdr, sh;
	GElf_Phdr ph;
	GElf_Ehdr eh;
	Elf_Scn *scn;
	Elf *elf;
	int is_dynamic;
	int nlibs;
	int fd;
	int i;

	size_t sh_entsize;
	Elf_Data *data;
	GElf_Dyn dyn;

	assert(elf_version(EV_CURRENT) != EV_NONE);

	fd = open(elf_path, O_RDONLY, 0);

	assert(fd >= 0);

	elf = elf_begin(fd, ELF_C_READ, NULL);

	assert(elf != NULL);
	assert(elf_kind(elf) == ELF_K_ELF);

	if (gelf_getehdr(elf, &eh) != &eh) {
		printf("could not find elf header\n");
		exit(2);
	}

	if (eh.e_type != ET_EXEC && eh.e_type != ET_DYN) {
		printf("unsupported image\n");
		exit(2);
	}

	/* Image type ELFCLASS32, ELFCLASS64 */
	//eh.e_ident[EI_CLASS];

	is_dynamic = 0;

	for (i = 0; i < eh.e_phnum; i++) {
		if (gelf_getphdr(elf, i, &ph) != &ph) {
			printf("could not get program header %d\n", i);
			exit(2);
		}
		switch (ph.p_type) {
		case PT_DYNAMIC:
			is_dynamic = 1;
			break;
		case PT_INTERP:
			nlibs++;
			break;
		}
	}

	if (!is_dynamic)
		goto done;

	scn = NULL;
	data = NULL;

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		assert(gelf_getshdr(scn, &shdr) == &shdr);

		if (shdr.sh_type == SHT_DYNAMIC) {
			data = elf_getdata(scn, data);
			assert(data != NULL);

			sh_entsize = gelf_fsize(elf, ELF_T_DYN, 1, EV_CURRENT);

			for (i = 0; i < shdr.sh_size / sh_entsize; i++) {
				assert(gelf_getdyn(data, i, &dyn) == &dyn);
				if (dyn.d_tag == DT_NEEDED)
					nlibs++;
			}
		}
	}

done:
	assert(elf_end(elf) == 0);
	assert(close(fd) == 0);

	*nlibs0 = nlibs;

	return (0);
}
