#include "libpmcstat/libpmcstat.h"

struct trace_context {
	struct pmcstat_process *pp;
	struct hwt_record_user_entry *records;
	void *base;
	int cpu;
	int hwt_id;
	int bufsize;
};

struct pmcstat_process * hwt_process_test(void);
struct pmcstat_process *
hwt_process_create(pmcstat_interned_string path, struct pmcstat_args *args,
    struct pmc_plugins *plugins);
