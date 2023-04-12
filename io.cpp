#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "time.h"


FILE *fh = fopen("input.dat", "a+");

bool verbose = false;

void send(const bool is_verbose, const char *fmt, ...)
{
	uint64_t now = get_ts_ms();
	time_t t_now = now / 1000;

	struct tm tm { 0 };
	if (!localtime_r(&t_now, &tm))
		fprintf(stderr, "localtime_r: %s\n", strerror(errno));

	char *ts_str = nullptr;
	asprintf(&ts_str, "%04d-%02d-%02d %02d:%02d:%02d.%03d [%d] ",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, int(now % 1000), getpid());

	char *str = nullptr;

	va_list ap;
	va_start(ap, fmt);
	(void)vasprintf(&str, fmt, ap);
	va_end(ap);
	fflush(fh);

	fprintf(fh, "%s%s\n", ts_str, str);

	if (is_verbose) {
		if (verbose)
			printf("%s\n", str);
	}
	else {
		printf("%s\n", str);
	}

	fflush(fh);

	free(str);
	free(ts_str);
}

void setVerbose(const bool v)
{
	verbose = v;
}

void closeLog()
{
	fclose(fh);

	fh = nullptr;
}
