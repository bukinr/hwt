struct trace_context {
	struct pmcstat_process *pp;
	struct hwt_record_user_entry *records;
	void *base;
	int cpu;
	int hwt_id;
	int bufsize;
};

struct pmcstat_process * hwt_process_test(void);
