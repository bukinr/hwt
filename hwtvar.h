#include "libpmcstat/libpmcstat.h"

struct trace_context {
	struct pmcstat_process *pp;
	struct hwt_record_user_entry *records;
	void *base;
	int bufsize;
	int cpu_id;
	int pid;
};

struct pmcstat_process *hwt_process_alloc(void);
int hwt_create_process(int *sockpair, char **cmd, char **env, int *pid0);
int hwt_start_process(int *sockpair);
