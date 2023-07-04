#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

static void *
test_thread(void *arg __unused)
{
	char *caps;
	int len;

	caps = malloc(128);
	sprintf(caps, "capabilities");

	len = strlen(caps);

	printf("Hello world from thread, strlen %d\n", len);

	return (NULL);
}

int
main(void)
{
	pthread_t thread;
	int error;

	error = strcmp("a", "b");
	printf("error %d\n", error);

	error = pthread_create(&thread, NULL, test_thread, NULL);
	printf("error %d\n", error);

	pthread_join(thread, NULL);

	return (0);
}
