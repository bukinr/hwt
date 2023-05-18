#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/errno.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sysexits.h>

#include "hwt.h"
#include "hwt_var.h"
#include "cs.h"

#include "libpmcstat/libpmcstat.h"

void
hwt_load_lib(struct pmcstat_process *pp, char *filepath,
    struct pmcstat_args *args, struct pmc_plugins *plugins)
{
	pmcstat_interned_string image_path;
	struct pmcstat_image *image;

	image_path = pmcstat_string_intern(filepath);
	image = pmcstat_image_from_path(image_path, 0, args, plugins);

	return;
	//pmcstat_image_link(pp, image, 0); //START
}

void
hwt_load_dynamic_libs(struct pmcstat_process *pp, pmcstat_interned_string path,
    struct pmcstat_args *args, struct pmc_plugins *plugins)
{
	const char *elfbase;
	const char *elf_path;
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
	char *filepath;

	elf_path = pmcstat_string_unintern(path);

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
				if (dyn.d_tag == DT_NEEDED) {
					filepath = elf_strptr(elf, shdr.sh_link,
					     dyn.d_un.d_val);
					printf("Shared lib: %s\n", filepath);
					hwt_load_lib(pp, filepath, args,
					    plugins);
				}
			}
		}
	}

	assert(elf_end(elf) == 0);
	assert(close(fd) == 0);
}

struct pmcstat_process *
hwt_process_create(pmcstat_interned_string path, struct pmcstat_args *args,
    struct pmc_plugins *plugins)
{
	struct pmcstat_stats pmcstat_stats;
	struct pmcstat_process *pp;
	uintptr_t entryaddr;

	if ((pp = malloc(sizeof(*pp))) == NULL)
		err(EX_OSERR, "ERROR: Cannot allocate pid descriptor");

#if 0
	pp->pp_pid = pid;
	pp->pp_isactive = 1;
#endif

        TAILQ_INIT(&pp->pp_map);

	entryaddr = 0x10000;
	entryaddr = 0x270000;

	pmcstat_process_exec(pp, path, entryaddr, args, plugins,
	    &pmcstat_stats);

	return (pp);
}

void
hwt_lookup(struct pmcstat_process *pp, uintptr_t ip)
{
	struct pmcstat_image *image;
	struct pmcstat_symbol *sym;
	struct pmcstat_pcmap *map;
	uint64_t newpc;

	//ip = 0x11264;

	map = pmcstat_process_find_map(pp, ip);
	if (map != NULL) {
		image = map->ppm_image;
		newpc = ip - (map->ppm_lowpc +
		    (image->pi_vaddr - image->pi_start));

		sym = pmcstat_symbol_search(image, newpc);
		//*img = image;

		if (sym == NULL)
			printf("symbol 0x%lx not found\n", newpc);
		else
			printf("symbol found: %s\n",
			    pmcstat_string_unintern(sym->ps_name));
	}
}

struct pmcstat_process *
hwt_process_test(void)
{
	struct pmcstat_process *pp;
	struct pmc_plugins plugins;
	struct pmcstat_args args;
	pmcstat_interned_string path;
	uintptr_t ip;

	path = pmcstat_string_intern("/usr/bin/uname");

	memset(&args, 0, sizeof(struct pmcstat_args));
	memset(&plugins, 0, sizeof(struct pmc_plugins));

	args.pa_fsroot = "/";
	ip = 0x11264;

	pp = hwt_process_create(path, &args, &plugins);
	//hwt_load_dynamic_libs(pp, path, &args, &plugins);
	//hwt_lookup(pp, ip);

	return (pp);
}
