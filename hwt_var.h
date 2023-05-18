struct trace_context {
	struct pmcstat_process *pp;
	struct hwt_mmap_user_entry *mmaps;
	void *base;
	int cpu;
	int hwt_id;
	int bufsize;
};

struct pmcstat_process * hwt_process_test(void);
