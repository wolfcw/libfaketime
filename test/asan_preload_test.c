#include <stdio.h>
#include <time.h>

int
main(void)
{
	time_t now = time(NULL);
	struct tm broken_down;
	char output[32];

	if (localtime_r(&now, &broken_down) == NULL ||
	    strftime(output, sizeof(output), "%Y-%m-%d %H:%M:%S", &broken_down) == 0) {
		return 1;
	}
	puts(output);
	return 0;
}
