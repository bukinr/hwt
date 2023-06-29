#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

static void *
test_thread(void *arg __unused)
{

	printf("Hello world from thread\n");

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
