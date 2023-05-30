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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "hwt.h"
#include "hwtvar.h"
#include "cs.h"

#define	PARENTSOCKET		0
#define	CHILDSOCKET		1
#define	NSOCKPAIRFD		2

struct pmcstat_image_hash_list pmcstat_image_hash[PMCSTAT_NHASH];
static struct trace_context tcs[4];

#include "libpmcstat/libpmcstat.h"

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

int
process_records(int fd, int pid)
{
	pmcstat_interned_string path;
	struct pmcstat_image *image;
	struct pmc_plugins plugins;
	struct pmcstat_process *pp;
	struct pmcstat_args args;
	unsigned long addr;
	struct hwt_record_get record_get;
	struct trace_context *tc;
	int nentries;
	int error;
	int j;
	int i;

	memset(&plugins, 0, sizeof(struct pmc_plugins));
	memset(&args, 0, sizeof(struct pmcstat_args));
	args.pa_fsroot = "/";
	nentries = 256;

	pp = hwt_process_alloc();

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

	error = hwt_start_process(sockpair);
	if (error != 0)
		return (error);

	printf("waiting proc to finish\n");

	sleep(1);

	process_records(fd, pid);

	close(fd);

	printf("processing\n");

	tc = &tcs[0];
	cs_init(tc);
	cs_process_chunk(tc);

	return (0);
}
