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
#include <string.h>

#include "hwt.h"
#include "hwt_var.h"
#include "cs.h"

#include "libpmcstat/libpmcstat.h"

struct pmcstat_process *
hwt_process_create(pmcstat_interned_string path)
{
	struct pmcstat_process *pp;

	if ((pp = malloc(sizeof(*pp))) == NULL)
		return (NULL);

#if 0
	pp->pp_pid = pid;
	pp->pp_isactive = 1;
#endif

	TAILQ_INIT(&pp->pp_map);

	return (pp);
}
