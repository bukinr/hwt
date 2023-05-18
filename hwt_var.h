struct trace_context {
	int hwt_id;
	int bufsize;
	void *base;
	struct hwt_mmap_user_entry *mmaps;
	struct pmcstat_process *pp;
	int cpu;
};
