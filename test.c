#include <assert.h>
#include <fcntl.h>
#include <gelf.h>
#include <stdio.h>
#include <unistd.h>

void
find_libs(const char *elf_path)
{
	size_t sh_entsize;
	GElf_Shdr shdr;
	GElf_Dyn dyn;
	Elf_Data *data;
	Elf_Scn *scn;
	Elf *elf;
	int fd;
	int i;

	assert(elf_version(EV_CURRENT) != EV_NONE);

	fd = open(elf_path, O_RDONLY, 0);

	assert(fd >= 0);

	elf = elf_begin(fd, ELF_C_READ, NULL);

	assert(elf != NULL);
	assert(elf_kind(elf) == ELF_K_ELF);

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
					printf("Shared lib: %s\n",
					    elf_strptr(elf, shdr.sh_link, dyn.d_un.d_val));
			}
		}
	}

	assert(elf_end(elf) == 0);
	assert(close(fd) == 0);
}

int
main(int argc, char **argv)
{
	char **cmd;

	cmd = argv + 1;

	find_libs(cmd[0]);

	return (0);
}
