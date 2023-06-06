/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Ruslan Bukin <br@bsdpad.com>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "hwt.h"
#include "hwtvar.h"
#include "hwt_coresight.h"

#define	PARENTSOCKET		0
#define	CHILDSOCKET		1
#define	NSOCKPAIRFD		2

struct pmcstat_image_hash_list pmcstat_image_hash[PMCSTAT_NHASH];
static struct trace_context tcs[4];

#include "libpmcstat/libpmcstat.h"

static void
hwt_sleep(void)
{
	struct timespec time_to_sleep;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 10000000; /* 10 ms */

	nanosleep(&time_to_sleep, &time_to_sleep);
}

void
hwt_procexit(pid_t pid)
{
	struct trace_context *tc;
	int i;

	for (i = 0; i < 4; i++) {
		tc = &tcs[i];
		if (tc->pid == pid)
			tc->terminate = 1;
	}
}

static int
hwt_ctx_alloc(int fd, struct trace_context *tc)
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

static size_t
get_offs(struct trace_context *tc, size_t *offs)
{
	struct hwt_bufptr_get bget;
	vm_offset_t curpage_offset;
	int curpage;
	int error;
	int ptr;

	bget.cpu_id = tc->cpu_id;
	bget.pid = tc->pid;
	bget.ptr = &ptr;
	bget.curpage = &curpage;
	bget.curpage_offset = &curpage_offset;
	error = ioctl(tc->fd, HWT_IOC_BUFPTR_GET, &bget);
	if (error)
		return (error);

	printf("curpage %d curpage_offset %ld\n", curpage, curpage_offset);

	*offs = curpage * PAGE_SIZE + curpage_offset;

	return (0);
}

int
main(int argc, char **argv, char **env)
{
	struct pmcstat_process *pp;
	struct trace_context *tc;
	struct hwt_start s;
	char **cmd;
	int error;
	int fd;
	int i;
	int sockpair[NSOCKPAIRFD];
	int pid;

	cmd = argv + 1;

	printf("cmd is %s\n", *cmd);

	error = hwt_process_create(sockpair, cmd, env, &pid);
	if (error != 0)
		return (error);

	printf("%s: process pid %d created\n", __func__, pid);

	fd = open("/dev/hwt", O_RDWR);
	if (fd < 0) {
		printf("Can't open /dev/hwt\n");
		return (-1);
	}

	pp = hwt_process_alloc();
	pp->pp_pid = pid;
	pp->pp_isactive = 1;

	for (i = 0; i < 4; i++) {
		tc = &tcs[i];
		tc->cpu_id = i;
		tc->pp = pp;
		tc->pid = pid;
		tc->fd = fd;

		if (i == 0)
			tc->bufsize = 16 * 1024 * 1024;

		error = hwt_ctx_alloc(fd, tc);
		if (error) {
			printf("%s: failed to alloc ctx, cpu_id %d pid %d error %d\n",
			    __func__, tc->cpu_id, tc->pid, error);
			return (error);
		}

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
		if (error) {
			printf("%s: failed to start tracing, error %d\n", __func__, error);
			return (error);
		}
	}

	error = hwt_process_start(sockpair);
	if (error != 0)
		return (error);

#if 0
	int status;

	printf("waiting proc to finish\n");

	wait(&status);

	if (WIFEXITED(status))
		printf("child complete with ret %d\n",
		    WEXITSTATUS(status));
	else
		printf("child complete with signal %d\n",
		    WTERMSIG(status));
#endif

	int nrecords;
	int tot_records;

	nrecords = 0;
	tot_records = 0;

	do {
		for (i = 0; i < 4; i++) {
			tc = &tcs[i];
			error = hwt_record_fetch(tc, &nrecords);
			if (error == 0)
				tot_records += nrecords;
		}
	} while (tot_records < 3);

	printf("processing\n");

	/* Coresight data is always on CPU0 due to funnelling by HW. */
	tc = &tcs[0];
	cs_init(tc);

	size_t offs;
	error = get_offs(tc, &offs);

	if (error)
		return (-1);

	printf("data to process %ld\n", offs);

	size_t start;
	size_t end;

	start = 0;
	end = offs;

	cs_process_chunk(tc, start, end);

	while (1) {
		if (tc->terminate)
			break;

		error = get_offs(tc, &offs);
		if (error)
			return (-1);

//printf("offs %ld new_offs %ld\n", end, offs);
		if (offs == end) {
			/* No new entries in trace. */
			hwt_sleep();
			continue;
		}

		if (offs > end) {
			/* New entries in the trace buffer. */
			start = end;
			end = offs;
			cs_process_chunk(tc, start, end);
			hwt_sleep();
			continue;
		}

		if (offs < end) {
			/* New entries in the trace buffer. Buffer wrapped. */
			start = end;
			end = tc->bufsize;
			cs_process_chunk(tc, start, end);

			start = 0;
			end = offs;
			cs_process_chunk(tc, start, end);

			hwt_sleep();
		}
	}

	close(fd);

	return (0);
}
