#include <sys/types.h>
#include <assert.h>
#include <fcntl.h>
#include <gelf.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define min(A,B)                ((A) < (B) ? (A) : (B))
#define max(A,B)                ((A) > (B) ? (A) : (B))

void
add_symbols(Elf *elf, Elf_Scn *scn, GElf_Shdr *sh)
{
	size_t nshsyms, nfuncsyms;
	Elf_Data *data;
	GElf_Sym sym;
	int i;

	printf("adding symbols\n");

	if ((data = elf_getdata(scn, NULL)) == NULL)
		return;

	nshsyms = sh->sh_size / sh->sh_entsize;
	nfuncsyms = 0;

	for (i = 0; i < nshsyms; i++) {
		if (gelf_getsym(data, i, &sym) != &sym)
			return;

		if (GELF_ST_TYPE(sym.st_info) == STT_FUNC)
			nfuncsyms++;
	}

	printf("nfuncs %ju\n", nfuncsyms);
}

void
find_libs(const char *elf_path)
{
	size_t sh_entsize, nph, nsh;
	GElf_Shdr shdr;
	GElf_Phdr ph;
	GElf_Dyn dyn;
	Elf_Data *data;
	Elf_Scn *scn;
	Elf *elf;
	GElf_Shdr sh;
	int fd;
	int i;

	assert(elf_version(EV_CURRENT) != EV_NONE);

	fd = open(elf_path, O_RDONLY, 0);

	assert(fd >= 0);

	elf = elf_begin(fd, ELF_C_READ, NULL);

	assert(elf != NULL);
	assert(elf_kind(elf) == ELF_K_ELF);

	GElf_Ehdr eh;

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

	if (elf_getphnum(elf, &nph) == 0) {
		printf("the number of program headers is unknown\n");
		exit(2);
	}

	for (i = 0; i < eh.e_phnum; i++) {
		if (gelf_getphdr(elf, i, &ph) != &ph) {
			printf("could not get program header %d\n", i);
			exit(2);
		}
		switch (ph.p_type) {
		case PT_DYNAMIC:
			printf("%s dyn\n", __func__);
			break;
		case PT_INTERP:
			printf("%s interp\n", __func__);
			break;
		case PT_LOAD:
			if ((ph.p_flags & PF_X) != 0) {
				printf("%s load\n", __func__);
				printf("vaddr %lx\n", ph.p_vaddr & (-ph.p_align));
			}
			break;
		}
	}

	if (elf_getshnum(elf, &nsh) == 0) {
		printf("the number of sections is unknown\n");
		exit(2);
	}

	uintptr_t minva, maxva;
	minva = ~(uintptr_t) 0;
	maxva = (uintptr_t) 0;

	for (i = 0; i < nsh; i++) {
		scn = elf_getscn(elf, i);
		if (scn == NULL) {
			printf("could not get section %d\n", i);
			exit(2);
		}

		if (gelf_getshdr(scn, &sh) != &sh) {
			printf("could not get section header %d\n", i);
			exit(2);
		}

		if (sh.sh_flags & SHF_EXECINSTR) {
			printf("minva %lx\n", (unsigned long)min(minva, sh.sh_addr));
			printf("maxva %lx\n", (unsigned long)max(maxva, sh.sh_addr + sh.sh_size));
		}

		if (sh.sh_type == SHT_SYMTAB || sh.sh_type == SHT_DYNSYM)
			add_symbols(elf, scn, &sh);
	}

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
