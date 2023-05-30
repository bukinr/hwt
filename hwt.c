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
#include <string.h>

#include "hwt.h"
#include "hwt_var.h"
#include "cs.h"

#define	NSOCKPAIRFD		2
#define	PARENTSOCKET		0
#define	CHILDSOCKET		1

extern char **environ;
static struct trace_context tcs[4];

#include "libpmcstat/libpmcstat.h"
struct pmcstat_image_hash_list pmcstat_image_hash[PMCSTAT_NHASH];

static int
hwt_create_process(int *sockpair, char **cmd, char **env, int *pid0)
{
	char token;
	pid_t pid;
	int error;

	printf("cmd %s\n", *cmd);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) < 0)
		return (-1);

	pid = fork();

	switch (pid) {
	case -1:
		return (-1);
	case 0:
		/* Child */
		close(sockpair[PARENTSOCKET]);

		error = write(sockpair[CHILDSOCKET], "+", 1);
		if (error != 1)
			return (-1);

		error = read(sockpair[CHILDSOCKET], &token, 1);
		if (error < 0)
			return (-2);

		if (token != '!')
			return (-3);

		close(sockpair[CHILDSOCKET]);

		execvp(cmd[0], cmd);

		kill(getppid(), SIGCHLD);

		exit(-3);
	default:
		/* Parent */
		close(sockpair[CHILDSOCKET]);
		break;
	}

	*pid0 = pid;

	return (0);
}

static int
hwt_start_process(int *sockpair)
{
	int error;

	error = write(sockpair[PARENTSOCKET], "!", 1);
	if (error != 1)
		return (-1);

	return (0);
}

static int
hwt_alloc_ctx(int fd, struct trace_context *tc)
{
	struct hwt_alloc al;
	int error;

	al.cpu_id = tc->cpu_id;
	al.pid = tc->pid;

	error = ioctl(fd, HWT_IOC_ALLOC, &al);
	if (error != 0)
		return (error);

	return (0);
}

static int
hwt_map_memory(struct trace_context *tc)
{
	char filename[32];
	int fd;

	sprintf(filename, "/dev/hwt_%d%d", tc->cpu_id, tc->pid);

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		printf("Can't open %s\n", filename);
		return (-1);
	}

	tc->base = mmap(NULL, tc->bufsize, PROT_READ, MAP_SHARED, fd, 0);
	if (tc->base == MAP_FAILED) {
		printf("mmap failed: err %d\n", errno);
		return (-1);
	}

	printf("%s: tc->base %#p\n", __func__, tc->base);

	return (0);
}

int
main(int argc, char **argv, char **env)
{
	struct hwt_start s;
	char filename[20];
	struct trace_context *tc;
	char **cmd;
	int error;
	int fd;
	int i;
	int sockpair[NSOCKPAIRFD];
	int pid;

	sprintf(filename, "/dev/hwt");

	cmd = argv + 1;

	printf("cmd is %s\n", *cmd);

	error = hwt_create_process(sockpair, cmd, env, &pid);
	if (error != 0)
		return (error);

	printf("%s: process pid %d created\n", __func__, pid);

#if 0
	while (1)
		sleep(5);
	return (0);
#endif

	fd = open(filename, O_RDWR);
	if (fd < 0) {
		printf("Can't open %s\n", filename);
		return (-1);
	}

	for (i = 0; i < 4; i++) {
		tc = &tcs[i];

		if (i == 0)
			tc->bufsize = 16 * 1024 * 1024;

		tc->cpu_id = i;
		tc->pid = pid;

		error = hwt_alloc_ctx(fd, tc);
		printf("%s: alloc_ctx cpu_id %d pid %d error %d\n",
		    __func__, tc->cpu_id, tc->pid, error);
		if (error)
			return (error);

		if (i == 0) {
			error = hwt_map_memory(tc);
			if (error != 0) {
				printf("can't map memory");
				return (error);
			}
		}

		s.cpu_id = tc->cpu_id;
		s.pid = tc->pid;
		error = ioctl(fd, HWT_IOC_START, &s);
		printf("%s: ioctl start returned %d\n", __func__, error);
		if (error)
			return (error);
	}

	error = hwt_start_process(sockpair);
	if (error != 0)
		return (error);

	printf("waiting proc to finish\n");
	sleep(1);

	struct hwt_record_get record_get;
	int nentries;
	int j;

	nentries = 256;

	unsigned long addr;
	struct pmcstat_process *pp;
	struct pmc_plugins plugins;
	struct pmcstat_args args;
	pmcstat_interned_string path;
	struct pmcstat_image *image;

	memset(&plugins, 0, sizeof(struct pmc_plugins));
	memset(&args, 0, sizeof(struct pmcstat_args));
	args.pa_fsroot = "/";

	path = pmcstat_string_intern(*cmd);
	pp = hwt_process_create(path, &args, &plugins);

	printf("%s: pp %#p\n", __func__, pp);

	for (i = 0; i < 4; i++) {
		tc = &tcs[i];
		tc->cpu_id = i;
		tc->pp = pp;
		tc->records = malloc(sizeof(struct hwt_record_user_entry) * 1024);
		record_get.pid = pid;
		record_get.cpu_id = tc->cpu_id;
		record_get.records = tc->records;
		record_get.nentries = &nentries;
		error = ioctl(fd, HWT_IOC_RECORD_GET, &record_get);
		printf("RECORD_GET cpuid %d error %d entires %d\n", i, error, nentries);
		if (error != 0 || nentries == 0)
                        continue;

		for (j = 0; j < nentries; j++) {
			printf("  lib #%d: path %s addr %lx size %lx\n", j,
			    tc->records[j].fullpath,
			    (unsigned long)tc->records[j].addr,
			    tc->records[j].size);

			path = pmcstat_string_intern(tc->records[j].fullpath);
			if ((image = pmcstat_image_from_path(path, 0,
			    &args, &plugins)) == NULL)
				return (-1);

			if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
				pmcstat_image_determine_type(image, &args);

			addr = (unsigned long)tc->records[j].addr & ~1;
			addr -= (image->pi_start - image->pi_vaddr);
			pmcstat_image_link(pp, image, addr);
			printf("image pi_vaddr %lx pi_start %lx pi_entry %lx\n",
			    (unsigned long)image->pi_vaddr,
			    (unsigned long)image->pi_start,
			    (unsigned long)image->pi_entry);
		}
	}

	close(fd);

	sleep(1);

	tc = &tcs[0];
	cs_init(tc);

	printf("processing\n");
	cs_process_chunk(tc);

	return (0);
}
