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
#include "hwt_coresight.h"

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

#if 0
	while (1)
		sleep(5);

	return (0);
#endif

	fd = open("/dev/hwt", O_RDWR);
	if (fd < 0) {
		printf("Can't open /dev/hwt\n");
		return (-1);
	}

	pp = hwt_process_alloc();

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

	printf("waiting proc to finish\n");

	sleep(1);

	for (i = 0; i < 4; i++) {
		tc = &tcs[i];
		hwt_record_fetch(tc);
	}

	close(fd);

	printf("processing\n");

	/* Coresight data is always on CPU0 due to funnelling by HW. */
	tc = &tcs[0];
	cs_init(tc);
	cs_process_chunk(tc);

	return (0);
}
