#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/errno.h>

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
hwt_process_test(void)
{
	struct pmcstat_process *pp;

	if ((pp = malloc(sizeof(*pp))) == NULL)
		err(EX_OSERR, "ERROR: Cannot allocate pid descriptor");

#if 0
	pp->pp_pid = pid;
	pp->pp_isactive = 1;
#endif

        TAILQ_INIT(&pp->pp_map);

	pmcstat_interned_string path;
	path = pmcstat_string_intern("/usr/bin/uname");

	uintptr_t entryaddr;
	struct pmcstat_args args;
	args.pa_fsroot = "/";
	struct pmc_plugins plugins;
	struct pmcstat_stats pmcstat_stats;

	entryaddr = 0;

	pmcstat_process_exec(pp, path, entryaddr, &args, &plugins,
	    &pmcstat_stats);

	struct pmcstat_image *image;
	struct pmcstat_symbol *sym;
	struct pmcstat_pcmap *map;
	uint64_t newpc;
	uintptr_t ip;

	ip = 0x11264;

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
