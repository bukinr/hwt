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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sysexits.h>
#include <string.h>

#include "hwt.h"
#include "hwtvar.h"

#include "libpmcstat/libpmcstat.h"

int
hwt_process_start(int *sockpair)
{
	int error;

	error = write(sockpair[PARENTSOCKET], "!", 1);
	if (error != 1)
		return (-1);

	return (0);
}

int
hwt_process_create(int *sockpair, char **cmd, char **env, int *pid0)
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

struct pmcstat_process *
hwt_process_alloc(void)
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
