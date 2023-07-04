#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

static void *
test_thread(void *arg __unused)
{
	int len;

	len = strlen("capabilities");

	printf("Hello world from thread, strlen %d\n", len);

	return (NULL);
}

int
main(void)
{
	pthread_t thread;
	int error;

	error = pthread_create(&thread, NULL, test_thread, NULL);
	printf("error %d\n", error);

	pthread_join(thread, NULL);

	return (0);
}
