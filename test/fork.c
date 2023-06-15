#include <unistd.h>
#include <stdio.h>

int
main(void)
{
	pid_t pid;

	pid = fork();
	switch (pid) {
	case -1:
		break;
	case 0:
		printf("Hello World from child\n");
		return (0);
	}

	printf("Hello World\n");

	return (0);
}
