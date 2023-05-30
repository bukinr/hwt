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
#include "hwt_var.h"
#include "cs.h"

#include "libpmcstat/libpmcstat.h"

int
hwt_start_process(int *sockpair)
{
	int error;

	error = write(sockpair[PARENTSOCKET], "!", 1);
	if (error != 1)
		return (-1);

	return (0);
}

int
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

struct pmcstat_process *
hwt_process_create(pmcstat_interned_string path)
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
